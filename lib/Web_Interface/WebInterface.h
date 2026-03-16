#pragma once
#include <WebServer.h>

// ------------------- Shared Types -------------------
struct NutrientPump {
    const char* name;
    uint8_t pin;
    bool state;
    unsigned long onStartMillis;
};

void setupWebInterface(WebServer &server);

float readPH();
float readEC();
float readTemp();
float readHumidity();