#pragma once
#include <WebServer.h>

// ------------------- Shared Types -------------------
struct NutrientPump {
    const char* name;
    uint8_t pin;
    bool state;
    unsigned long onStartMillis;
};

struct Recipe { float micro, gro, bloom, phUp, phDown; };

void recordPumpStop(int i);

void setupWebInterface(WebServer &server);

float readPH();
float readEC();
float readTemp();
float readHumidity();