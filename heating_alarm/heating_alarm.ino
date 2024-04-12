#include <LiquidCrystalRus.h>
#include <microDS18B20.h>
#include <ESP8266WiFi.h>

#define TIMER_INITIALIZATION 60
#define TEMPERATURE_TRESHOLD 35
#define SEND_STATS_EVERY_X_TICK 5

#define STATE_INITIALIZATION 1
#define STATE_ARMED 2
#define STATE_ALARM 3
#define STATE_DISARMED 4
#define ABSOLUTE_ZERO_TEMP -273

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
bool connected = false;
int currentStatsTick = 0;

int pinBeep = 0;
int pinBtn = 15;
int pinPotentiometer = A0;
MicroDS18B20<2> sensor;

LiquidCrystalRus lcd(16, 14, 12, 13, 5, 4); // (RS, E, DB4, DB5, DB6, DB7)

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
    connected = false;
      Serial.println("WiFi DISCONNECTED ssid(" + String(ssid) + ")");
      break;
    case WIFI_EVENT_STAMODE_GOT_IP:
    connected = true;
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

void httpGet(const String& url) {
  WiFiClient client;

  if (WiFi.status() == WL_CONNECTED && client.connect(host, port)) {
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
            "Host: " + host + "\r\n" +
            "Connection: close\r\n" +
            "\r\n");
    client.stop();
  }  
}

void httpStats(int temp) {
  if (currentStatsTick == 0) {
    String url = "/ha/stats?t=";
    url += String(temp);
    httpGet(url);
  }
  currentStatsTick = (currentStatsTick + 1) % SEND_STATS_EVERY_X_TICK;
}

void httpChangeState(int state) {
  if (state != lastNotifiedState) {
    String url = "/ha/state?s=";
    url += String(state);
    httpGet(url);
    lastNotifiedState = state;
  }
}

void httpError(const String& msg) {
    String url = "/ha/error?msg=";
    url += String(msg);
    httpGet(url);  
}

char getWifiStatusSymbol() {
  return connected ? char(SYMBOL_ONLINE_CODE) : char(SYMBOL_OFFLINE_CODE);
}

void setup() {
  timer = TIMER_INITIALIZATION;
  Serial.begin(115200);
  delay(10);
  lcdSetup();
  pinMode(pinBtn, INPUT);
  wifiSetup();
}

void loop() {
  int stateBtn = digitalRead(pinBtn);
  int t = getCurrentTemperature();
  httpChangeState(state);
  lcd.clear();

  if (state == STATE_INITIALIZATION) {
    if (timer > 0) {
      lcd.print("Подготовка: " + String(timer));
      lcd.setCursor(0, 1);
      lcd.print(sign(t) + String(t) + char(SYMBOL_GRAD_CODE));
      lcd.setCursor(13, 1);
      lcd.print('(' + String(getWifiStatusSymbol()) + ')');
      timer--;

      Serial.print("STATE_INITIALIZATION t=");
      Serial.print(t);
      Serial.print(" tl=");
      Serial.print(TEMPERATURE_TRESHOLD);
      Serial.print(" timer=");
      Serial.println(timer);
    } else {
      changeState(STATE_ARMED);
      playSuccessSound();
    }
  } else if (state == STATE_ARMED) {
    if (t > TEMPERATURE_TRESHOLD) {
      lcd.print("В работе:");
      lcd.setCursor(0, 1);
      lcd.print(sign(t) + String(t) + char(SYMBOL_GRAD_CODE));
      lcd.setCursor(13, 1);
      lcd.print('(' + String(getWifiStatusSymbol()) + ')');

      Serial.print("STATE_ARMED t=");
      Serial.print(t);
      Serial.print(" tl=");
      Serial.print(TEMPERATURE_TRESHOLD);

      httpStats(t);
    } else if (t != ABSOLUTE_ZERO_TEMP) {
      changeState(STATE_ALARM);
    } else {
      Serial.println("ERROR! Can't read temperature value! Try to make it again...");
      httpError("cant_read_temp");
      playErrorSound();
    }
  } else if (state == STATE_ALARM) {
    lcd.print("СРАБОТАЛА");
    lcd.setCursor(0, 1);
    lcd.print("СИГНАЛИЗАЦИЯ !!!");
    tone(pinBeep, 4000);
    delay(1000);
    noTone(pinBeep);

    Serial.print("STATE_ALARM t=");
    Serial.print(t);

    if (stateBtn == HIGH) {
      changeState(STATE_DISARMED);
      playSuccessSound();
      lcd.clear();
      lcd.print("Отключение");
      lcd.setCursor(0, 1);
      lcd.print("сигнализации...");
      delay(10000);
      playSuccessSound();
    }
  } else if (state == STATE_DISARMED) {
    lcd.print("Нажмите кнопку,");
    lcd.setCursor(0, 1);
    lcd.print("чтобы включить");

    Serial.print("STATE_DISARMED t=");
    Serial.print(t);

    if (stateBtn == HIGH) {
      timer = TIMER_INITIALIZATION;
      changeState(STATE_INITIALIZATION);
      playSuccessSound();
    }
  } else {
    changeState(STATE_INITIALIZATION);
    playErrorSound();

    Serial.println("ERROR! Rollback to init state...");
    httpError("unknown_state");
  }
}
