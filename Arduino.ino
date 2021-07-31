/***************************************** Librerías *****************************************/
// Librerias ESP8266
#include <ESP8266WiFi.h>
#include <HTTPSRedirect.h>
#include <ESP8266HTTPClient.h>

// Librerias para OTA
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

// Librerias LCD
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/
// Definiciones para la deputacion para el puerto serie
// 0 sin depuración
// 1 con depuracion por puerto seria
#define DEBUG_MODE                  DEBUG_NONE  // Establece aqui el modo de depuración

#define DEBUG_NONE                  0
#define DEBUG_SERIAL                1

#if (DEBUG_MODE == DEBUG_NONE)
#define DEBUG_PRINT(...)
#define DEBUG_PRINTF(...)
#define DEBUG_PRINTLN(...)
#elif (DEBUG_MODE == DEBUG_SERIAL)
#define DEBUG_PRINT(...)            Serial.print(__VA_ARGS__)
#define DEBUG_PRINTF(...)           Serial.printf(__VA_ARGS__)
#define DEBUG_PRINTLN(...)          Serial.println(__VA_ARGS__)
#endif

// Estados del sistema
#define SYSTEM_STATE_IDLE           0
#define SYSTEM_STATE_PROCESSING     1
#define SYSTEM_STATE_READY          2
/*********************************************************************************************/

/***************************************** Instancias ****************************************/
// Configuración de la RED
const char SSID[] = "TPLINK_24G";
const char PASSWORD[] = "BasexB1Au1974*";

const IPAddress staticIP(192, 168, 0, 100);         // IP estática
const IPAddress gateway(192, 168, 0, 1);            // Dirección de gateway
const IPAddress subnet(255, 255, 255, 0);           // Mascara de red
const IPAddress primaryDNS(8, 8, 8, 8);             // Dirección DNS 1
const IPAddress secondaryDNS(8, 8, 4, 4);           // Dirección DNS 2

// Servidor web
AsyncWebServer server(80);

// Configuracion OTA
const char *OTA_USERNAME = "DEFY";
const char *OTA_PASSWORD = "DEFY0102";

// Configuración de Google Scripts
const String GOOGLE_SCRIPT_ID = "AKfycbxd-sjre0PKJ-6WF29JRffbp6R3ZCauxzoNei-2Lnh-DdSndn6ZjlsJzTr6mKUH9TNhOg";
const char *GOOGLE_SCRIPT_HOST = "script.google.com";
const String GOOGLE_SCRIPT_URL = "/macros/s/" + GOOGLE_SCRIPT_ID + "/exec";
const int GOOGLE_SCRIPT_PORT = 443;

// Estructura de datos para el request de https
typedef struct https_request
{
  int requestCode;
  String payload;
}https_request_t;

String data = ""; // Datos que se enviaran a Google

// Estado del sistema
uint8_t systemState = SYSTEM_STATE_IDLE;

// Funcion de envio de datos via POST
bool sendDataPOST(const char *host, const String &url, const int port,
                  const String &data, https_request_t *httpsRequest);

// Funcion que analiza los datos recibidos via POST
void readDataPOST(String payload);

// LCD
#if (DEBUG_MODE != DEBUG_SERIAL)
// LCD Adafruit_PCD8544(sclk_pin, din_pin, dc_pin, cs_pin, rst_pin)
Adafruit_PCD8544 display = Adafruit_PCD8544(D10, D9, D2, D1, D0);

// Funcion de bienvenida
void lcdWellcome();
#endif
/*********************************************************************************************/

/************************************ Configuración inicial **********************************/
void setup()
{
  // Pines GPIO
  pinMode(LED_BUILTIN, OUTPUT);

  // Puerto serie
#if (DEBUG_MODE == DEBUG_SERIAL)
  Serial.begin(9600);
#endif

#if (DEBUG_MODE != DEBUG_SERIAL)
  // Display
  display.begin();

  display.setContrast(60);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  lcdWellcome();
#endif

  // Configuración de la conexión
  WiFi.mode(WIFI_STA);  // Modo estación
  WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS); // Configuración de la conexión
  WiFi.begin(SSID, PASSWORD); // Se conecta a la RED

  DEBUG_PRINTLN("Iniciando conexion");
#if (DEBUG_MODE != DEBUG_SERIAL)
  display.println("Iniciando");
  display.println("conexion");

  display.display();
#endif

  // Se espera a que este realizada la conexión
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    DEBUG_PRINT("Conexion fallida, reiniciando");
#if (DEBUG_MODE != DEBUG_SERIAL)
    display.println("Fallo,");
    display.println("reiniciando");

    display.display();
#endif

    delay(5000);

    ESP.restart();
  }

  // Se habilita el OTA
  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  server.begin();

  DEBUG_PRINTLN("WiFi conectado con IP: " + WiFi.localIP().toString());

#if (DEBUG_MODE != DEBUG_SERIAL)
  display.println("IP:");
  display.println(WiFi.localIP().toString());

  display.display();
#endif

  // Se espera un poco a la visualizacion
  delay(5000);

#if (DEBUG_MODE != DEBUG_SERIAL)
  display.clearDisplay();
  display.display();
