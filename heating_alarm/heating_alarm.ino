#include <LiquidCrystalRus.h>
#include <microDS18B20.h>
#include <ESP8266WiFi.h>

#define TIMER_INITIALIZATION 60
#define TEMPERATURE_TRESHOLD 30
#define SEND_STATS_EVERY_X_TICK 5

#define STATE_INITIALIZATION 1
#define STATE_ARMED 2
#define STATE_ALARM 3
#define STATE_DISARMED 4
#define ABSOLUTE_ZERO_TEMP -273
#define SERIAL_FREQUENCY 115200

static const float ANALOG_RANGE_STEPS = 1024.0;

static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;

/*
 * ESP8266 Pinout: https://lastminuteengineers.com/esp8266-pinout-reference/
 */
static const uint8_t GPIO16 = D0; // HIGH at boot, used to wake up from deep sleep
static const uint8_t GPIO5  = D1;
static const uint8_t GPIO4  = D2;
static const uint8_t GPIO0  = D3; // connected to FLASH button, boot fails if pulled LOW
static const uint8_t GPIO2  = D4; // HIGH at boot, boot fails if pulled LOW
static const uint8_t GPIO14 = D5;
static const uint8_t GPIO12 = D6;
static const uint8_t GPIO13 = D7;
static const uint8_t GPIO15 = D8; // Required for boot, boot fails if pulled HIGH

uint8_t symbolGrad[8] = {
  B00110,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000
};

uint8_t symbolOnline[8] = {
  B10001,
  B10101,
  B10001,
  B10101,
  B00100,
  B01110,
  B11111
};

uint8_t symbolOffline[8] = {
  B10001,
  B01010,
  B00100,
  B01010,
  B10001,
  B01110,
  B11111
};

#define SYMBOL_GRAD_CODE 0
#define SYMBOL_ONLINE_CODE 1
#define SYMBOL_OFFLINE_CODE 2

const char* ssid = "Altai";
const char* password = "WanConnect2";

const char* host = "vpn.jbtits.com";
const int port = 80;

int state = STATE_INITIALIZATION;
int lastNotifiedState = -1;
int timer = 0;
int currentStatsTick = 0;

int pinBeep = GPIO0;
// int pinFlameDetector = GPIO15;
// int pinVoltmeter = A0;
MicroDS18B20<2> sensor;

LiquidCrystalRus lcd(GPIO16, GPIO14, GPIO12, GPIO13, GPIO5, GPIO4); // (RS, E, DB4, DB5, DB6, DB7)

void playSuccessSound() {
  tone(pinBeep, 500);
  delay(200);
  noTone(pinBeep);
}

void playErrorSound() {
  tone(pinBeep, 20);
  delay(200);
  noTone(pinBeep);
}

void playAlarmSound() {
  tone(pinBeep, 4000);
  delay(1000);
  noTone(pinBeep);
}

int getCurrentTemperature() {
  sensor.requestTemp();
  delay(1000);
  if (sensor.readTemp()) {
    return sensor.getTemp();
  } else {
    return ABSOLUTE_ZERO_TEMP;
  }
}

char sign(int a) {
  if (a > 0) {
    return '+';
  } else if (a < 0) {
    return '-';
  } else {
    return ' ';
  }
}

void changeState(int newState) {
  state = newState;
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("WiFi DISCONNECTED ssid(" + String(ssid) + ")");
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("WiFi CONNECTED ssid(" + String(ssid) + ")");
      break;
    default:
      // do nothing   
      break; 
  }
}

void wifiSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiEvent);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
}

void lcdSetup() {
  lcd.begin(16, 2); // (cols, rows)
  lcd.createChar(SYMBOL_GRAD_CODE, symbolGrad);
  lcd.createChar(SYMBOL_ONLINE_CODE, symbolOnline);
  lcd.createChar(SYMBOL_OFFLINE_CODE, symbolOffline);
}

bool httpGet(const String& url) {
  WiFiClient client;
  bool connected = WiFi.status() == WL_CONNECTED && client.connect(host, port);

  if (connected) {
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "Connection: close\r\n" +
            "\r\n");
    client.stop();
    return true;
  }
  return false;
}

bool httpStats(int temp, bool lastConnectedState) {
  if (currentStatsTick == 0) {
    String url = "/ha/stats?t=";
    url += String(temp);
    url += "&tl=";
    url += String(TEMPERATURE_TRESHOLD);
    return httpGet(url);
  }
  currentStatsTick = (currentStatsTick + 1) % SEND_STATS_EVERY_X_TICK;
  return lastConnectedState;
}

bool httpChangeState(int state) {
  String url = "/ha/state?s=";
  url += String(state);
  return httpGet(url);
}

bool httpError(const String& msg) {
    String url = "/ha/error?msg=";
    url += String(msg);
    return httpGet(url);  
}

char getConnectionStateSymbol(bool connected) {
  return connected ? char(SYMBOL_ONLINE_CODE) : char(SYMBOL_OFFLINE_CODE);
}

void setup() {
  timer = TIMER_INITIALIZATION;
  Serial.begin(SERIAL_FREQUENCY);
  delay(10);
  lcdSetup();
  wifiSetup();
}

void loop() {
  int t = getCurrentTemperature();
  bool connected = false;
  if (state != lastNotifiedState) {
    connected = httpChangeState(state);
    lastNotifiedState = state;
  }

  if (state == STATE_INITIALIZATION) {
    if (timer > 0) {
      connected = httpStats(t, connected);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Подготовка: " + String(timer));
      lcd.setCursor(0, 1);
      lcd.print(sign(t) + String(t) + char(SYMBOL_GRAD_CODE));
      lcd.setCursor(13, 1);
      lcd.print('(' + String(getConnectionStateSymbol(connected)) + ')');

      Serial.print("STATE_INITIALIZATION t=");
      Serial.print(t);
      Serial.print(" tl=");
      Serial.print(TEMPERATURE_TRESHOLD);
      Serial.print(" timer=");
      Serial.println(timer);

      timer--;
    } else {
      changeState(STATE_ARMED);
      playSuccessSound();
    }
  } else if (state == STATE_ARMED) {
    if (t > TEMPERATURE_TRESHOLD) {
      connected = httpStats(t, connected);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("В работе:");
      lcd.setCursor(0, 1);
      lcd.print(sign(t) + String(t) + char(SYMBOL_GRAD_CODE));
      lcd.setCursor(13, 1);
      lcd.print('(' + String(getConnectionStateSymbol(connected)) + ')');

      Serial.print("STATE_ARMED t=");
      Serial.print(t);
      Serial.print(" tl=");
      Serial.println(TEMPERATURE_TRESHOLD);
    } else if (t != ABSOLUTE_ZERO_TEMP) {
      changeState(STATE_ALARM);
    } else {
      Serial.println("ERROR! Can't read temperature value! Try to make it again...");
      httpError("cant_read_temp");
      playErrorSound();
    }
  } else if (state == STATE_ALARM) {
    lcd.clear();
    lcd.print("СРАБОТАЛА");
    lcd.setCursor(0, 1);
    lcd.print("СИГНАЛИЗАЦИЯ !!!");
    playAlarmSound();
    lcd.clear();

    Serial.print("STATE_ALARM t=");
    Serial.print(t);
    Serial.print(" tl=");
    Serial.println(TEMPERATURE_TRESHOLD);
  } else {
    changeState(STATE_INITIALIZATION);
    playErrorSound();

    Serial.println("ERROR! Rollback to init state...");
    httpError("unknown_state");
  }
}
