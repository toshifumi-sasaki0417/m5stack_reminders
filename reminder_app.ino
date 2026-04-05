#include <M5Unified.h>
#include <WiFi.h>
#include "time.h"

// ---- WiFi / NTP settings ----
const char* ssid      = "YOUR_WIFI_SSID";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset = 9 * 3600;
const int   dstOffset = 0;
const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// ---- Reminder data ----
#define MAX_REMINDERS 5
#define NUM_LABELS    7
const char* labels[NUM_LABELS] = {
  "Medicine", "Meeting", "Water", "Exercise", "Wake Up", "Sleep", "Custom"
};

struct Reminder {
  int  hour;
  int  minute;
  int  labelIdx;
  bool active;
  bool triggered;
};

Reminder reminders[MAX_REMINDERS];
int reminderCount = 0;

// ---- App state ----
// 0=Home  1=Add Reminder  2=Alert
int  currentMode  = 0;
int  editHour     = 8;
int  editMinute   = 0;
int  editLabelIdx = 0;
int  alertIdx     = -1;

M5Canvas sprite(&M5.Lcd);
unsigned long lastClockUpdate = 0;

// ===================== Draw helpers =====================

void drawDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  sprite.fillSprite(0x2104);  // dark grey
  sprite.setTextSize(2);
  sprite.setTextColor(WHITE);
  sprite.setCursor(8, 10);
  sprite.printf("%02d/%02d %s  %02d:%02d:%02d",
    timeinfo.tm_mon + 1, timeinfo.tm_mday,
    weekdays[timeinfo.tm_wday],
    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  sprite.pushSprite(0, 0);
}

void drawHome() {
  M5.Lcd.fillScreen(BLACK);
  drawDateTime();

  // Reminder list
  if (reminderCount == 0) {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(0x4208);  // dim grey
    M5.Lcd.setCursor(50, 105);
    M5.Lcd.println("No reminders set.");
    M5.Lcd.setCursor(60, 130);
    M5.Lcd.println("Tap [+ ADD] below.");
  } else {
    for (int i = 0; i < reminderCount; i++) {
      int y = 50 + i * 32;
      uint16_t col = reminders[i].active ? WHITE : 0x4208;
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(col);
      M5.Lcd.setCursor(10, y);
      M5.Lcd.printf("%02d:%02d  %s", reminders[i].hour, reminders[i].minute, labels[reminders[i].labelIdx]);
      // status dot
      uint16_t dot = reminders[i].triggered ? 0x4208 : (reminders[i].active ? GREEN : 0x4208);
      M5.Lcd.fillCircle(305, y + 8, 6, dot);
    }
  }

  // Bottom buttons
  uint16_t addColor = (reminderCount < MAX_REMINDERS) ? 0x0460 : 0x2104;
  M5.Lcd.fillRoundRect(10, 208, 140, 28, 6, addColor);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor(35, 215);
  M5.Lcd.println("+ ADD");

  uint16_t clrColor = (reminderCount > 0) ? 0xA000 : 0x2104;
  M5.Lcd.fillRoundRect(160, 208, 150, 28, 6, clrColor);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(175, 215);
  M5.Lcd.println("CLEAR ALL");
}

void drawAddReminder() {
  M5.Lcd.fillScreen(BLACK);

  // Header
  M5.Lcd.fillRect(0, 0, 320, 38, 0x2104);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(75, 10);
  M5.Lcd.println("Set Reminder");

  // ---- Hour ----
  M5.Lcd.setTextColor(0x8410);
  M5.Lcd.setCursor(22, 50);
  M5.Lcd.println("HOUR");

  M5.Lcd.fillRoundRect(10, 70, 44, 40, 6, NAVY);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(22, 79);
  M5.Lcd.println("-");

  M5.Lcd.setTextSize(4);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(65, 68);
  M5.Lcd.printf("%02d", editHour);

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillRoundRect(126, 70, 44, 40, 6, NAVY);
  M5.Lcd.setCursor(138, 79);
  M5.Lcd.println("+");

  // ---- Minute ----
  M5.Lcd.setTextColor(0x8410);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(182, 50);
  M5.Lcd.println("MIN");

  M5.Lcd.fillRoundRect(170, 70, 44, 40, 6, NAVY);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(182, 79);
  M5.Lcd.println("-");

  M5.Lcd.setTextSize(4);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(224, 68);
  M5.Lcd.printf("%02d", editMinute);

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.fillRoundRect(278, 70, 44, 40, 6, NAVY);
  M5.Lcd.setCursor(288, 79);
  M5.Lcd.println("+");

  // ---- Label ----
  M5.Lcd.setTextColor(0x8410);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 128);
  M5.Lcd.println("Label:");

  M5.Lcd.fillRoundRect(10, 150, 44, 32, 6, NAVY);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(20, 157);
  M5.Lcd.println("<");

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(65, 158);
  M5.Lcd.printf("%-10s", labels[editLabelIdx]);

  M5.Lcd.fillRoundRect(266, 150, 44, 32, 6, NAVY);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(276, 157);
  M5.Lcd.println(">");

  // ---- Buttons ----
  M5.Lcd.fillRoundRect(10, 200, 130, 34, 6, 0x0460);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor(42, 210);
  M5.Lcd.println("SAVE");

  M5.Lcd.fillRoundRect(180, 200, 130, 34, 6, 0xA000);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(198, 210);
  M5.Lcd.println("CANCEL");
}

