//GRUPO 8 5°LB Dagotto Alfiz Schajris
#define ENABLE_USER_AUTH //Le dice a firebase qué productos vamos a usar
#define ENABLE_DATABASE

//Incluyo librerías
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "time.h"
#include <U8g2lib.h>

// Network and Firebase credentials
#define WIFI_SSID "ORT-IoT"
#define WIFI_PASSWORD "NuevaIOT$25"

#define Web_API_KEY "AIzaSyD9LnTpwtYE-DhzlKsF6-k4Q_UhzxoF0UI" //La API del proyecto
#define DATABASE_URL "https://tp-firebase-6a1b6-default-rtdb.firebaseio.com/" //Link a la base de datos
#define USER_EMAIL "48386836@est.ort.edu.ar" //El mail del usuario
#define USER_PASS "*********" //Mi contraseña (Normalmente no son asteriscos)

//Pines de botones
#define PIN_BOTON1 34
#define PIN_BOTON2 35

#define DHTPIN 23      // pin del dht11
#define DHTTYPE DHT11  // tipo de dht (hay otros)

DHT dht(DHTPIN, DHTTYPE); // defino el DHT (el sensor necesita ser iniciado con esta función) (Más abajo la pantallita es iniciada con una función similar)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);  //defino la pantallita

#define PULSADO LOW
#define N_PULSADO !PULSADO

// Defino estados de nuestra máquina de estados
#define PANTALLA1 1
#define ESTADO_CONFIRMACION1 2
#define PANTALLA2 3
#define ESTADO_CONFIRMACION2 4
#define SUBIR_INTERV 7
#define BAJAR_INTERV 6

// Función definida abajo
void processData(AsyncResult &aResult);

// Autenticación, Revisa si en el proyecto con esa API hay un usuario con estos datos
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

// Firebase components
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

//Dos delays sin bloqueo, uno para el envío de los datos y el otro para la máquina de estado

// Timer variables for sending data every 30 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000;  // 30 seconds in milliseconds

unsigned long TiempoUltimoCambio = 0; //Variable que almacenará millis. Explicada mejor abajo
const long INTERVALO = 1000; //Cuanto tiempo dura el delay sin bloqueo
unsigned long TiempoAhora; //Guarda los milisegundos desde que se ejecutó el programa

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String tempPath = "/temperature";
String timePath = "/timestamp";

//Variables que usamos
bool cambioHecho = LOW; //cambioHecho se usa para que al apretar cualquier boton, el valor aumente 1 sola vez
int CicloActual = 30000; //Ciclo actual. Al principio vale 30 segundos
const int Ciclo_MINIMO = 30000; //Intervalo mínimo
float temperature = 0; //Temperatura inicializada en el 0

// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;

const char *ntpServer = "pool.ntp.org";

// Create JSON objects for storing data
object_t jsonData, obj1, obj2;
JsonWriter writer;

int estadoActual = PANTALLA1;  // inicia en PANTALLA1

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

void setup() {
  Serial.begin(115200);

  // Pines configurados como entradas normales (sin pull-up porque 34 y 35 no lo soportan)
  pinMode(PIN_BOTON1, INPUT);
  pinMode(PIN_BOTON2, INPUT);

  initWiFi();
  configTime(0, 0, ntpServer);

  // Configure SSL client
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  // Initialize Firebase
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  dht.begin();   // inicializo el dht          (No confundir estas dos funciones de inicialización con las definiciones arriba)
  u8g2.begin();  //inicializo la pantallita

}

