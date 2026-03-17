#pragma once
#include <WebServer.h>

// ------------------- Shared Types -------------------
struct NutrientPump {
    const char* name;
    uint8_t pin;
    bool state;
    unsigned long onStartMillis;
};

struct WeekDose  { float micro, gro, bloom; };
struct GrowRecipe { char name[32]; int numWeeks; WeekDose weeks[16]; };

void recordPumpStop(int i);
void saveRecipeToNVS(int idx);
int  rawPHReading();
int  rawECReading();

void setupWebInterface(WebServer &server);

float readPH();
float readEC();
float readTemp();
float readHumidity();