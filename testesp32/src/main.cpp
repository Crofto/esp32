#include <Arduino.h>
#include <WiFi.h>
#include <soc/rtc.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoBLE.h> 
#include <EEPROM.h>
#include <String.h>

#define M1_CALPOINT1_CELSIUS 23.0f
#define M1_CALPOINT1_RAW 128253742.0f
#define M1_CALPOINT2_CELSIUS -20.0f
#define M1_CALPOINT2_RAW 114261758.0f
const char* ssid = "Honor 10";      // Nom du réseau Wi-Fi
const char* password = "mot de passe pour le fucking tp";  // Mot de passe du réseau Wi-Fi

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR byte TIME_TO_SLEEP = 10; /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR byte TIME_TO_SEND = 30; /* Time ESP32 will send data every 30 seconds */
RTC_DATA_ATTR byte CONFIG = 2; /* ESP32 type of send */
RTC_DATA_ATTR int bootCount = 0;
const char* serverURL = "https://192.168.54.50:7265";

#define M2_CALPOINT1_CELSIUS 23.0f
#define M2_CALPOINT1_RAW 163600.0f
#define M2_CALPOINT2_CELSIUS -20.0f
#define M2_CALPOINT2_RAW 183660.0f

const char* mqttUser = "bah mon username pair";
const char* mqttPassword = "bah mon username pair";

/* BLE
BLEService batteryService("100F");
BLEUnsignedCharCharacteristic batteryLevel("2A19", BLERead | BLENotify);

BLEService InternalTemperatureService ("91991b50-cad7-43ce-a0ff-6f4f6cac9298");
BLECharCharacteristic lastTempChar("1cd1fe57-6dc3-41a0-95b2-39fe46f4c373", BLERead | BLENotify);
BLEByteCharacteristic sleepTime("2cd1fe57-6dc3-41a0-95b2-39fe46f4c373", BLERead | BLEWrite | BLENotify);
BLEByteCharacteristic config("3cd1fe57-6dc3-41a0-95b2-39fe46f4c373", BLERead | BLEWrite | BLENotify);
BLEByteCharacteristic sendTime("4cd1fe57-6dc3-41a0-95b2-39fe46f4c373", BLERead | BLEWrite | BLENotify);

*/

const char* publish = "topic/ynov-lyon-2023/esp32/reymbaut/in";

float readTemp1(bool printRaw = false) {
  uint64_t value = 0;
  int rounds = 100;

  for(int i=1; i<=rounds; i++) {
    value += rtc_clk_cal_ratio(RTC_CAL_RTC_MUX, 100);
    yield();
  }
  value /= (uint64_t)rounds;

  if(printRaw) {
    Serial.print(__FUNCTION__);
    Serial.print(": raw value is: ");
    Serial.println(value);
  }  

  return ((float)value - M1_CALPOINT1_RAW) * (M1_CALPOINT2_CELSIUS - M1_CALPOINT1_CELSIUS) / (M1_CALPOINT2_RAW - M1_CALPOINT1_RAW) + M1_CALPOINT1_CELSIUS;
}


float readTemp2(bool printRaw = false) {
  uint64_t value = rtc_time_get();
  delay(100);
  value = (rtc_time_get() - value);

  if(printRaw) {
    printf("%s: raw value is: %llu\r\n", __FUNCTION__, value);
  }

  return ((float)value*10.0 - M2_CALPOINT1_RAW) * (M2_CALPOINT2_CELSIUS - M2_CALPOINT1_CELSIUS) / (M2_CALPOINT2_RAW - M2_CALPOINT1_RAW) + M2_CALPOINT1_CELSIUS;
}

RTC_DATA_ATTR int tabTemp[60]; 
RTC_DATA_ATTR int count;

//const char* mqttServer = "b0f050d3e8404beb9021ecc91d65d179.s2.eu.hivemq.cloud";
//const int mqttPort = 8883;
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
WiFiClient espClient;
PubSubClient client(espClient);



void connexionWifi(){

  esp_wifi_start();
  // Connexion au réseau Wi-Fi
  WiFi.mode( WIFI_STA );
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connexion au réseau Wi-Fi en cours...");
  }

  Serial.println("Connecté au réseau Wi-Fi !");

  client.setServer(mqttServer, mqttPort);

  while (!client.connected()) {
    if (client.connect("ArduinoClient3222222225852")){//,mqttUser, mqttPassword)) {
      Serial.println("Connecté au broker MQTT !");
      //client.subscribe(publish);  // Spécifiez les sujets auxquels vous souhaitez vous abonner
    } else {
      Serial.print("Échec de la connexion MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" Réessayer dans 5 secondes...");
      delay(5000);
    }
  }
}

String createJson(){
  String payload = "{\"config\":{\"tempFreq\": " + String(TIME_TO_SLEEP) + ",\"connectionConfig\":" + String(CONFIG) + ",\"connectionFreq\":" + String(TIME_TO_SEND) + "},\"temperatures\":[";
  for (int i = 0; i < count; i++) {
    payload += (String)tabTemp[i];
    if (i + 1 < count) {
      payload += ",";
    }
  }
  payload += "]}";
  return payload;
}

void sendDataToMQTT() {
  String payload = createJson();

  bool reussite = client.publish(publish, payload.c_str());  // Spécifiez le sujet sur lequel vous souhaitez publier les données
  if (reussite)  
    Serial.print("envoiemqréussie\r\n");
  else
    Serial.print("envoiemqraté\r\n");
}


