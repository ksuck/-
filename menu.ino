#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences prefs;

#define SDA_PIN 21
#define SCL_PIN 22

#define RELAY_PIN 25
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define BTN_MENU 13
#define BTN_UP   14
#define BTN_DOWN 27
#define BTN_OK   26

#define BATTERY_PIN 34
#define BATTERY_MIN 6.0
#define BATTERY_MAX 8.4


enum ScreenState {
  SCREEN_HOME,
  SCREEN_MODE,
  SCREEN_CUSTOM_MIST,
  SCREEN_CUSTOM_REST,
  SCREEN_SAVE_CONFIRM
};

ScreenState screen = SCREEN_HOME;

int homeSelect = 0;   // 0 START/STOP, 1 MODE
int modeSelect = 1;   // 1-4

bool running = false;
bool mistOn = false;

unsigned long lastChange = 0;
unsigned long mistTime = 10000;
unsigned long restTime = 300000;

int selectedMode = 1;

int customMistDigit[4] = {0, 0, 1, 0}; // 00:10
int customRestDigit[4] = {0, 5, 0, 0}; // 05:00
int editIndex = 0;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED fail");
    while (true);
  }

  prefs.begin("mist", false);
  selectedMode = prefs.getInt("mode", 1);

  loadCustomTime();
  setModeTime();

  drawScreen();
}

void loop() {
  readButtons();

  if (running) {
    runCycle();
  } else {
    mistOn = false;
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }

  drawScreen();
  delay(120);
}

bool pressed(int pin) {
  static unsigned long lastPressTime[40] = {0};

  if (digitalRead(pin) == LOW) {
    if (millis() - lastPressTime[pin] > 80) {
      lastPressTime[pin] = millis();

      while (digitalRead(pin) == LOW) {
        delay(1);
      }

      return true;
    }
  }

  return false;
}

void readButtons() {
  if (screen == SCREEN_HOME) {
    if (pressed(BTN_UP) || pressed(BTN_DOWN)) {
      homeSelect = !homeSelect;
    }

    if (pressed(BTN_OK)) {
      if (homeSelect == 0) {
        running = !running;
        if (running) {
          mistOn = true;
          lastChange = millis();
          digitalWrite(RELAY_PIN, RELAY_ON);
        } else {
          mistOn = false;
          digitalWrite(RELAY_PIN, RELAY_OFF);
        }
      } else {
        screen = SCREEN_MODE;
      }
    }
  }

  else if (screen == SCREEN_MODE) {
    if (pressed(BTN_UP)) {
      modeSelect--;
      if (modeSelect < 1) modeSelect = 4;
    }

    if (pressed(BTN_DOWN)) {
      modeSelect++;
      if (modeSelect > 4) modeSelect = 1;
    }

    if (pressed(BTN_OK)) {
      selectedMode = modeSelect;

      if (selectedMode == 4) {
        editIndex = 0;
        screen = SCREEN_CUSTOM_MIST;
      } else {
        prefs.putInt("mode", selectedMode);
        setModeTime();
        screen = SCREEN_HOME;
      }
    }

    if (pressed(BTN_MENU)) {
      screen = SCREEN_HOME;
    }
  }

  else if (screen == SCREEN_CUSTOM_MIST) {
    editDigits(customMistDigit);

    if (pressed(BTN_OK)) {
      editIndex++;
      if (editIndex > 3) {
        editIndex = 0;
        screen = SCREEN_CUSTOM_REST;
      }
    }

    if (pressed(BTN_MENU)) {
      screen = SCREEN_MODE;
    }
  }

  else if (screen == SCREEN_CUSTOM_REST) {
    editDigits(customRestDigit);

    if (pressed(BTN_OK)) {
      editIndex++;
      if (editIndex > 3) {
        editIndex = 0;
        screen = SCREEN_SAVE_CONFIRM;
      }
    }

    if (pressed(BTN_MENU)) {
      screen = SCREEN_CUSTOM_MIST;
    }
  }

  else if (screen == SCREEN_SAVE_CONFIRM) {
    if (pressed(BTN_OK)) {
      saveCustomTime();
      selectedMode = 4;
      prefs.putInt("mode", selectedMode);
      setModeTime();
      screen = SCREEN_HOME;
    }

    if (pressed(BTN_MENU)) {
      screen = SCREEN_CUSTOM_REST;
    }
  }
}

