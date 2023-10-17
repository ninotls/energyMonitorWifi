#define CONNECTED_MODE 0

#include "EmonLib.h"
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#include <Vector.h>
#include <StringSplitter.h>

#if CONNECTED_MODE
#include <SoftwareSerial.h>

// SoftwareSerial
#define RX_PIN 11  // old: 2
#define TX_PIN 9   // old: 3
#endif

#define RED_LED_PIN 4
#define YELLOW_LED_PIN 5
#define GREEN_LED_PIN 6

#define DISP_BTN_PIN 2  // old: 7

#define SCREEN_DELAY 2  // number of sequence(s) of 30 sec

// OLED Screen
#define I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;

EnergyMonitor emon;

unsigned long previous_millis = 0;
unsigned long compteur_temp = 0;
float w_instantane_in = 0.0;
float w_instantane_out = 0.0;
float kwh_cumule_out = 0.0;
float kwh_cumule_in = 0.0;
uint8_t delayToSleep = 0;

String time = "";
bool screenOff = false;

#if CONNECTED_MODE
// Init Software serial
SoftwareSerial esp8266(RX_PIN, TX_PIN);
#endif

void setup() {
#if CONNECTED_MODE
  String WIFI_SSID = "MyFreeBox";  // Your WiFi ssid
  String PASSWORD = "abcde12345";  // Password
  String ip = "";
  // Init ESP8266 serial connection
  esp8266.begin(115200);
  esp8266.println("AT");

  // Connexion to WiFI Network
  sendCommandToESP8266("AT", 5, (char *)"OK");
  oled.setCursor(0, 0);
  oled.print("Init ESP01 ... ");
  sendCommandToESP8266("AT+RST", 5, (char *)"OK");
  delay(5000);
  sendCommandToESP8266("AT+CWMODE=1", 5, (char *)"OK");
  oled.println("Done");
  oled.print("Conn Wifi ... ");
  do {
    getLocalIP(&ip);
    delay(500);
  } while (ip == "0.0.0.0");
  oled.println("Done");
  sendCommandToESP8266("AT+CWJAP=\"" + WIFI_SSID + "\",\"" + PASSWORD + "\"", 20, (char *)"OK");
  oled.println(ip);
#endif
  // ### Etalonnage - Debug ###
  //Serial.begin(9600);

  // Init Display
  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(Adafruit5x7);
  oled.clear();

  emon.voltage(0, 201, 1.7);
  emon.current(1, 28.9);

  pinMode(DISP_BTN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DISP_BTN_PIN), wakeUpScreen, CHANGE);

  // Init LEDs
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  delay(500);
  digitalWrite(RED_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(500);
  digitalWrite(YELLOW_LED_PIN, LOW);
  delay(500);
  digitalWrite(RED_LED_PIN, LOW);

  initDisplay();
}
//----------------------- DEMARRAGE DE LA BOUCLE----------------------------------------------------

