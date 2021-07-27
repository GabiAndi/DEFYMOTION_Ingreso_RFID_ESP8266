/***************************************** Librerías *****************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/

/*********************************************************************************************/

/***************************************** Instancias ****************************************/
// Conexión a la RED
const char googleScriptUrl[] = "script.google.com";
const uint32_t httpsPort = 443;
const char googleScriptUrlContent[] = "script.googleusercontent.com";

// Configuración de la RED
const char ssid[] = "TPLINK_24G";
const char password[] = "BasexB1Au1974*";

IPAddress staticIP(192, 168, 1, 120);         // IP estática
IPAddress gateway(192, 168, 1, 1);            // Dirección de gateway
IPAddress subnet(255, 255, 255, 0);           // Mascara de red
IPAddress primaryDNS(8, 8, 8, 8);             // Dirección DNS 1
IPAddress secondaryDNS(8, 8, 4, 4);           // Dirección DNS 2

// Google
WiFiClientSecure client;

const String GOOGLE_SCRIPT_ID = "AKfycbwpNdDrvYaH2NDdEJJYZyEvE_QygqanishV76WQK6NBh8E7p84jxqP1xf8BcHtmW20N";
const String UNIT_NAME = "headquarter";

String nombre = "";
String estado = "";
String hora = "";
String listado = "listado";

String data = ""; // Datos que se enviaran a Google

void handleDataFromGoogle(String data); // Funcion de envio de datos
String sendData(String params, char* domain); // Funcion que prepara el paquete de datos a Google
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
  WiFi.begin(ssid, password); // Se conecta a la RED

  Serial.println("Conectando");

  // Se espera a que este realizada la conexión
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);  // Este delay SIEMPRE debe estar, de otra manera, se satura de consultas al ESP con WiFi.status()
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  
  delay(100); // Delay para en inicio
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Test de conexión
  data = "";
  
  for (uint8_t i = 0xA0 ; i < 4 ; i++)
  {
    data += String(i, HEX);
  }
  
  data = sendData("id=" + UNIT_NAME + "&uid=" + data, NULL); // Paquete de datos

  Serial.println("Enviando: " + data);
  
  //handleDataFromGoogle(data); // Envio de datos a Google
  
  delay(60000); // Delay entre tomas
}
/*********************************************************************************************/

/****************************************** Funciones ****************************************/
void handleDataFromGoogle(String data)
{
  int ind = data.indexOf(":");
  String access = data.substring(0, ind);
  estado = access;
  int nextInd = data.indexOf(":", ind + 1);
  String name = data.substring(ind + 1, nextInd);
  nombre = name;
  String fechayhora = data.substring(nextInd + 1, data.length());
  int indFecha = fechayhora.indexOf(" ");
  int ind2 = fechayhora.indexOf(";");
  hora = fechayhora.substring(indFecha + 1, ind2);
  listado = fechayhora.substring(ind2 + 1, fechayhora.length());

  Serial.println("-->Ingresos<--");
  
  if (listado.length() > 1)
  {
    int posfin = listado.length() - 1;
    int pos1 = 0;
    int pos2 = 0;
    String palabra = "";
    
    for (uint8_t i = 0; i < 6; i++)
    {
      pos1 = listado.indexOf("-", pos2);
      palabra = listado.substring(pos2, pos1);
      Serial.println(palabra);
      pos2 = pos1 + 1;
      
      if (pos2 >= posfin)
      {
        break;
      }
    }
  }
}

String sendData(String params, char *domain)
{
  // Los scrips de Google requieren dos respuestas
  bool needRedir = false;
  
  // Si el dominio ingresado es nulo
  if (domain == NULL)
  {
    domain = (char *)(googleScriptUrl);
    needRedir = true;
    params = "/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + params;
  }

  // Configuración del cliente
  String result = "";
  client.setInsecure();

  if (!client.connect(googleScriptUrl, httpsPort))
  {
    return "";
  }

  client.print(String("GET ") + params + " HTTP/1.1\r\n" +
               "Host: " + domain + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected())
  {
    String line = client.readStringUntil('\n');

    if (needRedir)
    {
      int ind = line.indexOf("/macros/echo?user");
      
      if (ind > 0)
      {
        line = line.substring(ind);
        ind = line.lastIndexOf("\r");
        line = line.substring(0, ind);
        result = line;
      }
    }

    if (line == "\r")
    {
      break;
    }
  }
  
  while (client.available())
  {
    String line = client.readStringUntil('\n');
    
    if (!needRedir)
    {
      if (line.length() > 5)
      {
        result = line;
      }
    }
  }
  
  if (needRedir)
  {
    return sendData(result, (char *)(googleScriptUrlContent));
  }
  
  else
  {
    return result;
  }
}
/*********************************************************************************************/
