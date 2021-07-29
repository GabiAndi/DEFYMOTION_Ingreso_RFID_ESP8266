/***************************************** Librerías *****************************************/
#include <ESP8266WiFi.h>
#include <HTTPSRedirect.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/

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

// Configuración de Google Scripts
const String GOOGLE_SCRIPT_ID = "AKfycbx89NLbtMisXLw49XCYycQeYgD9huv3huf626IlFQQE_te0nKaBfLlIvp-vN_JineEm";
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

// Funcion de envio de datos via POST
bool sendDataPOST(const char *host, const String &url, const int port,
                  const String &data, https_request_t *httpsRequest);
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

  // Mensaje de inicio
  delay(2000);

  Serial.println("Iniciando conexión");

  // Se espera a que este realizada la conexión
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");

    delay(1000);  // Este delay SIEMPRE debe estar, de otra manera, se satura de consultas al ESP con WiFi.status()
  }

  Serial.println("");
  Serial.println("WiFi conectado con IP: " + WiFi.localIP().toString());
  
  delay(1000); // Delay para en inicio
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Datos a enviar
  data = "";
  
  for (uint8_t i = 0 ; i < 4 ; i++)
  {
    data += String(0x0A + i, HEX);
  }

  data = "uid=" + data;

  // Respuesta recibida por el envio del paquete
  https_request_t httpsRequest;
  
  // Se envia el paquete de datos y se retorna la respuesta
  if (sendDataPOST(GOOGLE_SCRIPT_HOST, GOOGLE_SCRIPT_URL, GOOGLE_SCRIPT_PORT, data, &httpsRequest))
  {
    Serial.println("-------------- CONEXION CORRECTA ---------------");
  }

  else
  {
    Serial.println("------------ CONEXION CON ERRORES --------------");
  }

  Serial.println("---------------- Datos enviados ----------------");
  Serial.println("HOST: " + String(GOOGLE_SCRIPT_HOST));
  Serial.println("DATOS: " + data);
  Serial.println("----------------- Request POST -----------------");
  Serial.println("CODE: " + String(httpsRequest.requestCode));
  Serial.println("PAYLOAD: " + httpsRequest.payload);
  Serial.println("------------------------------------------------");
  
  delay(30000); // Delay entre tomas
}
/*********************************************************************************************/

/****************************************** Funciones ****************************************/
/*
 * Función de envio de datos a un HOST
 * 
 * Esta función envia datos al host con url por el puerto
 * via POST y devuelve el request.
 */
bool sendDataPOST(const char *host, const String &url, const int port, const String &data, https_request_t *httpsRequest)
{
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
/*********************************************************************************************/
