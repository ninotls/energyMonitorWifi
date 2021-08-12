#include "EmonLib.h"
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// SoftwareSerial
#define RX 10
#define TX 11

String WIFI_SSID = "XXXXX";       // Your WiFi ssid
String PASSWORD =  "XXXXX";         // Password

// Thingspeak details
String HOST = "api.thingspeak.com";
String PATH = "/update?api_key=";
String writeAPIKey = "XXXXX";
String PORT = "80";

EnergyMonitor emon;
LiquidCrystal_I2C lcd(0x27, 20, 4);

float w_instantane_in  = 0.0;
float w_instantane_out = 0.0;
float kwh_cumule_out   = 0.0;
float kwh_cumule_in    = 0.0;
float puissance_reelle = 0.0;
float cosinus_phi      = 0.0;
int offset_in          = 0;
int offset_out         = 0;
// AT Commands
int countTrueCommand   = 0;
int countTimeCommand   = 0;
boolean found          = false;
String dataValues      = "";
String request         = "";

unsigned long previous_millis = 0;
unsigned long compteur_temp    = 0;

// Init Software serial
SoftwareSerial esp8266(RX, TX);


//-----------------------INITIALISATION DU PROGRAMME-------------------------------------------------

void setup()
{
  // ### Etalonnage - Debug ###
  //  Serial.begin(9600);
  //  Serial.println(esp8266.read());

  lcd.init();
  lcd.backlight();

  emon.voltage(0, 363, 1.7);
  emon.current(2, 28.6);

  // Init ESP8266 serial connection
  esp8266.begin(115200);
  esp8266.println("AT");

  // Connexion to WiFI Network
  sendCommandToESP8266("AT", 5, "OK");
  lcd.setCursor(0,0);
  lcd.print("Initializing ESP01");
  sendCommandToESP8266("AT+CWMODE=1", 5, "OK");
  lcd.setCursor(0,1);
  lcd.print("Connecting to Wifi");
  lcd.setCursor(0,2);
  lcd.print(WIFI_SSID);
  sendCommandToESP8266("AT+CWJAP=\"" + WIFI_SSID + "\",\"" + PASSWORD + "\"", 20, "OK");

  //1ere ligne
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conso:");
  lcd.setCursor(11, 0);
  lcd.print("Wts Cos_");
  lcd.print((char) 236);

  //2eme ligne
  lcd.setCursor(0, 1);
  lcd.print("Rejet:");
  lcd.setCursor(11, 1);
  lcd.print("Wts ");

  //3eme ligne
  lcd.setCursor(0, 2);
  lcd.print("Cumul conso:");
  lcd.setCursor(17, 2);
  lcd.print("kWh");

  //4eme ligne
  lcd.setCursor(0, 3);
  lcd.print("Cumul rejet:");
  lcd.setCursor(17, 3);
  lcd.print("kWh");
}
//----------------------- DEMARRAGE DE LA BOUCLE----------------------------------------------------

void loop()
{
  emon.calcVI(40, 2000);
  puissance_reelle = emon.realPower;
  cosinus_phi = emon.powerFactor;

  // ### Etalonnage ###
  //  float verif_voltage    = emon.Vrms;
  //  float verif_ampere     = emon.Irms;

  //--------------------------Etalonnage des volts et ampères sans LCD--------------------------------------

  // ### Etalonnage ###
  //  Serial.print("Est-ce le bon voltage? ");
  //  Serial.print(verif_voltage);
  //  Serial.print(" V  ");
  //  Serial.print(verif_ampere);
  //  Serial.println(" A ");

  //----------------POUR AVOIR LES W, Wh et kWh de l'élélectricité qui rentre et de l'électricité qui sort de ma maison------------------

  if (puissance_reelle >= 0)
  {
    w_instantane_in  = puissance_reelle;
    w_instantane_out = 0.0;

    kwh_cumule_in += puissance_reelle * (millis() - previous_millis) / 3600000000;
  }
  else
  {
    w_instantane_in = 0.0;
    w_instantane_out = abs(puissance_reelle);

    kwh_cumule_out += w_instantane_out * (millis() - previous_millis) / 3600000000;
  }

  // Update LCD Display
  //1ere ligne
  lcd.setCursor(6, 0);
  lcd.print("     ");
  if (w_instantane_in < 1.00) {
    lcd.setCursor(9, 0);
  }
  else {
    lcd.setCursor(10 - log10(w_instantane_in), 0);
  }
  lcd.print(w_instantane_in, 0);

  //2eme ligne
  lcd.setCursor(6, 1);
  lcd.print("     ");
  if (w_instantane_out < 1.00) {
    lcd.setCursor(9, 1);
  }
  else {
    lcd.setCursor(10 - log10(w_instantane_out), 1);
  }
  lcd.print(w_instantane_out, 0);
  lcd.setCursor(15, 1);
  lcd.print("   ");
  lcd.setCursor(15, 1);
  lcd.print(abs(cosinus_phi), 2);

  //3eme ligne
  if (kwh_cumule_in < 1.00) {
    offset_in = 0;
  } else {
    offset_in = floor(log10(kwh_cumule_in));
  }
  lcd.setCursor(12, 2);
  lcd.print("     ");
  lcd.setCursor(15 - abs(offset_in), 2);
  lcd.print(floor(kwh_cumule_in), 0);

  //4eme ligne
  if (kwh_cumule_out < 1.00) {
    offset_out = 0;
  } else {
    offset_out = floor(log10(kwh_cumule_out));
  }
  lcd.setCursor(12, 3);
  lcd.print("     ");
  lcd.setCursor(15 - abs(offset_out), 3);
  lcd.print(floor(kwh_cumule_out), 0);

  compteur_temp += millis() - previous_millis;

  // Send data to server
  if (compteur_temp >= 30000)
  {
    dataValues = "field1=" + String(w_instantane_in) + "&field2=" + String(w_instantane_out)
                 + "&field3=" + String(cosinus_phi) + "&field4=" + String(kwh_cumule_in)
                 + "&field5=" + String(kwh_cumule_out);
    request = "GET " + PATH + writeAPIKey + "&" + dataValues + "\r\n";
    sendCommandToESP8266("AT+CIPMUX=0", 2, "OK");
    sendCommandToESP8266("AT+CIPSTART=\"TCP\",\"" + HOST + "\"," + PORT, 2, "OK");
    sendCommandToESP8266("AT+CIPSEND=" + String(request.length()), 2, ">");
    sendData(request);
    sendCommandToESP8266("AT+CIPCLOSE", 2, "OK");
    compteur_temp = 0;
  }

  previous_millis = millis();
}

void sendCommandToESP8266(String command, int maxTime, char readReplay[]) {
  // Debug
  //  Serial.print(countTrueCommand);
  //  Serial.print(". at command => ");
  //  Serial.print(command);
  //  Serial.print(" ");

  while (countTimeCommand < maxTime)
  {
    esp8266.println(command);
    if (esp8266.find(readReplay))
    {
      found = true;
      break;
    }

    countTimeCommand++;
  }

  if (found == true)
  {
    //Debug
    //Serial.println("Success");

    countTrueCommand++;
  }
  else {
    //Debug
    //Serial.println("Fail");

    countTrueCommand = 0;
  }

  countTimeCommand = 0;
  found = false;
}

void sendData(String postRequest) {
  //Debug
  //Serial.println(postRequest);

  esp8266.println(postRequest);
  countTrueCommand++;
}