void parseJson(const char* jsonString) {
  const size_t bufferSize = JSON_OBJECT_SIZE(3) + 40;
  
  // Création d'un objet DynamicJsonDocument pour stocker les données parsées
  StaticJsonDocument<512> jsonBuffer;

  // Parse du JSON à partir de la chaîne de caractères
  DeserializationError error = deserializeJson(jsonBuffer, jsonString);

  // Vérification des erreurs de parsing
  if (error) {
    Serial.print("Erreur de parsing du JSON : ");
    Serial.println(error.c_str());
    int tempTimeToSleep = EEPROM.read(1);
    if (!isnan(tempTimeToSleep)) TIME_TO_SLEEP = tempTimeToSleep >= 1 ? tempTimeToSleep : 10;
    tempTimeToSleep = EEPROM.read(2);
    if (!isnan(tempTimeToSleep)) CONFIG = tempTimeToSleep >= 1 ? tempTimeToSleep : 2;
    tempTimeToSleep = EEPROM.read(3);
    if (!isnan(tempTimeToSleep)) TIME_TO_SEND = tempTimeToSleep >= 1 ? tempTimeToSleep : 30;
  }
  else {
    // Récupération des valeurs du JSON
    int freq = jsonBuffer["tempFreq"];
    int conf = jsonBuffer["connectionConfig"];
    int connexFreq = jsonBuffer["connectionFreq"];

    EEPROM.put(1, freq);
    EEPROM.put(2, conf);
    EEPROM.put(3, connexFreq);
    
    TIME_TO_SLEEP = freq; /* Time ESP32 will go to sleep (in seconds) */
    CONFIG = conf;
    TIME_TO_SEND = connexFreq; /* Time ESP32 will send data every 30 seconds */
  }

  //à priori il y a besoin de ca sinon il me chie à la gueule
  Serial.println(TIME_TO_SLEEP);  
  Serial.println(CONFIG);  
  Serial.println(TIME_TO_SEND);  
}

void setup() {
  Serial.begin(9600);
  Serial.print("===================================================\r\n");

  // Connexion au réseau Wi-Fi

  connexionWifi();

  HTTPClient http;
  // Envoyez la requête POST au serveur
  http.begin(serverURL + String("/values"));
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.GET();
  // Vérifiez la réponse du serveur
  if (httpResponseCode > 0) {
    Serial.print("Code de réponse HTTP : ");
    Serial.println(http.errorToString(httpResponseCode));
    String payload = http.getString();
    parseJson(payload.c_str());   
    
  } else {
    Serial.println("Erreur lors de la requête HTTP.");
    Serial.println(httpResponseCode);    
  }

  http.end();
  
/* blutooth
  if (!BLE.begin()){
    Serial.println ("BLE non démaré");    
  }
  Serial.println ("BLE  démaré");    
  BLE.setDeviceName("Esp32SmbDemoTanguy");
  BLE.setLocalName("Esp32SmbDemoTanguy");
  Serial.println ("BLE  nommé");    


  batteryService.addCharacteristic(batteryLevel);
  InternalTemperatureService.addCharacteristic(lastTempChar);
  InternalTemperatureService.addCharacteristic(config);
  InternalTemperatureService.addCharacteristic(sendTime);  
  InternalTemperatureService.addCharacteristic(sleepTime);


  config.writeValue(CONFIG);    
  sleepTime.writeValue(TIME_TO_SLEEP);    
  sendTime.writeValue(TIME_TO_SEND);

  BLE.setAdvertisedService(batteryService);
  BLE.setAdvertisedService(InternalTemperatureService);
  
  BLE.addService(batteryService);
  BLE.addService(InternalTemperatureService);

  Serial.println ("char envoyé");    

  BLE.advertise();
*/

  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000); 
  count=0;
}

void sendDataToServer() {  
  HTTPClient http;

  String requestBody = createJson();

  // Envoyez la requête POST au serveur
  http.begin(serverURL + String("/values"));
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(requestBody);
  http.end();

  // Vérifiez la réponse du serveur
  if (httpResponseCode > 0) {
    Serial.print("Code de réponse HTTP : ");
    Serial.println(httpResponseCode);
  } else {
    Serial.println("Erreur lors de la requête HTTP.");
    Serial.println(httpResponseCode);    
  }
}


void loop() {

/*
  BLEDevice central = BLE.central();
  int batteryLevelValue = 23;
  int lastTempValue = 23;

  if(central){
    Serial.printf("Connected to central device : %s\n", central.address());
    while (central.connected()){
      batteryLevel.writeValue(batteryLevelValue);
      if(batteryLevelValue == 100) batteryLevelValue = 0;
      batteryLevelValue ++;

      lastTempChar.writeValue(readTemp1(true));
      Serial.println ("char ecrit"); 
      BLE.poll();

      if (config.written()) {
        byte value = config.value();
        Serial.print("Valeur reçue : ");
        Serial.println(value);
      }

      delay(1000);      
    }
    Serial.print("Central est déconnecté");

  }
  */

  // Lire les valeurs de température
  float temp1 = readTemp1(true);
  float temp2 = readTemp2(true);

  tabTemp[count] = (int)temp1;  
  count++; 

  if( count * TIME_TO_SLEEP >= TIME_TO_SEND ){
    connexionWifi();
    // Envoyer les données au serveur
    if(CONFIG == 1){
      sendDataToServer();
    }
    else{
      sendDataToMQTT();
    }
    esp_wifi_disconnect();
    memset(tabTemp, 0, sizeof(tabTemp));
    count = 0;
  }

  client.loop();

  // Attendre 30 secondes avant d'envoyer les données suivantes
  Serial.println(TIME_TO_SLEEP);
  delay(TIME_TO_SLEEP * 1000);  
  //esp_light_sleep_start();
}


void onCharacteristicChanged(BLECharacteristic& characteristic){
    Serial.println("changed");
}