void drawAlert(int idx) {
  M5.Lcd.fillScreen(0x2000);  // dark red

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(60, 35);
  M5.Lcd.println("! REMINDER !");

  M5.Lcd.setTextSize(5);
  M5.Lcd.setTextColor(WHITE);
  int tw = String(reminders[idx].hour < 10 ? "0" : "").length();
  M5.Lcd.setCursor(60, 85);
  M5.Lcd.printf("%02d:%02d", reminders[idx].hour, reminders[idx].minute);

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(60, 150);
  M5.Lcd.println(labels[reminders[idx].labelIdx]);

  M5.Lcd.fillRoundRect(85, 198, 150, 36, 8, GREEN);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor(110, 209);
  M5.Lcd.println("DISMISS");
}

// ===================== Reminder check =====================

void checkReminders() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  for (int i = 0; i < reminderCount; i++) {
    if (reminders[i].active &&
        !reminders[i].triggered &&
        reminders[i].hour   == timeinfo.tm_hour &&
        reminders[i].minute == timeinfo.tm_min) {
      reminders[i].triggered = true;
      alertIdx = i;
      currentMode = 2;
      drawAlert(i);
      // Beep alert
      for (int b = 0; b < 3; b++) {
        M5.Speaker.tone(1000, 200);
        delay(300);
      }
      return;
    }
  }
}

// ===================== Setup / Loop =====================

void setup() {
  auto cfg = M5.config();
  cfg.internal_rtc = true;
  M5.begin(cfg);
  Serial.begin(115200);

  sprite.createSprite(320, 38);

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset, dstOffset, ntpServer);
    struct tm timeinfo;
    int ntpRetry = 0;
    while (!getLocalTime(&timeinfo) && ntpRetry < 10) {
      delay(500);
      ntpRetry++;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  drawHome();
}

void loop() {
  M5.update();

  // Refresh clock + check reminders every second on Home screen
  if (currentMode == 0 && millis() - lastClockUpdate > 1000) {
    lastClockUpdate = millis();
    drawDateTime();
    checkReminders();
  }

  // Touch handling
  if (M5.Touch.getCount() > 0) {
    auto t = M5.Touch.getDetail(0);
    if (t.wasReleased()) {
      int tx = t.x;
      int ty = t.y;

      // ---- Home ----
      if (currentMode == 0) {
        if (ty >= 208) {
          if (tx <= 150 && reminderCount < MAX_REMINDERS) {
            // + ADD
            editHour = 8; editMinute = 0; editLabelIdx = 0;
            currentMode = 1;
            drawAddReminder();
          } else if (tx >= 160 && reminderCount > 0) {
            // CLEAR ALL
            reminderCount = 0;
            drawHome();
          }
        }
      }

      // ---- Add Reminder ----
      else if (currentMode == 1) {
        // Hour -
        if (tx >= 10 && tx <= 54 && ty >= 70 && ty <= 110) {
          editHour = (editHour + 23) % 24;
          drawAddReminder();
        }
        // Hour +
        if (tx >= 126 && tx <= 170 && ty >= 70 && ty <= 110) {
          editHour = (editHour + 1) % 24;
          drawAddReminder();
        }
        // Minute -
        if (tx >= 170 && tx <= 214 && ty >= 70 && ty <= 110) {
          editMinute = (editMinute + 59) % 60;
          drawAddReminder();
        }
        // Minute +
        if (tx >= 278 && tx <= 322 && ty >= 70 && ty <= 110) {
          editMinute = (editMinute + 1) % 60;
          drawAddReminder();
        }
        // Label <
        if (tx >= 10 && tx <= 54 && ty >= 150 && ty <= 182) {
          editLabelIdx = (editLabelIdx + NUM_LABELS - 1) % NUM_LABELS;
          drawAddReminder();
        }
        // Label >
        if (tx >= 266 && tx <= 310 && ty >= 150 && ty <= 182) {
          editLabelIdx = (editLabelIdx + 1) % NUM_LABELS;
          drawAddReminder();
        }
        // SAVE
        if (tx >= 10 && tx <= 140 && ty >= 200) {
          reminders[reminderCount] = {editHour, editMinute, editLabelIdx, true, false};
          reminderCount++;
          currentMode = 0;
          drawHome();
        }
        // CANCEL
        if (tx >= 180 && ty >= 200) {
          currentMode = 0;
          drawHome();
        }
      }

      // ---- Alert ----
      else if (currentMode == 2) {
        // DISMISS
        if (tx >= 85 && tx <= 235 && ty >= 198) {
          reminders[alertIdx].active = false;
          currentMode = 0;
          drawHome();
        }
      }
    }
  }
}