void loop() {
  emon.calcVI(40, 2000);

  // ### Etalonnage ###
  // float verif_voltage = emon.Vrms;
  // float verif_ampere = emon.Irms;
  // int puissance = (230 - 1*floor(verif_ampere))*verif_ampere*0.9;

  // Serial.print("Est-ce le bon voltage? ");
  // Serial.print(verif_voltage, 0);
  // Serial.print(" V  ");
  // Serial.print(verif_ampere, 2);
  // Serial.print(" A ");
  // Serial.print("PowerFactor = ");
  // Serial.print(emon.powerFactor);
  // Serial.print(" PR =");
  // Serial.print(emon.realPower, 0);
  // Serial.println();

  if (emon.realPower >= 0) {
    w_instantane_in = emon.realPower;
    w_instantane_out = 0.0;

    kwh_cumule_in += w_instantane_in * (millis() - previous_millis) / 3600000000;
  } else {
    w_instantane_in = 0.0;
    w_instantane_out = abs(emon.realPower);

    kwh_cumule_out += w_instantane_out * (millis() - previous_millis) / 3600000000;
  }

  if (!screenOff) {
    if (delayToSleep == 0) {
      initDisplay();
      delayToSleep++;
    }
    updateData();
  }

  compteur_temp += millis() - previous_millis;
  previous_millis = millis();

  // Send data to server
  if (compteur_temp >= 30000) {  // 20000
#if CONNECTED_MODE
    String request = "GET /update?api_key=XNZFTJFOJ8CSUPYQ&field1=" + String(w_instantane_in);
    request += "&field2=" + String(w_instantane_out);
    request += "&field3=" + String(emon.powerFactor);
    request += "&field4=" + String(kwh_cumule_in);
    request += "&field5=" + String(kwh_cumule_out);
    request += "\r\n\r\n";

    sendCommandToESP8266("AT+CIPMUX=1", 5, (char *)"OK");
    sendCommandToESP8266("AT+CIPSTART=0,\"TCP\",\"api.thingspeak.com\",80", 15, (char *)"OK");
    sendCommandToESP8266("AT+CIPSEND=0," + String(request.length()), 4, (char *)"OK");
    esp8266.println(request);
    delay(2000);
    sendCommandToESP8266("AT+CIPCLOSE=0", 5, (char *)"OK");
    sendCommandToESP8266("AT+RST", 5, (char *)"OK");
#endif
    compteur_temp = 0;
    if (delayToSleep == SCREEN_DELAY) {
      screenOff = true;
      oled.clear();
    }
    delayToSleep++;
  }
}

void clearValue(uint8_t col, uint8_t row) {
  uint8_t rows = oled.fontRows();
  oled.clear(col, 128, row, row + rows - 1);
}

void updateData() {
  oled.set2X();
  clearValue(40, 0);
  oled.print(w_instantane_in, 0);
  oled.print(" W");

  clearValue(40, 3);
  oled.print(w_instantane_out, 0);
  oled.print(" W");

  oled.set1X();
  clearValue(80, 6);
  oled.print(kwh_cumule_in, 0);
  oled.print(" kWh");

  clearValue(80, 7);
  oled.print(kwh_cumule_out, 0);
  oled.println(" kWh");
}

#if CONNECTED_MODE
void sendCommandToESP8266(String command, int maxTime, char readReplay[]) {
  // Debug
  // Serial.print(". at command => ");
  // Serial.print(command);
  // Serial.print(" ; ");

  bool cmd_ok = false;
  uint8_t countTimeCommand = 0;

  while (countTimeCommand < maxTime) {
    esp8266.print(command + "\r\n");
    if (esp8266.find(readReplay)) {
      // cmd_ok = true;
      // Serial.println("Cmd OK");
      break;
    }

    countTimeCommand++;
    delay(100);
  }

  // if (!cmd_ok)
  //   Serial.println("Cmd KO");

  delay(100);
}

void getLocalIP(String *localIP) {
  sendCommandToESP8266("AT+CIFSR", 5, (char *)"+CIFSR:STAIP");
  localIP->concat(esp8266.readStringUntil('\n'));

  localIP->remove(0, 2);
  localIP->remove(localIP->length() - 2, 2);
}

int separate(String str, char **p, int size) {
  int n;
  char s[100];

  strcpy(s, str.c_str());

  *p++ = strtok(s, " ");
  for (n = 1; NULL != (*p++ = strtok(NULL, " ")); n++)
    if (size == n)
      break;

  return n;
}
#endif

void wakeUpScreen() {
  if (screenOff) {
    screenOff = false;
    delayToSleep = 0;
  }
}

void initDisplay() {
  oled.clear();
  oled.set1X();

  oled.setCursor(0, 0);
  oled.print("Conso:");

  oled.setCursor(0, 3);
  oled.print("Rejet:");

  oled.setCursor(0, 6);
  oled.print("Cumul Conso:");

  oled.setCursor(0, 7);
  oled.print("Cumul Rejet:");
}
