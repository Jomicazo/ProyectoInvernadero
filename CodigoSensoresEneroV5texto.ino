#include <ESP8266WiFi.h> 
#include "DHTesp.h"
#include "ArduinoJson.h"
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
//DHHT11

DHTesp dht1;
DHTesp dht2;
DHTesp dht3;
DHTesp dht4;

// keep in sync with slave struct
#define MENSAJE_MAXSIZE 250
char mensaje[MENSAJE_MAXSIZE];

unsigned long waitMs=0;

// cadenas para topics e ID
char ID_PLACA[16];
char topic_PUB[256];
char topic_SUB[256];

WiFiClient wClient;
PubSubClient mqtt_client(wClient);

// Update these with values suitable for your network.
const char* ssid = "MIWIFI_k2Cu";
const char* password = "c74aEp26";
const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "II6";
const char* mqtt_pass = "8YfOHejt";

// GPIOs
int LED1 = 2;  
int LED2 = 16;
int LED_OTA = 16;

// datos para actualización   >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("172.16.53.142")         // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu"

//Activación ventiladores
char act[4];

// FOTA

//funciones para progreso de OTA
void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);
//-----------------------------------------------------
void intenta_OTA()
{ 
  Serial.println( "--------------------------" );  
  Serial.println( "Comprobando actualización:" );
  Serial.print(HTTP_OTA_ADDRESS);Serial.print(":");Serial.print(HTTP_OTA_PORT);Serial.println(HTTP_OTA_PATH);
  Serial.println( "--------------------------" );  
  ESPhttpUpdate.setLedPin(LED_OTA, LOW);
  ESPhttpUpdate.onStart(inicio_OTA);
  ESPhttpUpdate.onError(error_OTA);
  ESPhttpUpdate.onProgress(progreso_OTA);
  ESPhttpUpdate.onEnd(final_OTA);
  WiFiClient wClient;
  switch(ESPhttpUpdate.update(wClient, HTTP_OTA_ADDRESS, HTTP_OTA_PORT, HTTP_OTA_PATH, HTTP_OTA_VERSION)) {
    case HTTP_UPDATE_FAILED:
      Serial.printf(" HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F(" El dispositivo ya está actualizado"));
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F(" OK"));
     break;
    }
}
//-----------------------------------------------------
void inicio_OTA()
{
  Serial.println("Nuevo Firmware encontrado. Actualizando...");
}
//-----------------------------------------------------
void progreso_OTA(int x, int todo)
{
  char cadena[256];
  int progress=(int)((x*100)/todo);
  if(progress%10==0)
  {
    snprintf(cadena,256,"Progreso: %d%% - %dK de %dK",progress,x/1024,todo/1024);
    Serial.println(cadena);
  }
}
//-----------------------------------------------------


// MQTT

void conecta_wifi() {
  Serial.printf("\nConnecting to %s:\n", ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
}

//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass)) {
      Serial.printf(" conectado a broker: %s\n",mqtt_server);
      mqtt_client.subscribe(topic_SUB);
    } else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) {

  act[0]=(char)payload[1];

 act[1]=(char)payload[3];

act[2]=(char)payload[5];

 act[3]=(char)payload[7];
  
  char *mensaje = (char *)malloc(length+1); // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length); // copio el mensaje en cadena de caracteres
  mensaje[length]='\0'; // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  free(mensaje);
}

// Json

