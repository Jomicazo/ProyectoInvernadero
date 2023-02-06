#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>


WiFiClient wClient;
PubSubClient mqtt_client(wClient);

// Update these with values suitable for your network.
const char* ssid = "infind";
const char* password = "1518wifi";
const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "II6";
const char* mqtt_pass = "8YfOHejt";

// ID placa
char ID_PLACA[16];

// Topics
char topic_PUB_Riego[256];
char topic_PUB_Conexion[256];
char topic_SUB_Riego[256];

// datos para actualización   >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("172.16.53.142")         // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu"

// GPIOs
int LED1 = 2;
int LED2 = 16;
int LED_OTA = 16;

int cont = 0;
int valor;

// funciones para progreso de OTA
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
void final_OTA()
{
  Serial.println("Fin OTA. Reiniciando...");
}
//-----------------------------------------------------
void error_OTA(int e)
{
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);
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
void conecta_wifi() {
  Serial.printf("\nConnecting to %s:\n", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
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
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass, "II6/ESP/Conexion", 1, true, "Servicio RIEGO offline.")) {
      Serial.printf(" conectado a broker: %s\n", mqtt_server);
      mqtt_client.publish(topic_PUB_Conexion, "Servicio RIEGO online.", true);
      int riego = valor;
      mqtt_client.subscribe(topic_SUB_Riego);

    } else {
      Serial.printf("failed, rc=%d  try again in 25s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(25000);
    }
  }
}

//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  char* mensaje = (char*)malloc(length + 1);  // reservo memoria para copia del mensaje
  strncpy(mensaje, (char*)payload, length);   // copio el mensaje en cadena de caracteres
  mensaje[length] = '\0';                     // caracter cero marca el final de la cadena
  Serial.printf("Mensaje recibido [%s] %s\n", topic, mensaje);
  // compruebo el topic
  if (strcmp(topic, topic_SUB_Riego) == 0) {
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, mensaje, length);
    if (error)
    {
      Serial.print("Error deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    else if (root.containsKey("riego"))
    {
      cont=int(payload[0]);
      valor = root["riego"];
    }
    else {
      Serial.print("Error : ");
      Serial.println("\"riego\" key not found in JSON");
    }
  }
  else
  {
     Serial.println("Error: Topic desconocido");   
  }  
}

//-----------------------------------------------------
//     SETUP
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  pinMode(LED1, OUTPUT);  // inicializa GPIO como salida
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, HIGH); 
  digitalWrite(LED2, HIGH);
  // Obtenemos chipID de la placa y generamos el topic para NodeRED
  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topic_PUB_Riego, "II6/ESP%d/nivel_riego", ESP.getChipId());
  sprintf(topic_PUB_Conexion, "II6/ESP/Conexion");
  sprintf(topic_SUB_Riego, "II6/ESP%d/Riego", ESP.getChipId());
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512);
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  Serial.printf("Identificador placa: %s\n", ID_PLACA);
  Serial.printf("Topic publicacion  : %s\n", topic_PUB_Riego);
  Serial.printf("Topic publicacion  : %s\n", topic_PUB_Conexion);
  Serial.printf("Termina setup en %lu ms\n\n", millis());
  
}


//-----------------------------------------------------
#define TAMANHO_MENSAJE 128
unsigned long ultimo_mensaje = 0;
//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------
void loop() {
  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop();  
  unsigned long ahora = millis(); // tiempo actual
  if (ahora - ultimo_mensaje >= 30000) { // 30 segundos entre mensajes
    ultimo_mensaje = ahora; // Ajustamos el tiempo
    intenta_OTA();
    char mensaje[TAMANHO_MENSAJE];
    char mensajeControl[TAMANHO_MENSAJE];
    int nivel1, nivel2, nivel3;
    int varControl;
    
    if (cont != 0)
    {
      analogWrite(5,1023);
    }
    else
    {
      analogWrite(5,0);
      };    
    analogWriteRange (1);
    nivel1 = analogRead(12);
    nivel2 = analogRead(13);
    nivel3 = analogRead(15);
    if (nivel3 == 1) 
    {
      varControl = 0;
      snprintf(mensaje, TAMANHO_MENSAJE, "{\"Tanque\" : %d, \n\"Control\" : %d}",2, varControl); // Cadena formateada en JSON
      Serial.println(mensaje);      
    }
    else if (nivel2 == 1)
    {
      varControl = 1;
      snprintf(mensaje, TAMANHO_MENSAJE, "{\"Tanque\" : %d, \n\"Control\" : %d}",1, varControl); // Cadena formateada en JSON
      Serial.println(mensaje);      
    }
    else
    {
      varControl = 2;
      snprintf(mensaje, TAMANHO_MENSAJE, "{\"Tanque\" : %d, \n\"Control\" : %d}",0, varControl); // Cadena formateada en JSON
      Serial.println(mensaje);      
    }

    mqtt_client.publish(topic_PUB_Riego, mensaje);

    digitalWrite(LED2, LOW);  // LED ON
  }
 if (digitalRead(LED2) == LOW && ahora - ultimo_mensaje >= 100)
    digitalWrite(LED2, HIGH);
}
