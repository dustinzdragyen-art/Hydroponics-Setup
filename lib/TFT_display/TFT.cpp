// =============================================================================
// TFT.cpp — 3.2" ILI9341 display for the Hydroponics Controller
// =============================================================================
// Flicker-free via TFT_eSprite: every value is rendered into an off-screen
// buffer then pushed to the display in one atomic SPI transfer.
//
// ⚠ PIN CONFLICT WARNING:
//   GPIO 19 (MISO) and GPIO 23 (MOSI) are used for TFT SPI.
//   Micro pump moved: 19 → 12    pH Up pump moved: 23 → 14
//
// Backlight: tie screen LED to 3.3 V, or set TFT_BL_PIN to a free GPIO.
// Touch calibration runs on first boot and is stored in NVS automatically.
// =============================================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include "TFT.h"
#include "WebInterface.h"

#define TFT_BL_PIN  -1

// ── Externs from main.cpp ────────────────────────────────────────────────────
extern NutrientPump        pumps[];
extern const int           PUMP_COUNT;
extern unsigned long       pumpStopTimes[];
extern const unsigned long PUMP_AUTO_OFF_MS;
extern GrowRecipe          recipes[];
extern int                 numRecipes, activeRecipeIdx, currentWeek;
extern float               pumpWeekMl[], pumpTotalMl[];
extern Preferences         prefs;

// ── Colours (RGB565) ─────────────────────────────────────────────────────────
#define C_BG      0x0822
#define C_CARD    0x1946
#define C_BORDER  0x320A
#define C_ACCENT  0x3693
#define C_WHITE   0xE71D
#define C_GRAY    0x6B90
#define C_RED     0xF800
#define C_YELLOW  0xFFE0
#define C_ORANGE  0xFD20
#define C_PURPLE  0xA45F
#define C_DKGRN   0x0340

// ── Layout ───────────────────────────────────────────────────────────────────
#define SCR_W  320
#define SCR_H  240
#define HDR_H   22
#define NAV_H   26
#define BODY_Y  HDR_H
#define BODY_H  (SCR_H - HDR_H - NAV_H)
#define NAV_Y   (SCR_H - NAV_H)

#define PBW  148   // pump button width
#define PBH   42   // pump button height
#define PBG    6

// ── Sprite dimensions ────────────────────────────────────────────────────────
// One reusable sprite; sized to fit the largest single element (pump button).
#define SPR_W  PBW
#define SPR_H  PBH

// ── Objects & state ──────────────────────────────────────────────────────────
static TFT_eSPI    tft;
static TFT_eSprite spr = TFT_eSprite(&tft);

static int8_t        page      = 0;
static int8_t        drawnPage = -1;
static unsigned long lastDraw  = 0;
static unsigned long lastTouch = 0;

#define PAGES        3
#define REDRAW_MS  500
#define TOUCH_DBNC 300

// =============================================================================
// Sprite push helpers  (render off-screen → push atomically, zero flicker)
// =============================================================================

// Push a single value string centred in a sprite of given size at (px, py).
static void pushValue(int px, int py, int sw, int sh,
                      const char* buf, uint8_t font,
                      uint32_t fg, uint32_t bg) {
    spr.createSprite(sw, sh);
    spr.fillSprite(bg);
    spr.setTextDatum(MC_DATUM);
    spr.setTextFont(font);
    spr.setTextColor(fg, bg);
    spr.drawString(buf, sw / 2, sh / 2);
    spr.pushSprite(px, py);
    spr.deleteSprite();
}

// Push a complete pump button sprite at (px, py).
static void pushPumpBtn(int px, int py, int idx, unsigned long now) {
    bool     on = pumps[idx].state;
    uint32_t bg = on ? C_DKGRN  : C_CARD;
    uint32_t br = on ? C_ACCENT : C_BORDER;

    spr.createSprite(PBW, PBH);
    spr.fillSprite(bg);
    spr.drawRoundRect(0, 0, PBW, PBH, 6, br);

    spr.setTextDatum(ML_DATUM);
    spr.setTextFont(2);
    spr.setTextColor(C_WHITE, bg);
    spr.drawString(pumps[idx].name, 8, PBH / 2);

    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(on ? C_ACCENT : C_GRAY, bg);
    spr.drawString(on ? "ON" : "OFF", PBW - 6, PBH / 2);

    if (on && pumpStopTimes[idx] > now) {
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%us",
                 (unsigned)((pumpStopTimes[idx] - now) / 1000));
        spr.setTextFont(1);
        spr.setTextColor(C_ACCENT, bg);
        spr.setTextDatum(BR_DATUM);
        spr.drawString(tbuf, PBW - 4, PBH - 3);
    }

    spr.pushSprite(px, py);
    spr.deleteSprite();
}