void loop() {
  // Maintain authentication and async tasks
  TiempoAhora = millis();
  app.loop();
  
  // Get sensor readings
  temperature = dht.readTemperature(); //Temperatura medida por el DHT

  bool lecturaBoton1 = digitalRead(PIN_BOTON1);
  bool lecturaBoton2 = digitalRead(PIN_BOTON2);

  // Check if authentication is ready
  if (app.ready()) {

    // Periodic data sending every 30 seconds
    unsigned long currentTime = millis();


    if (currentTime - lastSendTime >= CicloActual) {
      // Update the last send time
      lastSendTime = currentTime;

      uid = app.getUid().c_str();

      // Update database path
      databasePath = "/UsersData/" + uid + "/readings";

      //Get current timestamp
      timestamp = getTime();
      Serial.print("time: ");
      Serial.println(timestamp);

      parentPath = databasePath + "/" + String(timestamp);

      // Create a JSON object with the data
      writer.create(obj1, tempPath, temperature);
      writer.create(obj2, timePath, timestamp);
      writer.join(jsonData, 2, obj1, obj2);

      Database.set<object_t>(aClient, parentPath, jsonData, processData, "RTDB_Send_Data");
    }
  }
    //Inicializo máquina de estados
  switch (estadoActual) { //Empieza en el estado que estadoActual tiene como valor
    case PANTALLA1: //Pantalla 1, muestra la temperatura
      if (TiempoAhora - TiempoUltimoCambio >= INTERVALO) { //Delay sin bloqueo
        TiempoUltimoCambio = millis(); //actualizo el tiempo


        char bufferTemperatura[10]; //Reserva espacios en el búfer para la variable de temperatura
       
        sprintf(bufferTemperatura, "%.2f", temperature); //Guarda la variable temperatura en los espacios reservados para bufferTemperatura


        u8g2.clearBuffer(); //Borra lo almacenado en el búfer anteriormente
        u8g2.setFont(u8g2_font_helvB10_tf); //La fuente del texto del oled
        u8g2.drawStr(10, 30, "Temperatura");
        u8g2.drawStr(10, 15, bufferTemperatura);
        u8g2.sendBuffer(); //Envía los datos guardados en el búfer a la pantalla
      }


      if (lecturaBoton1 == PULSADO && lecturaBoton2 == PULSADO) { //Si los dos botones son pulsados, se sale de pantalla 1 y se entra a un estado de confirmación (Línea 123)
        estadoActual = ESTADO_CONFIRMACION1;
      }
      break;




    case ESTADO_CONFIRMACION1:
      if (lecturaBoton1 == N_PULSADO && lecturaBoton2 == N_PULSADO) { //Si los dos botones se dejan de pulsar, se entra a la pantalla 2 (Línea 130)
        estadoActual = PANTALLA2;
      }
      break;




    case PANTALLA2:
      if (TiempoAhora - TiempoUltimoCambio >= INTERVALO)  ///delay sin bloqueo
      {
        TiempoUltimoCambio = millis();  /// importante actualizar el tiempo
        char bufferCiclo[10]; //Reserva espacios en el búfer para la variable de ciclo


        sprintf(bufferCiclo, "%d s", CicloActual / 1000); //Guarda la variable CicloActual en los espacios reservados para bufferCiclo


        u8g2.clearBuffer(); //Véase línea 106 a 114
        u8g2.setFont(u8g2_font_helvB10_tf);
        u8g2.drawStr(5, 15, "Intervalo:");
        u8g2.drawStr(5, 30, bufferCiclo);
        u8g2.sendBuffer();
      }


      if (lecturaBoton1 == PULSADO && lecturaBoton2 == PULSADO) { //Si los dos botones son pulsados, se sale de pantalla 2 y se entra a un estado de confirmación (Línea 162)
        estadoActual = ESTADO_CONFIRMACION2;
      }


      if (lecturaBoton1 == PULSADO) { //Si solo el boton 1 es pulsado, entra al estado de bajar ciclo (Línea 192) y cambioHecho devuelve True
        cambioHecho = HIGH;
        estadoActual = BAJAR_INTERV;
        //Serial.println(cambioHecho);
      }
      if (lecturaBoton2 == PULSADO) { //Si solo el boton 2 es pulsado, entra al estado de subir ciclo (Línea 169) y cambioHecho devuelve True
        cambioHecho = HIGH;
        estadoActual = SUBIR_INTERV;
        //Serial.println(cambioHecho);
      }
      break;




    case ESTADO_CONFIRMACION2:
      if (lecturaBoton1 == N_PULSADO && lecturaBoton2 == N_PULSADO) { //Si los dos botones se dejan de pulsar, se entra a la pantalla 1 (Línea 96)
        estadoActual = PANTALLA1;
      }
      break;


    case SUBIR_INTERV:
      if (lecturaBoton1 == PULSADO) { //Si se presiona el otro botón, vamos a confirmación
        estadoActual = ESTADO_CONFIRMACION2;
      }

      if (lecturaBoton2 == N_PULSADO) { //Si se deja de presionar el boton 2
        if (cambioHecho == HIGH) { //Si el cambio aún no se aplicó
            CicloActual = CicloActual + 30000; //Incrementamos el intervalo
            cambioHecho = LOW;
        }
        estadoActual = PANTALLA2; //Volvemos a pantalla 2
      }
      break;




    case BAJAR_INTERV:
      if (lecturaBoton2 == PULSADO) { //Si el boton 2 es pulsado, se entra al estado de confirmación 2 (para llegar a BAJAR_INTERV se necesitaba pulsar el boton 1)
        estadoActual = ESTADO_CONFIRMACION2;
      }


      if (lecturaBoton1 == N_PULSADO) { //Si se deja de presionar el boton 1 sin que se presione el boton 2
        if (CicloActual == Ciclo_MINIMO) {  //Si el ciclo es igual a 30000, significa que ya no debería bajar más, por lo que es rechazado
          Serial.println ("El intervalo de guardado no puede superar los 30 segundos.");
          cambioHecho = LOW;
          //Serial.println(CicloActual);
        }


        if (cambioHecho == HIGH) { //Si cambio hecho sigue siendo HIGH, el ciclo disminuye en 30000, y se imprime en el monitor serial
          CicloActual = CicloActual - 30000;
          cambioHecho = LOW;
        }
        estadoActual = PANTALLA2;
      }
      break;
    
  }
}

void processData(AsyncResult &aResult) { //Función que se encarga de comunicar nuevos resultadoa a la database sin mantener conexión constante.
  if (!aResult.isResult()) //Si no hay nada que procesar:
    return; //Sale de la función

  if (aResult.isEvent()) //Si sucede un evento, Te da info
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isDebug())//Si es un debug
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

  if (aResult.available())
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());


}