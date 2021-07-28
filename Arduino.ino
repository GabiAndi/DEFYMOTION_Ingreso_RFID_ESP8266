/***************************************** Librerías *****************************************/
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/

/*********************************************************************************************/

/***************************************** Instancias ****************************************/
// Configuración de la RED
const char SSID[] = "DEFYMOTION";
const char PASSWORD[] = "laquequieras";

const IPAddress staticIP(192, 168, 1, 100);         // IP estática
const IPAddress gateway(192, 168, 1, 1);            // Dirección de gateway
const IPAddress subnet(255, 255, 255, 0);           // Mascara de red
const IPAddress primaryDNS(8, 8, 8, 8);             // Dirección DNS 1
const IPAddress secondaryDNS(8, 8, 4, 4);           // Dirección DNS 2

// Google
const String GOOGLE_SCRIPT_ID = "AKfycbwpNdDrvYaH2NDdEJJYZyEvE_QygqanishV76WQK6NBh8E7p84jxqP1xf8BcHtmW20N";
const String GOOGLE_SCRIPT_HOST = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec";

const String UNIT_NAME = "defymotion_ingreso_rfid";

// Estructura de datos para el request de https
typedef struct https_request
{
  int requestCode;
  String payload;
}https_request_t;

String data = ""; // Datos que se enviaran a Google

void sendData(const String host, String &data, https_request_t *httpsRequest); // Funcion de datos
/*********************************************************************************************/

/************************************ Configuración inicial **********************************/
void setup()
{
  // Pines GPIO
  pinMode(LED_BUILTIN, OUTPUT);

  // Puerto serie
  Serial.begin(9600);

  // Configuración de la conexión
  WiFi.mode(WIFI_STA);  // Modo estación
  WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS); // Configuración de la conexión
  WiFi.begin(SSID, PASSWORD); // Se conecta a la RED

  // Se espera a que este realizada la conexión
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");

    delay(1000);  // Este delay SIEMPRE debe estar, de otra manera, se satura de consultas al ESP con WiFi.status()
  }

  Serial.println("");
  Serial.println("WiFi conectado con IP: " + WiFi.localIP().toString());
  
  delay(100); // Delay para en inicio
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Test de conexión
  data = "";
  
  for (uint8_t i = 0 ; i < 4 ; i++)
  {
    data += String(0x0A + i, HEX);
  }

  data = "id=" + String(UNIT_NAME) + "&uid=" + data;

  https_request_t httpsRequest;
  
  sendData(GOOGLE_SCRIPT_HOST, data, &httpsRequest); // Paquete de datos

  Serial.println("---------------- Datos enviados ----------------");
  Serial.println("HOST: " + GOOGLE_SCRIPT_HOST);
  Serial.println("DATOS: " + data);
  Serial.println("----------------- Request POST -----------------");
  Serial.println("CODE: " + String(httpsRequest.requestCode));
  Serial.println("PAYLOAD: " + httpsRequest.payload);
  
  delay(30000); // Delay entre tomas
}
/*********************************************************************************************/

/****************************************** Funciones ****************************************/
/*
 * Función de envio de datos a un HOST
 * 
 * Esta función envia datos al dominio via POST y
 * devuelve el request.
 */
void sendData(const String host, String &data, https_request_t *httpsRequest)
{
  // Para el estado de retorno
  httpsRequest->requestCode = HTTP_CODE_BAD_REQUEST;
  httpsRequest->payload = "";

  // Conexión con cliente
  WiFiClientSecure client;
  HTTPClient https;

  // Establecemos una conexión insegura, es decir no nos importa el certificado SSL
  client.setInsecure();

  // Iniciamos la conexión con host
  if (https.begin(client, host))
  {
    Serial.println("Conexión con cliente establecida.");

    // Se añade la cabecera con el tipo de dato para el POST
    https.addHeader("Content-Type", "application/plain-text");

    // Se manda el POST y se captura la respuesta
    httpsRequest->requestCode = https.POST(data);
    httpsRequest->payload = https.getString();

    https.end();
  }

  else
  {
    Serial.println("No se pudo conectar al cliente.");
  }
}
/*********************************************************************************************/