// =============================================================================
// Shared chrome (drawn directly — solid fills so no blank-frame issue)
// =============================================================================

static void drawHeader(const char* title) {
    tft.fillRect(0, 0, SCR_W, HDR_H, C_CARD);
    tft.setTextColor(C_ACCENT, C_CARD);
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(1);
    tft.drawString(title, 5, HDR_H / 2);

    int bars = 0;
    if (WiFi.status() == WL_CONNECTED) {
        int r = WiFi.RSSI();
        bars = (r > -55) ? 4 : (r > -65) ? 3 : (r > -75) ? 2 : 1;
    }
    for (int i = 0; i < 4; i++) {
        int bh = 4 + i * 3;
        tft.fillRect(SCR_W - 26 + i * 6, HDR_H - 2 - bh, 4, bh,
                     i < bars ? C_ACCENT : C_BORDER);
    }
}

static void drawNavBar() {
    tft.fillRect(0, NAV_Y, SCR_W, NAV_H, C_CARD);
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(page > 0         ? C_WHITE : C_BORDER, C_CARD);
    tft.drawString("<", 10, NAV_Y + NAV_H / 2);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(page < PAGES - 1 ? C_WHITE : C_BORDER, C_CARD);
    tft.drawString(">", SCR_W - 10, NAV_Y + NAV_H / 2);
    for (int i = 0; i < PAGES; i++) {
        int cx = SCR_W / 2 - (PAGES - 1) * 8 + i * 16;
        tft.fillCircle(cx, NAV_Y + NAV_H / 2, 4,
                       i == page ? C_ACCENT : C_BORDER);
    }
}

// =============================================================================
// Page 0 — Sensors
// =============================================================================

static const int SC_CW = 154, SC_CH = 92, SC_GAP = 4;
static int scX(int i) { return SC_GAP + (i & 1) * (SC_CW + SC_GAP); }
static int scY(int i) { return BODY_Y + SC_GAP + (i >> 1) * (SC_CH + SC_GAP); }

// Value sprite fits inside a card — 100 wide × 36 tall, centred vertically
#define SV_W  100
#define SV_H   36

static void initSensors() {
    tft.fillRect(0, BODY_Y, SCR_W, BODY_H, C_BG);
    drawHeader("SENSORS");
    drawNavBar();

    const char* labels[4] = { "pH", "TDS", "Temp", "Humidity" };
    const char* units[4]  = { "",  "ppm", "F",    "%" };

    for (int i = 0; i < 4; i++) {
        int cx = scX(i), cy = scY(i);
        tft.fillRoundRect(cx, cy, SC_CW, SC_CH, 6, C_CARD);
        tft.setTextFont(1);
        tft.setTextColor(C_GRAY, C_CARD);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(labels[i], cx + 6, cy + 5);
        if (units[i][0]) {
            tft.setTextDatum(BR_DATUM);
            tft.drawString(units[i], cx + SC_CW - 5, cy + SC_CH - 4);
        }
    }
}

static void updateSensors() {
    float vals[4] = {
        readPH(),
        readEC(),
        readTemp() * 9.0f / 5.0f + 32.0f,
        readHumidity()
    };
    uint32_t cols[4] = { C_ACCENT, C_YELLOW, C_ORANGE, C_PURPLE };

    for (int i = 0; i < 4; i++) {
        bool     alert = (i == 0) && vals[0] > 0.0f &&
                         (vals[0] < 5.5f || vals[0] > 6.5f);
        uint32_t bg    = alert ? 0x2800 : C_CARD;
        uint32_t fg    = alert ? C_RED  : cols[i];
        int      cx    = scX(i), cy = scY(i);

        char buf[12];
        if      (i == 0) snprintf(buf, sizeof(buf), "%.2f", vals[i]);
        else if (i == 1) snprintf(buf, sizeof(buf), "%.0f",  vals[i]);
        else             snprintf(buf, sizeof(buf), "%.1f",  vals[i]);

        // Value sprite centred in the card body (below label, above unit)
        int px = cx + SC_CW / 2 - SV_W / 2;
        int py = cy + SC_CH / 2 - SV_H / 2 + 4;
        pushValue(px, py, SV_W, SV_H, buf, 4, fg, bg);

        // Update alert border without clearing card
        tft.drawRoundRect(cx, cy, SC_CW, SC_CH, 6, alert ? C_RED : C_CARD);
    }
}

