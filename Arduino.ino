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

// Librerias para el RFID
#include <SPI.h>
#include <MFRC522.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/
// Definiciones para la deputacion para el puerto serie
// 0 sin depuración
// 1 con depuracion por puerto serie
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

// Pines del lector RF
#define SS_PIN                      D4
#define RST_PIN                     D3

// Numero maximo de usuarios
#define MAX_USERS                   20
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

// Servidor web
AsyncWebServer server(80);

// Configuracion OTA
const char *OTA_USERNAME = "DEFY";
const char *OTA_PASSWORD = "DEFY0102";

// Configuración de Google Scripts
const String GOOGLE_SCRIPT_ID = "AKfycbxre4Zetg29e0i2PdZ-ID4j9tw6CczbxfNzoiA3Xke4rHvpLLNbDPRvF8gAbUTcs1GCpw";
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

// Estructura de usuarios
typedef struct system_user
{
  String name;
  String uid;
  String state;
}system_user_t;

// Estructura de datos del sistema
typedef struct system
{
  uint8_t systemState;
  system_user_t *usersEntry[MAX_USERS];
}system_t;

system_t systemManager;

// Funcion de envio de datos via POST
bool sendDataPOST(const char *host, const String &url, const int port,
                  const String &data, https_request_t *httpsRequest);

// Funcion que analiza los datos recibidos via POST
void readDataPOST(String &payload);

// Funcion que añade un usario a los ingresados
void addUserToUsersEntry(system_user *user);

// Funcion que remueve un usario de los ingresados
void removeUserToUsersEntryUID(String &uid);

// LCD
#if (DEBUG_MODE != DEBUG_SERIAL)
// LCD Adafruit_PCD8544(sclk_pin, din_pin, dc_pin, cs_pin, rst_pin)
Adafruit_PCD8544 display = Adafruit_PCD8544(D10, D9, D2, D1, D0);

// Funcion de bienvenida
void lcdWellcome();
#endif

// RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);

byte uidCard[4];
/*********************************************************************************************/

/************************************ Configuración inicial **********************************/
void setup()
{
  // Pines GPIO
  pinMode(LED_BUILTIN, OUTPUT);

  // Inicio del system manager
  systemManager.systemState = SYSTEM_STATE_IDLE;

  for (uint i = 0 ; i < MAX_USERS ; i++)
  {
    systemManager.usersEntry[i] = nullptr;
  }

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

  // Lector RFID
  SPI.begin();
  mfrc522.PCD_Init();

  // Se inicia la pagina local
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Pagina principal");
  });

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
  if (systemManager.systemState == SYSTEM_STATE_READY)
  {
    // Si se detecto una nueva tarjeta
    if (mfrc522.PICC_IsNewCardPresent())
    {
      // Seleccionamos una tarjeta
      if (mfrc522.PICC_ReadCardSerial())
      {
        data = "";

        for (uint8_t i = 0 ; i < 4 ; i++)
        {
          uidCard[i] = mfrc522.uid.uidByte[i];

          data += String(uidCard[i], HEX);
        }

        data = String("uid=" + data);
        
        mfrc522.PICC_HaltA();

        // Se pasa a estado de procesamiento
        systemManager.systemState = SYSTEM_STATE_PROCESSING;
      }
    }
  }

  // Sistema procesando el pase de una tarjeta
  else if (systemManager.systemState == SYSTEM_STATE_PROCESSING)
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

      else
      {
        display.clearDisplay();
        display.println("NUEVO INGRESO");
        display.println("REGISTRO FAIL:");
        display.println("Error " + String(httpsRequest.requestCode));

        display.display();

        delay(5000);
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

    systemManager.systemState = SYSTEM_STATE_IDLE;
  }

  // Se imprime que el sistema estará en espera
  else if (systemManager.systemState == SYSTEM_STATE_IDLE)
  {
#if (DEBUG_MODE != DEBUG_SERIAL)
    display.clearDisplay();
    display.println("INGRESOS");
    
    // Se imprimen los usuario ingresados
    uint8_t printLCDIndex = 0;

    for (uint i = 0 ; i < 10 ; i++)
    {
      if (printLCDIndex == 5)
      {
        display.setCursor(display.width() / 2, 8);
      }

      if (systemManager.usersEntry[i] != nullptr)
      {
        display.println(systemManager.usersEntry[i]->name);

        printLCDIndex++;
      }
    }

    display.display();
#endif

    systemManager.systemState = SYSTEM_STATE_READY;
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

    if (httpsRequest->requestCode > 0)
    {
      returnCode = true;
    }
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
void readDataPOST(String &payload)
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

  // Si el usuario entra se lo añade
  if (state == "ENTRADA")
  {
    addUserToUsersEntry(new system_user_t{name, uid, state});
  }

  // Si el usuario sale se lo quita
  else if (state == "SALIDA")
  {
    removeUserToUsersEntryUID(uid);
  }

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

/*
 * Funcion que añade usuarios a la lista de ingresados
 */
void addUserToUsersEntry(system_user *user)
{
  // Bandera de lugar disponible
  bool freeSpace = false;

  // Donde se encuentre un espacio se asigna
  for (uint i = 0 ; i < MAX_USERS ; i++)
  {
    if (systemManager.usersEntry[i] == nullptr)
    {
      systemManager.usersEntry[i] = user;

      freeSpace = true;

      break;
    }
  }

  // Si no hay lugar disponible se elimina la memoria creada
  if (!freeSpace)
  {
    delete user;
  }
}

/*
 * Funcion que remueve usuarios de la lista de ingresados
 */
void removeUserToUsersEntryUID(String &uid)
{
  // Donde se encuentre el usuario se elimina
  for (uint i = 0 ; i < MAX_USERS ; i++)
  {
    if (systemManager.usersEntry[i] != nullptr)
    {
      if (systemManager.usersEntry[i]->uid == uid)
      {
        delete systemManager.usersEntry[i];

        systemManager.usersEntry[i] = nullptr;

        break;
      }
    }
  }
}
/*********************************************************************************************/
