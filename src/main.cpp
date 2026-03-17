#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include <Preferences.h>
#include "WebInterface.h"
#include "config.h"

// ------------------- WiFi -------------------
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ------------------- Server -------------------
WebServer server(80);

// ------------------- Pumps -------------------
NutrientPump pumps[]={{"Micro",19,false,0},{"Gro",21,false,0},{"Bloom",22,false,0},{"pH Up",23,false,0},{"pH Down",25,false,0}};
extern const int PUMP_COUNT = sizeof(pumps)/sizeof(pumps[0]);
unsigned long pumpStopTimes[PUMP_COUNT] = {0};
extern const unsigned long PUMP_AUTO_OFF_MS=30000;

// ------------------- Sensors -------------------
#define PH_PIN       34
#define EC_PIN       35
#define TEMP_PIN     32
#define HUMIDITY_PIN 27     // DHT22 data pin
#define DHTTYPE      DHT22  // change to DHT11 if using that sensor

DHT dht(HUMIDITY_PIN, DHTTYPE);

// Oversample the ADC to reduce noise from relay switching and supply fluctuations
static int adcAverage(uint8_t pin, int samples = 16) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(200); // brief gap between samples
    }
    return sum / samples;
}

// Calibration offsets and multipliers — adjustable via /calibrate endpoint
float phOffset=0.0f, phMult=1.0f;
float ecOffset=0.0f, ecMult=1.0f;

// ------------------- Persistent Storage -------------------
Preferences prefs;

// ------------------- Solution Tracking -------------------
float pumpFlowRateMlPerSec = 1.0f;
float pumpTotalMl[5] = {0};  // lifetime ml per pump, saved to NVS
float pumpWeekMl[5]  = {0};  // this-week ml per pump, saved to NVS
int   lastSavedWeek  = 0;

float readPH() {
    int r = adcAverage(PH_PIN);
    float raw = r * (3.3f / 4095.0f) * 3.5f;
    return (raw + phOffset) * phMult;
}
float readEC() {
    int r = adcAverage(EC_PIN);
    float raw = r * (3.3f / 4095.0f) * 2.0f;
    return (raw + ecOffset) * ecMult;
}
float readHumidity() { float h=dht.readHumidity();    return isnan(h) ? -1.0f : h; }
float readTemp() {
    // NTC 10K thermistor with 10K pull-up resistor to 3.3V — adjust beta (3950) for your sensor
    int r = adcAverage(TEMP_PIN);
    float v   = r * (3.3f / 4095.0f);
    float res = 10000.0f * v / (3.3f - v);
    float tK  = 1.0f / (logf(res / 10000.0f) / 3950.0f + 1.0f / 298.15f);
    return tK - 273.15f;
}

// ------------------- History -------------------
extern const int HISTORY_SIZE=1440; // 6 hours @ 15-sec intervals
float phHistory[HISTORY_SIZE]={0}, ecHistory[HISTORY_SIZE]={0}, tempHistory[HISTORY_SIZE]={0}, humidityHistory[HISTORY_SIZE]={0};
float microHistory[HISTORY_SIZE]={0}, groHistory[HISTORY_SIZE]={0}, bloomHistory[HISTORY_SIZE]={0};
int historyIndex=0;
int historyCount=0; // how many slots have been written (caps at HISTORY_SIZE)
int currentWeek=1;
int weekOffset=0;  // manual UI adjustment on top of NTP-computed week

// ------------------- Growth Stage Recipes -------------------
Recipe weekRecipes[12] = {
    {5,3,1,0,0},{5,3,2,0,0},{6,4,2,0,0},{6,4,3,0,0}, // weeks 1-4
    {7,5,4,0,0},{8,5,4,0,0},{8,6,4,0,0},{9,6,5,0,0}, // weeks 5-8
    {10,7,5,0,0},{10,7,6,0,0},{11,8,6,0,0},{12,8,7,0,0} // weeks 9-12
};

// ------------------- Pump Stop Helper -------------------
void recordPumpStop(int i) {
    if (pumps[i].onStartMillis > 0) {
        float ml = ((millis() - pumps[i].onStartMillis) / 1000.0f) * pumpFlowRateMlPerSec;
        pumpTotalMl[i] += ml;
        pumpWeekMl[i]  += ml;
        prefs.begin("hydro", false);
        prefs.putFloat(("tot_" + String(i)).c_str(), pumpTotalMl[i]);
        prefs.putFloat(("wk_"  + String(i)).c_str(), pumpWeekMl[i]);
        prefs.end();
    }
    pumps[i].state         = false;
    pumps[i].onStartMillis = 0;
    digitalWrite(pumps[i].pin, LOW);
    pumpStopTimes[i]       = 0;
}