// =============================================================================
// Page 1 — Recipe & Usage
// =============================================================================

// Recipe value sprite: fits inside a table cell
#define RV_W  66
#define RV_H  24

static void initRecipe() {
    tft.fillRect(0, BODY_Y, SCR_W, BODY_H, C_BG);
    drawNavBar();   // header drawn each update (contains week number)

    int y = BODY_Y + 4;
    tft.setTextFont(1);
    tft.setTextColor(C_GRAY, C_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Nutrient", 6,   y + 5);
    tft.drawString("Target",  112,  y + 5);
    tft.drawString("This Wk", 182,  y + 5);
    tft.drawString("All Time",252,  y + 5);
    tft.drawLine(0, y + 16, SCR_W, y + 16, C_BORDER);

    // Row backgrounds and static nutrient labels
    const char* names[3] = { "Micro", "Gro", "Bloom" };
    for (int i = 0; i < 3; i++) {
        int      ry = y + 20 + i * 34;
        uint32_t bg = (i & 1) ? C_BG : C_CARD;
        tft.fillRect(0, ry, SCR_W, 32, bg);
        tft.setTextFont(2);
        tft.setTextColor(C_WHITE, bg);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(names[i], 6, ry + 16);
    }
}

static void updateRecipe() {
    int maxWk = recipes[activeRecipeIdx].numWeeks;
    int wk    = constrain(currentWeek, 1, maxWk);
    WeekDose  d = recipes[activeRecipeIdx].weeks[wk - 1];

    char hdr[40];
    snprintf(hdr, sizeof(hdr), "%.18s  Wk %d/%d",
             recipes[activeRecipeIdx].name, wk, maxWk);
    drawHeader(hdr);

    int    baseY = BODY_Y + 4;
    float  targets[3] = { d.micro, d.gro, d.bloom };

    for (int i = 0; i < 3; i++) {
        int      ry = baseY + 20 + i * 34;
        uint32_t bg = (i & 1) ? C_BG : C_CARD;
        char buf[14];

        snprintf(buf, sizeof(buf), "%.1f",   targets[i]);
        pushValue(112, ry + 4, RV_W, RV_H, buf, 2, C_ACCENT, bg);

        snprintf(buf, sizeof(buf), "%.1fml", pumpWeekMl[i]);
        pushValue(182, ry + 4, RV_W, RV_H, buf, 2, C_YELLOW, bg);

        snprintf(buf, sizeof(buf), "%.0fml", pumpTotalMl[i]);
        pushValue(252, ry + 4, RV_W, RV_H, buf, 2, C_GRAY,   bg);
    }

    // pH and footer — small text, direct draw is fine
    int py = baseY + 20 + 3 * 34 + 6;
    tft.fillRect(0, py, SCR_W, 36, C_BG);
    tft.setTextFont(1); tft.setTextColor(C_GRAY, C_BG); tft.setTextDatum(ML_DATUM);
    char buf[48];
    snprintf(buf, sizeof(buf), "pH Up: %.1f ml    pH Down: %.1f ml",
             pumpWeekMl[3], pumpWeekMl[4]);
    tft.drawString(buf, 6, py + 5);
    snprintf(buf, sizeof(buf), "Recipe %d of %d", activeRecipeIdx + 1, numRecipes);
    tft.setTextColor(C_BORDER, C_BG);
    tft.drawString(buf, 6, py + 22);
}

// =============================================================================
// Page 2 — Pump Controls
// =============================================================================

static int pumpBtnX(int i) {
    if (i == 4) return (SCR_W - PBW) / 2;
    return (i & 1) ? (SCR_W / 2 + PBG / 2) : (SCR_W / 2 - PBW - PBG / 2);
}
static int pumpBtnY(int i) {
    int row = (i == 4) ? 2 : (i >> 1);
    return BODY_Y + 8 + row * (PBH + PBG);
}

static void initPumps() {
    tft.fillRect(0, BODY_Y, SCR_W, BODY_H, C_BG);
    drawHeader("PUMP CONTROLS");
    drawNavBar();
}

static void updatePumps() {
    unsigned long now = millis();
    for (int i = 0; i < PUMP_COUNT; i++)
        pushPumpBtn(pumpBtnX(i), pumpBtnY(i), i, now);
}

// =============================================================================
// Touch handling
// =============================================================================

static bool handleNavTouch(uint16_t tx, uint16_t ty) {
    if (ty < (uint16_t)NAV_Y) return false;
    if      (tx < 40         && page > 0)         { page--; drawnPage = -1; }
    else if (tx > SCR_W - 40 && page < PAGES - 1) { page++; drawnPage = -1; }
    return true;
}

static void handlePumpTouch(uint16_t tx, uint16_t ty) {
    for (int i = 0; i < PUMP_COUNT; i++) {
        int px = pumpBtnX(i), py = pumpBtnY(i);
        if (tx >= (uint16_t)px && tx < (uint16_t)(px + PBW) &&
            ty >= (uint16_t)py && ty < (uint16_t)(py + PBH)) {
            if (pumps[i].state) {
                recordPumpStop(i);
            } else {
                pumps[i].state         = true;
                pumps[i].onStartMillis = millis();
                digitalWrite(pumps[i].pin, HIGH);
                pumpStopTimes[i]       = millis() + PUMP_AUTO_OFF_MS;
            }
            break;
        }
    }
}

// =============================================================================
// Public API
// =============================================================================

void setupTFT() {
    if (TFT_BL_PIN >= 0) { pinMode(TFT_BL_PIN, OUTPUT); digitalWrite(TFT_BL_PIN, HIGH); }

    tft.init();
    tft.setRotation(1);

    uint16_t cal[5] = {0, 0, 0, 0, 0};
    prefs.begin("hydro", true);
    bool hasCal = prefs.getBool("tft_cal_ok", false);
    if (hasCal)
        for (int i = 0; i < 5; i++)
            cal[i] = prefs.getUShort(("tft_c" + String(i)).c_str(), 0);
    prefs.end();

    if (!hasCal) {
        tft.fillScreen(TFT_BLACK);
        tft.calibrateTouch(cal, TFT_GREEN, TFT_BLACK, 12);
        prefs.begin("hydro", false);
        prefs.putBool("tft_cal_ok", true);
        for (int i = 0; i < 5; i++)
            prefs.putUShort(("tft_c" + String(i)).c_str(), cal[i]);
        prefs.end();
    }
    tft.setTouch(cal);

    tft.fillScreen(C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_ACCENT, C_BG); tft.setTextFont(4);
    tft.drawString("HYDRO", SCR_W / 2, SCR_H / 2 - 18);
    tft.setTextColor(C_GRAY,   C_BG); tft.setTextFont(2);
    tft.drawString("CONTROLLER", SCR_W / 2, SCR_H / 2 + 12);
    delay(1200);

    tft.fillScreen(C_BG);
    drawnPage = -1;
}

void updateTFT() {
    unsigned long now = millis();

    uint16_t tx, ty;
    if (now - lastTouch > TOUCH_DBNC && tft.getTouch(&tx, &ty)) {
        lastTouch = now;
        if (!handleNavTouch(tx, ty) && page == 2)
            handlePumpTouch(tx, ty);
    }

    if (now - lastDraw < REDRAW_MS) return;
    lastDraw = now;

    // Full layout once on page change
    if (drawnPage != page) {
        drawnPage = page;
        switch (page) {
            case 0: initSensors(); break;
            case 1: initRecipe();  break;
            case 2: initPumps();   break;
        }
    }

    // Sprite-push only the changing values
    switch (page) {
        case 0: updateSensors(); break;
        case 1: updateRecipe();  break;
        case 2: updatePumps();   break;
    }
}