void editDigits(int digitArray[4]) {
  if (pressed(BTN_UP)) {
    digitArray[editIndex]++;
    if (digitArray[editIndex] > 9) digitArray[editIndex] = 0;
  }

  if (pressed(BTN_DOWN)) {
    digitArray[editIndex]--;
    if (digitArray[editIndex] < 0) digitArray[editIndex] = 9;
  }
}

void loadCustomTime() {
  int mistMin = prefs.getInt("mistMin", 0);
  int mistSec = prefs.getInt("mistSec", 10);
  int restMin = prefs.getInt("restMin", 5);
  int restSec = prefs.getInt("restSec", 0);

  customMistDigit[0] = mistMin / 10;
  customMistDigit[1] = mistMin % 10;
  customMistDigit[2] = mistSec / 10;
  customMistDigit[3] = mistSec % 10;

  customRestDigit[0] = restMin / 10;
  customRestDigit[1] = restMin % 10;
  customRestDigit[2] = restSec / 10;
  customRestDigit[3] = restSec % 10;
}

void saveCustomTime() {
  int mistMin = customMistDigit[0] * 10 + customMistDigit[1];
  int mistSec = customMistDigit[2] * 10 + customMistDigit[3];

  int restMin = customRestDigit[0] * 10 + customRestDigit[1];
  int restSec = customRestDigit[2] * 10 + customRestDigit[3];

  prefs.putInt("mistMin", mistMin);
  prefs.putInt("mistSec", mistSec);
  prefs.putInt("restMin", restMin);
  prefs.putInt("restSec", restSec);
}

void setModeTime() {
  if (selectedMode == 1) {
    mistTime = 10UL * 1000UL;
    restTime = 5UL * 60UL * 1000UL;
  } else if (selectedMode == 2) {
    mistTime = 20UL * 1000UL;
    restTime = 10UL * 60UL * 1000UL;
  } else if (selectedMode == 3) {
    mistTime = 30UL * 1000UL;
    restTime = 15UL * 60UL * 1000UL;
  } else {
    int mistMin = customMistDigit[0] * 10 + customMistDigit[1];
    int mistSec = customMistDigit[2] * 10 + customMistDigit[3];

    int restMin = customRestDigit[0] * 10 + customRestDigit[1];
    int restSec = customRestDigit[2] * 10 + customRestDigit[3];

    mistTime = ((mistMin * 60UL) + mistSec) * 1000UL;
    restTime = ((restMin * 60UL) + restSec) * 1000UL;
  }
}

void runCycle() {
  unsigned long now = millis();

  if (mistOn && now - lastChange >= mistTime) {
    mistOn = false;
    lastChange = now;
  }

  if (!mistOn && now - lastChange >= restTime) {
    mistOn = true;
    lastChange = now;
  }

  digitalWrite(RELAY_PIN, mistOn ? RELAY_ON : RELAY_OFF);
}

String formatTime(unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  unsigned int min = totalSec / 60;
  unsigned int sec = totalSec % 60;

  char buf[6];
  sprintf(buf, "%02d:%02d", min, sec);
  return String(buf);
}


unsigned long getRemainTime() {
  unsigned long duration = mistOn ? mistTime : restTime;
  unsigned long elapsed = millis() - lastChange;

  if (elapsed >= duration) return 0;

  return duration - elapsed;
}


int readBatteryPercent() {
  int adc = analogRead(BATTERY_PIN);

  float adcVoltage = (adc / 4095.0) * 3.3;
  float batteryVoltage = adcVoltage * 5.0;

  int percent = (int)((batteryVoltage - BATTERY_MIN) * 100.0 / (BATTERY_MAX - BATTERY_MIN));

  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  return percent;
}

