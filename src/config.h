#pragma once

// ------------------- WiFi Credentials -------------------
// Update these to match your network before flashing
#define WIFI_SSID     "Avalon"
#define WIFI_PASSWORD "jkjmsl998084"

// ------------------- Web Auth -------------------
#define WEB_USER "admin"
#define WEB_PASS "hydro1234"

// ------------------- Grow Start Date -------------------
// Unix timestamp of day 1 of your grow (used for NTP-based week tracking).
// Get yours at: https://www.unixtimestamp.com
#define GROW_START_EPOCH 1710000000UL  // example: March 9 2024 — update before flashing