#endif
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Sistema en reposo esperando al pase de una tarjeta
  if (systemState == SYSTEM_STATE_READY)
  {
    delay(30000);

    // Datos a enviar
    data = "uid=93b8f22e";

    // Se pasa a estado de procesamiento
    systemState = SYSTEM_STATE_PROCESSING;
  }

  // Sistema procesando el pase de una tarjeta
  else if (systemState == SYSTEM_STATE_PROCESSING)
  {
    // Respuesta recibida por el envio del paquete
    https_request_t httpsRequest;
    
    // Se envia el paquete de datos y se retorna la respuesta
    if (sendDataPOST(GOOGLE_SCRIPT_HOST, GOOGLE_SCRIPT_URL, GOOGLE_SCRIPT_PORT, data, &httpsRequest))
    {
      DEBUG_PRINTLN("-------------- CONEXION CORRECTA ---------------");
      DEBUG_PRINTLN("---------------- Datos enviados ----------------");
      DEBUG_PRINTLN(String("HOST: ") + GOOGLE_SCRIPT_HOST + GOOGLE_SCRIPT_URL);
      DEBUG_PRINTLN("DATOS: " + data);
      DEBUG_PRINTLN("----------------- Request POST -----------------");
      DEBUG_PRINTLN("CODE: " + String(httpsRequest.requestCode));
      DEBUG_PRINTLN("PAYLOAD: " + httpsRequest.payload);

      if (httpsRequest.requestCode == HTTP_CODE_OK)
      {
        readDataPOST(httpsRequest.payload);
      }
    }

    else
    {
      DEBUG_PRINTLN("------------ CONEXION CON ERRORES --------------");

#if (DEBUG_MODE != DEBUG_SERIAL)
      display.clearDisplay();
      display.println("NUEVO INGRESO");
      display.println("REGISTRO FAIL:");
      display.println("Error de");
      display.println("conexion");

      display.display();
#endif
    }
    DEBUG_PRINTLN("------------------------------------------------");

    delay(7000);

    systemState = SYSTEM_STATE_IDLE;
  }

  // Se imprime que el sistema estará en espera
  else if (systemState == SYSTEM_STATE_IDLE)
  {
#if (DEBUG_MODE != DEBUG_SERIAL)
    display.clearDisplay();
    display.println("Pase tarjeta");

    display.display();
#endif

    systemState = SYSTEM_STATE_READY;
  }
}
/*********************************************************************************************/

/****************************************** Funciones ****************************************/
#if (DEBUG_MODE != DEBUG_SERIAL)
/*
 * Funcion de mensaje de inicio
 */
void lcdWellcome()
{
  display.setCursor(0, 0);

  display.setTextSize(2);
  display.println("  DEFY");
  display.println(" MOTION");

  display.display();

  display.setTextSize(1);

  delay(5000);

  display.clearDisplay();
}
#endif

/*
 * Función de envio de datos a un HOST
 * 
 * Esta función envia datos al host con url por el puerto
 * via POST y devuelve el request.
 */
bool sendDataPOST(const char *host, const String &url, const int port, const String &data, https_request_t *httpsRequest)
{
  // Mensaje de proceso
#if (DEBUG_MODE != DEBUG_SERIAL)
  display.clearDisplay();
  display.println("NUEVO INGRESO");
  display.println("REGISTRANDO");

  display.display();
#endif

  // Respuesta de conexión
  bool returnCode = false;

  // Conexión con cliente
  HTTPSRedirect client(port);

  // Request por defecto
  httpsRequest->requestCode = -1;
  httpsRequest->payload = "";

  // Establecemos una conexión insegura, es decir no nos importa el certificado SSL
  client.setInsecure();

  // Iniciamos la conexión con host
  if (client.connect(host, port))
  {
    // Se manda el POST y se captura la respuesta
    client.POST(url, host, data, false);

    httpsRequest->requestCode = client.getStatusCode();
    httpsRequest->payload = client.getResponseBody();

    returnCode = true;
  }

  else
  {
    returnCode = false;
  }

  return returnCode;
}

/*
 * Funcion encargada de analizar los datos recibidos por POST
 */
void readDataPOST(String payload)
{
  // Identificacion del UID (primer dato del payload)
  int initIndex = payload.indexOf("=");
  int endIndex = payload.indexOf("&");

  String uid = payload.substring(initIndex + 1, endIndex);

  payload.remove(0, endIndex + 1);

  // Identificacion del nombre (segundo dato del payload)
  initIndex = payload.indexOf("=");
  endIndex = payload.indexOf("&");

  String name = payload.substring(initIndex + 1, endIndex);

  payload.remove(0, endIndex + 1);

  // Identificacion del estado (tercer dato del payload)
  initIndex = payload.indexOf("=");
  endIndex = payload.indexOf("&");

  String state = payload.substring(initIndex + 1, endIndex);

  payload.remove(0, endIndex + 1);

  // Identificacion de la hora (cuarto dato del payload)
  initIndex = payload.indexOf("=");
  endIndex = payload.indexOf("\r\n");

  String timeDate = payload.substring(initIndex + 1, endIndex);

  payload.remove(0);
  
  DEBUG_PRINTLN("-------------- DATOS RESULTANTES ---------------");
  DEBUG_PRINTLN("UID: " + uid);
  DEBUG_PRINTLN("NOMBRE: " + name);
  DEBUG_PRINTLN("ESTADO: " + state);
  DEBUG_PRINTLN("HORA: " + timeDate);

#if (DEBUG_MODE != DEBUG_SERIAL)
  display.clearDisplay();
  display.println("NUEVO INGRESO");
  display.println("REGISTRO OK:");
  display.println(uid);
  display.println(name);
  display.println(state);
  display.println(timeDate);

  display.display();
#endif
}
/*********************************************************************************************/