void drawBatteryIcon(int x, int y, int percent) {
  display.drawRect(x, y, 22, 10, SSD1306_WHITE);
  display.drawRect(x + 22, y + 3, 2, 4, SSD1306_WHITE);

  int fillWidth = map(percent, 0, 100, 0, 18);
  display.fillRect(x + 2, y + 2, fillWidth, 6, SSD1306_WHITE);
}

void drawTimeEdit(int x, int y, int d[4]) {
  display.setTextSize(2);
  display.setCursor(x, y);

  display.print(d[0]);
  display.print(d[1]);
  display.print(":");
  display.print(d[2]);
  display.print(d[3]);

  int underlineX[4] = {
    x,
    x + 12,
    x + 36,
    x + 48
  };

  display.drawLine(underlineX[editIndex], y + 18, underlineX[editIndex] + 9, y + 18, SSD1306_WHITE);
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  

  if (screen == SCREEN_HOME) {
    display.drawLine(80, 0, 80, 64, SSD1306_WHITE);
    drawBatteryIcon(85, 2, readBatteryPercent());
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("MIST ");
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(formatTime((running && mistOn) ? getRemainTime() : mistTime));
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("REST ");
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.print(formatTime((running && !mistOn) ? getRemainTime() : restTime));
    display.setTextSize(1);

    display.setCursor(0, 56);
    display.print("STATUS: ");
    if (!running) display.print("STOP");
    else display.print(mistOn ? "MIST" : "REST");

    // START / STOP
    if(homeSelect == 0){
      display.fillRect(82, 26, 44, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(85,28);
    display.print(running ? "STOP" : "START");

    // MODE
    if(homeSelect == 1){
      display.fillRect(82,38,44,10,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(85,40);
    display.print("MODE:");
    display.print(selectedMode);
  }

  else if (screen == SCREEN_MODE) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("SELECT MODE");

    display.setCursor(0, 16);
    if(modeSelect==1){
      display.fillRect(0,15,128,10,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0,16);
    display.print("MODE 1  00:10 / 05:00");

    // MODE2
    if(modeSelect==2){
      display.fillRect(0,27,128,10,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0,28);
    display.print("MODE 2  00:20 / 10:00");

    // MODE3
    if(modeSelect==3){
      display.fillRect(0,39,128,10,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0,40);
    display.print("MODE 3  00:30 / 15:00");

    // CUSTOM
    if(modeSelect==4){
      display.fillRect(0,51,128,10,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }else{
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0,52);
    display.print("CUSTOM");

    display.setTextColor(SSD1306_WHITE);
  }

  else if (screen == SCREEN_CUSTOM_MIST) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("CUSTOM MIST TIME");

    drawTimeEdit(30, 24, customMistDigit);

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("UP/DOWN OK BACK");
  }

  else if (screen == SCREEN_CUSTOM_REST) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("CUSTOM REST TIME");

    drawTimeEdit(30, 24, customRestDigit);

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("UP/DOWN OK BACK");
  }

  else if (screen == SCREEN_SAVE_CONFIRM) {
    display.setTextSize(1);
    display.setCursor(0, 8);
    display.print("SAVE CUSTOM TIME?");

    display.setCursor(0, 28);
    display.print("MIST ");
    display.print(customMistDigit[0]);
    display.print(customMistDigit[1]);
    display.print(":");
    display.print(customMistDigit[2]);
    display.print(customMistDigit[3]);

    display.setCursor(0, 40);
    display.print("REST ");
    display.print(customRestDigit[0]);
    display.print(customRestDigit[1]);
    display.print(":");
    display.print(customRestDigit[2]);
    display.print(customRestDigit[3]);

    display.setCursor(0, 56);
    display.print("OK=SAVE  MENU=BACK");
  }

  display.display();
}