String serializa_StringJSON (float ilum,float* hum,float* temp,bool* vent)
{
  StaticJsonDocument<300> doc;
  String jsonString;
  doc["iluminacion"] = ilum;
  doc["hum"][0] = hum[0];
    doc["hum"][1] = hum[1];
      doc["hum"][2] = hum[2];
        doc["hum"][3] = hum[3];
  doc["temp"][0] = temp[0];
    doc["temp"][1] = temp[1];
      doc["temp"][2] = temp[2];
        doc["temp"][3] = temp[3];
  doc["vent"][0]=vent[0];
   doc["vent"][1]=vent[1];  
    doc["vent"][2]=vent[2];  
     doc["vent"][3]=vent[3];    
  serializeJson(doc,jsonString);
  return jsonString;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  dht1.setup(16, DHTesp::DHT11); // Sensor 1
  dht2.setup(5, DHTesp::DHT11); // Sensor 1
  dht3.setup(4, DHTesp::DHT11); // Sensor 1
  dht4.setup(0, DHTesp::DHT11); // Sensor 1
  pinMode(LED1, OUTPUT);    // inicializa GPIO como salida
  pinMode(LED2, OUTPUT);    
  digitalWrite(LED1, HIGH); // apaga el led
  digitalWrite(LED2, HIGH); 
  // crea topics usando id de la placa
  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topic_PUB, "II6/%s/DATOS",ID_PLACA);
  sprintf(topic_SUB, "II6/%s/Ventiladores",ID_PLACA);
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  Serial.printf("Identificador placa: %s\n", ID_PLACA );
  Serial.printf("Topic publicacion  : %s\n", topic_PUB);
  Serial.printf("Topic subscripcion : %s\n", topic_SUB);
  Serial.printf("Termina setup en %lu ms\n\n",millis());
  // Setup iluminacion
  pinMode(A0, INPUT);
  // Setup ventiladores
  pinMode(D5, OUTPUT); //Ventilador 1
  pinMode(D6, OUTPUT); //Ventilador 2
  pinMode(D7, OUTPUT); //Ventilador 3
  pinMode(D8, OUTPUT); //Ventilador 4
 
 
}

void loop() {
  mqtt_client.loop(); // esta llamada para que la librería recupere el control
  // put your main code here, to run repeatedly:
  intenta_OTA();
  waitMs=dht1.getMinimumSamplingPeriod();
  delay(waitMs);
  Serial.print("Se envia \n");
  float t[4];
  float h[4];
  bool v[4];
// Temperatura
  t[0]=dht1.getTemperature();
  
  t[1]=dht2.getTemperature();
    
  t[2]=dht2.getTemperature();

  t[3]=dht4.getTemperature();

//  Humedad
  h[0]=dht1.getHumidity();

  h[1]=dht2.getHumidity();

  h[2]=dht3.getHumidity();

  h[3]=dht4.getHumidity();

// Medidas de seguridad para que JSON no de problemas

   for(int i = 0; i<=4; i++){

       t[i] = (isnan(t[i]))? -255 : t[i]; // Si es NaN pongo un valor numérico para que JSON no de problemas
    
        h[i] = (isnan(h[i]))? -255 : h[i];

  }
 
  if (act[0]=='1') {
    digitalWrite(D5, HIGH); //Encender o apagar ventilador 1
    v[0]=true; 
  } else {
    digitalWrite(D5, LOW);
    v[0]=false; 
  }
  if (act[1]=='1') {  //Encender o apagar ventilador 2
    digitalWrite(D6, HIGH);
    v[1]=true; 
  } else {
 
    digitalWrite(D6, LOW);
    v[1]=false; 
  }
  if (act[2]=='1') {  //Encender o apagar ventilador 3
    digitalWrite(D7, HIGH);
    v[2]=true; 
  } else {
 
    digitalWrite(D7, LOW);
    v[2]=false;
  }  
  if (act[3]=='1') {  //Encender o apagar ventilador 4
    digitalWrite(D8, HIGH);
    v[3]=true;
  }
  else{
    digitalWrite(D8, LOW);
    v[3]=false;
  }
  float l=analogRead(A0);


  strncpy(mensaje, serializa_StringJSON(l,h,t,v).c_str(), MENSAJE_MAXSIZE); //Mensaje Json de los huertos

  Serial.print(mensaje); 
  Serial.print("\n");
  mqtt_client.publish(topic_PUB, mensaje); //Enviar mensjae
  delay(500);
}
