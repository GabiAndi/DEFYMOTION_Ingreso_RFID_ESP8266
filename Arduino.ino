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

// Librerias para el uso de la EEPROM
#include <EEPROM_Rotate.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/
// Definiciones para la deputacion para el puerto serie
// 0 sin depuración
// 1 con depuracion por puerto serie
#define DEBUG_MODE                  DEBUG_NONE  // Establece aqui el modo de depuración

// Definiciones para el estado
// 0 sin pagina de log
// 1 con pagina de log
#define LOG_MODE                    LOG_ON      // Establece una pagina donde ver el registro de estado

// Depuracion
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

// Log
#define LOG_OFF                     0
#define LOG_ON                      1

#if (LOG_MODE == LOG_ON)
#define SERVER_LOG_ROOT             "/log"

#define LOG_MAX_SIZE                4000  // Tamaño maximo de logueo en caracteres

#define ADD_LOG(...)                logAdd(__VA_ARGS__)
#else
#define ADD_LOG(...)
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

// Configuracion de la EEPROM
#define EEPROM_ROTATE_SIZE          1  // Uso de sectores para la rotacion
#define EEPROM_SIZE                 1  // Uso de sectores para el almacen de datos
#define EEPROM_BYTES_USER           4096 * EEPROM_SIZE  // Calculo de bytes utilizados
#define EEPROM_BYTES_OFFSET         10  // Offset de memoria
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

// Log
#if (LOG_MODE == LOG_ON)
String logData = "";
#endif

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

// EEPROM
EEPROM_Rotate eeprom;

// Estructura de datos que se va a guardar en la EEPROM
typedef struct system_eeprom
{
  system_user_t users_entry[MAX_USERS];
}system_eeprom_t;

// Estructura de datos del sistema
typedef struct system
{
  uint8_t system_state;
  system_eeprom_t system_eeprom_data;
}system_t;

system_t system_manager;

// Funcion de envio de datos via POST
bool sendDataPOST(const char *host, const String &url, const int port,
                  const String &data, https_request_t *httpsRequest);

// Funcion que analiza los datos recibidos via POST
void readDataPOST(String &payload);

// Funcion que añade un usario a los ingresados
void addUserToUsersEntry(system_user &user);

// Funcion que remueve un usario de los ingresados
void removeUserToUsersEntry(String &uid);

#if (LOG_MODE == LOG_ON)
// Funcion que se llama a un GET a la pagina de log
void logPage(AsyncWebServerRequest *request);

// Funcion que añade lineas al LOG
void logAdd(const String &log);
#endif

// Salva los datos en la eeprom
void eepromSave();

// Lee los datos de la eeprom
void eepromRestore();

// Escribe con datos vacios la eeprom
void eepromReset();

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
  system_manager.system_state = SYSTEM_STATE_IDLE;

  // Inicio de la EEPROM
  system_manager.system_eeprom_data = {"", "", ""};

  eeprom.size(EEPROM_ROTATE_SIZE);  // Sectores para la rotacion de memoria
  eeprom.begin(4096); // Uso el sector completo

  // Leemos la EEPROM
  ADD_LOG("Restaurando datos de la EEPROM");

  //eepromReset();  // Restauro la flash

  eepromRestore();  // Leo los datos almacenados

  ADD_LOG("Restaurados los datos de la EEPROM");

  // Puerto serie
#if (DEBUG_MODE == DEBUG_SERIAL)
  Serial.begin(9600);

  ADD_LOG("Depuración por puerto serie iniciada");

#elif (DEBUG_MODE == DEBUG_SERIAL)
  ADD_LOG("Depuración por puerto serie desactivada");
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
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    DEBUG_PRINT("Conexion fallida, reiniciando");

#if (DEBUG_MODE != DEBUG_SERIAL)
    display.println("Fallo,");
    display.println("reiniciando");

    display.display();

    delay(5000);
#endif

    ESP.restart();
  }

  DEBUG_PRINTLN("WiFi conectado con IP: " + WiFi.localIP().toString());

#if (DEBUG_MODE != DEBUG_SERIAL)
  display.println("IP:");
  display.println(WiFi.localIP().toString());

  display.display();
#endif

  // Se espera un poco a la visualizacion
  delay(5000);

  // Se habilita el OTA
  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  server.begin();

  // Lector RFID
  SPI.begin();
  mfrc522.PCD_Init();

#if (LOG_MODE == LOG_ON)
  // Se inicia la pagina local
  server.on(SERVER_LOG_ROOT, HTTP_GET, logPage);
#endif

#if (DEBUG_MODE != DEBUG_SERIAL)
  display.clearDisplay();
  display.display();
