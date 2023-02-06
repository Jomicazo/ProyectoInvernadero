#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient wClient;
PubSubClient mqtt_client(wClient);

// Update these with values suitable for your network.
const char* ssid = "sagemcom2A30";
const char* password = "T4GM5M2NUJZZMJ";
const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "II6";
const char* mqtt_pass = "8YfOHejt";

// cadenas para topics e ID
char ID_PLACA[16];
char topic_PUB[256];
char topic_SUB[256];
char topic_temp[256];
char topic_conex[256];
char topic_led_pub[256];
char topic_led_sub[256];
char topic_PUB_2[256];
char topic_SUB_2[256];

// datos para actualización   >>>> SUSTITUIR IP <<<<<
#define HTTP_OTA_ADDRESS      F("172.16.166.105")         // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
                                                       // Name of firmware
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu"

// GPIOs
float valor;
int GPIO5= 5;
int LED1=2;
int LED2=16;
// Intensidad del LED
int int_led;
// GPIOs
int LED_blink= 2;  
int LED_OTA = 16; 
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

void inicio_OTA()
{
  Serial.println("Nuevo Firmware encontrado. Actualizando...");
}

void error_OTA(int e)
{
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);
}

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
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass, "infind/GRUPO6/Conexion", 1, true, "{\"online\": false}")) {
      int led = valor;
      Serial.printf(" Conectado a broker: %s\n", mqtt_server);
    } else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
//-----------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Empieza setup...");
  pinMode(LED1, OUTPUT);  // inicializa GPIO como salida
  pinMode(LED2, OUTPUT);
  pinMode(A0, INPUT); 
  pinMode(GPIO5, OUTPUT);
  digitalWrite(LED1, HIGH);  // apaga el led
  digitalWrite(LED2, HIGH);
  sprintf(ID_PLACA, "ESP_%d", ESP.getChipId());
  sprintf(topic_PUB, "II6/%s/Gen", ID_PLACA);
  conecta_wifi();
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512);  // para poder enviar mensajes de hasta X bytes
  conecta_mqtt();

  Serial.printf("Identificador placa: %s\n", ID_PLACA);
  Serial.printf("Topic publicacion  : %s\n", topic_PUB);
  Serial.printf("Termina setup en %lu ms\n\n", millis());
  Serial.println();

  String thisBoard = ARDUINO_BOARD;
  Serial.println(thisBoard);

}
#define TAMANHO_MENSAJE 128
unsigned long ultimo_mensaje = 0;
unsigned long sendMs = 0;
//-----------------------------------------------------
//     LOOP
//-----------------------------------------------------
void loop() {
  // put your main code here, to run repeatedly:
if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop();  // esta llamada para que la librería recupere el control
  unsigned long ahora = millis();
  if (ahora - ultimo_mensaje >= 60000) {
    char mensaje[TAMANHO_MENSAJE];
    ultimo_mensaje = ahora;
    float valor = digitalRead(16);
    //float valor=analogRead(A0);   

  
    snprintf(mensaje, TAMANHO_MENSAJE, "{\"Gen\": \"%f\"}", valor);
    // ahora la cadena "mensaje" contiene el mensaje formateado en JSON
    Serial.println(mensaje);
    sendMs = millis();
    mqtt_client.publish(topic_PUB, mensaje);
    digitalWrite(LED2, LOW);  // enciende el led al enviar mensaje
}
  if (digitalRead(LED2) == LOW && ahora - ultimo_mensaje >= 100)
    digitalWrite(LED2, HIGH);
}