// ------------------- Setup -------------------
void setup(){
    Serial.begin(115200);
    WiFi.begin(ssid,password);
    Serial.print("Connecting WiFi...");
    while(WiFi.status()!=WL_CONNECTED){delay(500); Serial.print(".");}
    Serial.print("\nConnected! IP: "); Serial.println(WiFi.localIP());

    // Sync time via NTP for accurate week tracking across reboots
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing NTP...");
    for(int i=0; i<20 && time(nullptr)<100000; i++){delay(500); Serial.print(".");}
    Serial.println(time(nullptr)>100000 ? " OK" : " FAILED");

    dht.begin();
    prefs.begin("hydro", false);
    pumpFlowRateMlPerSec = prefs.getFloat("flow_rate", 1.0f);
    lastSavedWeek        = prefs.getInt("last_week", 0);
    for(int i=0;i<PUMP_COUNT;i++){
        pumpTotalMl[i] = prefs.getFloat(("tot_"+String(i)).c_str(), 0.0f);
        pumpWeekMl[i]  = prefs.getFloat(("wk_" +String(i)).c_str(), 0.0f);
    }
    prefs.end();
    for(int i=0;i<PUMP_COUNT;i++){pinMode(pumps[i].pin,OUTPUT); digitalWrite(pumps[i].pin,LOW);}
    setupWebInterface(server);  // all routes registered in WebInterface.cpp
    server.begin();
    Serial.println("HTTP server started");
}

// ------------------- Loop -------------------
void loop(){
    server.handleClient();
    unsigned long now=millis();
    for(int i=0;i<PUMP_COUNT;i++){
        if(pumps[i].state && pumpStopTimes[i]>0 && now>=pumpStopTimes[i]){
            recordPumpStop(i);
        }
    }

    // ------------------- Record History -------------------
    static unsigned long lastRecord=0;
    if(millis()-lastRecord>15000){ // every 15 seconds
        lastRecord=millis();
        phHistory[historyIndex]=readPH();
        ecHistory[historyIndex]=readEC();
        tempHistory[historyIndex]=readTemp();
        humidityHistory[historyIndex]=readHumidity();
        microHistory[historyIndex]=pumps[0].state?10:0;
        groHistory[historyIndex]=pumps[1].state?10:0;
        bloomHistory[historyIndex]=pumps[2].state?10:0;
        historyIndex=(historyIndex+1)%HISTORY_SIZE;
        if(historyCount < HISTORY_SIZE) historyCount++;
    }

    // ------------------- Growth Stage Automation -------------------
    // Throttled: only evaluate every 2 minutes. Only one pump may run at a time.
    static unsigned long lastAutoCheck = 0;
    if(now - lastAutoCheck >= 120000){
        lastAutoCheck = now;

        // Update week from NTP (survives reboots). weekOffset allows manual UI correction.
        time_t now_t = time(nullptr);
        if(now_t > 100000){
            int ntpWeek = (int)((now_t - (time_t)GROW_START_EPOCH) / (7L * 86400L)) + 1 + weekOffset;
            currentWeek = constrain(ntpWeek, 1, 12);
        }

        // Reset weekly ml totals when the week advances
        if(lastSavedWeek != 0 && currentWeek != lastSavedWeek){
            for(int i=0;i<PUMP_COUNT;i++) pumpWeekMl[i]=0;
            prefs.begin("hydro",false);
            for(int i=0;i<PUMP_COUNT;i++) prefs.putFloat(("wk_"+String(i)).c_str(),0.0f);
            prefs.putInt("last_week", currentWeek);
            prefs.end();
        }
        lastSavedWeek = currentWeek;

        // Skip if any pump is already running
        bool anyOn = false;
        for(int i = 0; i < PUMP_COUNT; i++) if(pumps[i].state) { anyOn = true; break; }
        if(anyOn) return;

        // Helper lambda: turn on one pump and return
        auto startPump = [&](int i){
            pumps[i].state = true;
            digitalWrite(pumps[i].pin, HIGH);
            pumpStopTimes[i] = now + PUMP_AUTO_OFF_MS;
        };

        int week = constrain(currentWeek, 1, 12);
        Recipe current = weekRecipes[week - 1];

        // pH correction takes priority over nutrients
        float ph = readPH();
        if(ph < 5.5){ startPump(3); return; }  // pH Up
        if(ph > 6.5){ startPump(4); return; }  // pH Down

        // Auto dose nutrients — activate the first one that needs it
        float ec = readEC();
        if(ec < current.micro){ startPump(0); return; }
        if(ec < current.gro  ){ startPump(1); return; }
        if(ec < current.bloom){ startPump(2); return; }
    }
}