#endif
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Si el WiFi se fue se reinicia
  if (WiFi.status() != WL_CONNECTED)
  {
#if (DEBUG_MODE != DEBUG_SERIAL)
    display.println("Fallo,");
    display.println("reiniciando");

    display.display();

    delay(5000);
#endif

    ESP.reset();
  }

  // Sistema en reposo esperando al pase de una tarjeta
  if (system_manager.system_state == SYSTEM_STATE_READY)
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
        system_manager.system_state = SYSTEM_STATE_PROCESSING;
      }
    }
  }

  // Sistema procesando el pase de una tarjeta
  else if (system_manager.system_state == SYSTEM_STATE_PROCESSING)
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
#if (DEBUG_MODE != DEBUG_SERIAL)
        display.clearDisplay();
        display.println("NUEVO INGRESO");
        display.println("REGISTRO FAIL:");
        display.println("Error " + String(httpsRequest.requestCode));

        display.display();

        delay(5000);
#endif

        ADD_LOG("Ingreso con error de respuesta, codigo: " + String(httpsRequest.requestCode));
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

      ADD_LOG("Error al conectar con el servidor.");
#endif
    }
    DEBUG_PRINTLN("------------------------------------------------");

    delay(7000);

    system_manager.system_state = SYSTEM_STATE_IDLE;
  }

  // Se imprime que el sistema estará en espera
  else if (system_manager.system_state == SYSTEM_STATE_IDLE)
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

      if ((system_manager.system_eeprom_data.users_entry[i].name != "") ||
          (system_manager.system_eeprom_data.users_entry[i].state != "") ||
          (system_manager.system_eeprom_data.users_entry[i].uid != ""))
      {
        display.println(system_manager.system_eeprom_data.users_entry[i].name);

        printLCDIndex++;
      }
    }

    display.display();
#endif

    system_manager.system_state = SYSTEM_STATE_READY;
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
    system_user_t new_user;

    new_user.name = name;
    new_user.uid = uid;
    new_user.state = state;

    addUserToUsersEntry(new_user);
  }

  // Si el usuario sale se lo quita
  else if (state == "SALIDA")
  {
    removeUserToUsersEntry(uid);
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
void addUserToUsersEntry(system_user &user)
{
  // Donde se encuentre un espacio se asigna
  for (uint i = 0 ; i < MAX_USERS ; i++)
  {
    if ((system_manager.system_eeprom_data.users_entry[i].name == "") &&
        (system_manager.system_eeprom_data.users_entry[i].state == "") &&
        (system_manager.system_eeprom_data.users_entry[i].uid == ""))
    {
      system_manager.system_eeprom_data.users_entry[i].name = user.name;
      system_manager.system_eeprom_data.users_entry[i].state = user.state;
      system_manager.system_eeprom_data.users_entry[i].uid = user.uid;

      eepromSave();

      break;
    }
  }
}

/*
 * Funcion que remueve usuarios de la lista de ingresados
 */
void removeUserToUsersEntry(String &uid)
{
  // Donde se encuentre el usuario se elimina
  for (uint i = 0 ; i < MAX_USERS ; i++)
  {
    if ((system_manager.system_eeprom_data.users_entry[i].name != "") &&
        (system_manager.system_eeprom_data.users_entry[i].state != "") &&
        (system_manager.system_eeprom_data.users_entry[i].uid != ""))
    {
      if (system_manager.system_eeprom_data.users_entry[i].uid == uid)
      {
        system_manager.system_eeprom_data.users_entry[i].uid = "";
        system_manager.system_eeprom_data.users_entry[i].name = "";
        system_manager.system_eeprom_data.users_entry[i].state = "";

        eepromSave();

        break;
      }
    }
  }
}

#if (LOG_MODE == LOG_ON)
/*
 * Funcion que lista la pagina de log
 */
void logPage(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", "ESTADO DEL SISTEMA\r\n\r\n" + logData);
}

/*
 * Funcion que añade lineas al log
 */
void logAdd(const String &log)
{
  // Si con el nuevo mensaje se excede del tamaño maximo se elimina una linea
  if (logData.length() > LOG_MAX_SIZE)
  {
    logData.remove(0, logData.indexOf('\n') + 1);
  }

  logData += String(millis()) + ": " + log + "\r\n";
}
#endif

/*
 * Funcion que guarda los datos en la eeprom
 */
void eepromSave()
{
  // Escribimos la EEPROM
  for (uint16_t i = 0 ; i < sizeof(system_eeprom_t) ; i++)
  {
    eeprom.write(i + EEPROM_BYTES_OFFSET, ((uint8_t *)(&system_manager.system_eeprom_data))[i]);
    eeprom.commit();
  }
}

/*
 * Funcion que lee los datos en la eeprom
 */
void eepromRestore()
{
  for (uint16_t i = 0 ; i < sizeof(system_eeprom_t) ; i++)
  {
    ((uint8_t *)(&system_manager.system_eeprom_data))[i] = eeprom.read(i + EEPROM_BYTES_OFFSET);
  }
}

/*
 * Funcion que escribe datos vacios en la epprom
 */
void eepromReset()
{
  for (uint16_t i = 0 ; i < MAX_USERS ; i++)
  {
    system_manager.system_eeprom_data.users_entry[i] = {"", "", ""};
  }

  eepromSave();
}
/*********************************************************************************************/
