/***************************************** Librerías *****************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <SPI.h>
#include <MFRC522.h>

#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
/*********************************************************************************************/

/**************************************** Definiciones ***************************************/
#define SS_PIN        D4  // Pin SDA
#define RST_PIN       D3  // Pin RESET
/*********************************************************************************************/

/***************************************** Instancias ****************************************/
// Conexión a la RED
MFRC522 mfrc522(SS_PIN, RST_PIN);

const char googleScriptUrl[] = "script.google.com";
const uint32_t httpsPort = 443;
const char googleScriptUrlContent[] = "script.googleusercontent.com";

// Configuración de la RED
const char* ssid = "DEFYMOTION";
const char* password = "laquequieras";

IPAddress staticIP(192, 168, 1, 120);         // IP estática
IPAddress gateway(192, 168, 1, 1);            // Dirección de gateway
IPAddress subnet(255, 255, 255, 0);           // Mascara de red
IPAddress primaryDNS(8, 8, 8, 8);             // Dirección DNS 1
IPAddress secondaryDNS(8, 8, 4, 4);           // Dirección DNS 2

// Google
WiFiClientSecure client;

const String GOOGLE_SCRIPT_ID = "AKfycbzP3ESddK27dx7BUvWEo74vXz3uWSFlrDsImJW6v6N7YC5oMz2GxHFs1SSRY2LtyC-4zA";
const String UNIT_NAME = "headquarter";

String nombre = "";
String estado = "";
String hora = "";
String listado = "listado";

String data = ""; // Datos que se enviaran a Google

void handleDataFromGoogle(String data); // Funcion de envio de datos
String sendData(String params, char* domain); // Funcion que prepara el paquete de datos a Google

// Display
Adafruit_PCD8544 display = Adafruit_PCD8544(D10, D9, D2, D1, D0);   // Adafruit_PCD8544(SCLK, DIN, DC, CS, RST)
/*********************************************************************************************/

/************************************ Configuración inicial **********************************/
void setup()
{
  // Pines GPIO
  pinMode(LED_BUILTIN, OUTPUT);

  // Display LCD
  display.begin();

  display.setContrast(60);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);

  display.println("DEFYMOTION");

  display.display();

  // Configuración de la conexión
  WiFi.mode(WIFI_STA);  // Modo estación
  WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS); // Configuración de la conexión
  WiFi.begin(ssid, password); // Se conecta a la RED

  // Se espera a que este realizada la conexión
  while (WiFi.status() != WL_CONNECTED);

  // Configuración del RF
  SPI.begin();  // Inicio del SPI
  mfrc522.PCD_Init(); // Iniciamos el lector RF
  
  delay(10); // Delay para en inicio del RF
}
/*********************************************************************************************/

/*************************************** Bucle principal *************************************/
void loop()
{
  // Si una nueva tarjeta es detectada
  if (mfrc522.PICC_IsNewCardPresent())
  {
    //Seleccionamos una tarjeta
    if (mfrc522.PICC_ReadCardSerial())
    {
      // Se convierte los bytes leidos de ID a un string
      data = "";
      
      for (uint8_t i = 0 ; i < 4 ; i++)
      {
        data += String(mfrc522.uid.uidByte[i], HEX);
      }
      
      data = sendData("id=" + UNIT_NAME + "&uid=" + data, NULL); // Paquete de datos
      
      handleDataFromGoogle(data); // Envio de datos a Google
      mfrc522.PICC_HaltA(); // Se finaliza la lectura de la tarjeta actual
    }
  }
  
  delay(250); // Delay entre tomas
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

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(estado);
  display.println(nombre);
  display.println(hora);
  display.display();
  
  delay(4000);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("-->Ingresos<--");
  
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
      display.println(palabra);
      pos2 = pos1 + 1;
      
      if (pos2 >= posfin)
      {
        break;
      }
    }
  }
  
  display.display();
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
