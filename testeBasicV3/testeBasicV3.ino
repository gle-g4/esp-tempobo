#include <Wire.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// ==== CONFIGURAÇÕES DE REDE E FIREBASE ==== //
#define WIFI_SSID "BRATTIO"
#define WIFI_PASSWORD "Adrian@68"
#define API_KEY "AIzaSyC_98m8YaKIe3QHXtNH39B8TVGWTE-vI4U"
#define DATABASE_URL "https://esp-projeto-5d4b2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "joao.souza.111867@a.fecaf.com.br"
#define USER_PASSWORD "Ps164907@"
#define ROOT_NODE "ESP-Projeto-V2"

//----SENSOR D BMP280 ----//
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bmp;

// Configuração NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // GMT-3 (Brasília)
const int daylightOffset_sec = 0;       // Sem horário de verão

//Struct para armazenar os dados do BMP280
struct DadosBMP {
  float temperatura;
  float pressao;
  float altitude;
};

// ==== VARIÁVEIS ==== //
unsigned long lastFirebaseUpdate = 0;
const unsigned long firebaseInterval = 60000; // 1 minuto

int currentScreen = 0;

bool estado = 0; // true ou false
float pressao = 0;   // Ex.: 50.00 a 100.00
float temperatura=0;
float umidade = 0;     // 0 a 999
String mensagem = "";

// ==== Sensor de Temperatura e Humidade ==== //
#include "DHT.h"  // Biblioteca do sensor DHT
#define DHTPIN D3       // Pino conectado ao DHT
#define DHTTYPE DHT11   // Modelo do sensor: DHT11
DHT dht(DHTPIN, DHTTYPE); 
//Estrutura para armazenar os dados de Temperatura e Humidade do DHT11
struct DadosDHT {
  float temperaturaDHT;
  float umidadeDHT;
};

// ==== Sensor de Chuva ==== //
int pin_chuva_digital = D0; // Pino digital ligado ao sensor
int pin_chuva_analog = A0; // Pino analógico ligado ao sensor

// ==== DISPLAY ==== //
SSD1306Wire display(0x3c, D2, D3);
OLEDDisplayUi ui(&display);

// ==== FIREBASE ==== //
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ==== OVERLAY ==== //
void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()/1000) + "s");
}
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

// ==== FUNÇÕES ==== //

// Função para pegar timestamp Unix em millisegundos
  unsigned long long getTimestamp() {
  time_t now = time(nullptr);
  return (unsigned long long)now * 1000ULL; // Multiplica por 1000 para millisegundos
}

//Sensor de Pressao e Altitude BMP
DadosBMP lerBMP280() {
  DadosBMP dados;

  // Inicializa o I2C nos pinos desejados
  Wire.begin(D6, D5); // SDA = D6, SCL = D5

  // Inicializa sensor
  if (!bmp.begin(0x76)) {
    Serial.println("Não achou no 0x76, tentando 0x77");
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 não encontrado!");
      dados.temperatura = 0;
      dados.pressao = 0;
      dados.altitude = 0;
      return dados;  // retorna valores inválidos
    }
  }

  // Faz leitura
  dados.temperatura = bmp.readTemperature();
  dados.pressao = bmp.readPressure();
  dados.altitude = bmp.readAltitude(1013.25);
  Serial.println("📡 Leitura do BMP:");
  Serial.print("Pressao: ");
  Serial.println(dados.pressao);
  Serial.print("Temperatura: ");
  Serial.println(dados.temperatura);
  Serial.print("Altitude: ");
  Serial.println(dados.altitude);  
  Serial.println("--------------------------");

  return dados;
}


//Sensor de Humidade e Temperatura
DadosDHT lerDHT() {
  DadosDHT dados;
  dados.umidadeDHT = dht.readHumidity();
  dados.temperaturaDHT = dht.readTemperature();

  // Exibe no Serial para depuração (opcional)
  Serial.println("📡 Leitura do DHT:");
  Serial.print("Umidade: ");
  Serial.println(dados.umidadeDHT);
  Serial.print("Temperatura: ");
  Serial.println(dados.temperaturaDHT);
  Serial.println("--------------------------");

  return dados;  // Retorna a struct com os dois valores
}

// Sensor de Chuva
int lerSensorChuva() {
  int val_chuva = analogRead(pin_chuva_analog);
  Serial.print("Valor do sensor de chuva: ");
  Serial.println(val_chuva);
  return val_chuva;
}

// Conecta WiFi
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

// Inicializa Firebase
void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  Serial.printf("Firebase conectado! (v%s)\n", FIREBASE_CLIENT_VERSION);
}

// Inicializa display
void initDisplay() {
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  ui.setTargetFPS(24);
}

// Atualiza dados do Firebase
void atualizarDadosDoFirebase() {
  String caminho = "/" + String(ROOT_NODE) + "/ultimo";
  Serial.println("Lendo dados do Firebase: " + caminho);

  if (Firebase.ready() && Firebase.RTDB.getJSON(&fbdo, caminho.c_str())) {
    FirebaseJson json = fbdo.to<FirebaseJson>();
    FirebaseJsonData result;

    if (json.get(result, "bool")) estado = result.to<bool>();
    if (json.get(result, "float")) temperatura = result.to<float>();
    if (json.get(result, "double")) pressao = result.to<float>();
    if (json.get(result, "int")) umidade = result.to<float>();
    if (json.get(result, "string")) mensagem = result.to<String>();

    /*Serial.println("=== DADOS RECEBIDOS ===");
    Serial.printf("Temperatura: %.2f\n", temperatura);
    Serial.printf("Pressão: %.2f\n", pressao);
    Serial.printf("Altitude: %.2f\n", altitude);
    Serial.printf("Estado: %s\n", estado ? "Ativo" : "Inativo");
    Serial.printf("Mensagem: %s\n", mensagem.c_str());*/
  } else {
    Serial.printf("Erro ao ler dados: %s\n", fbdo.errorReason().c_str());
  }
}

// --- FRAMES DE EXIBIÇÃO --- //
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Estado:");
  display->drawString(0 + x, 20 + y, estado ? "Ativo" : "Inativo");
  display->display();
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Temperatura:");
  display->drawString(0 + x, 20 + y, String(temperatura,1) + " C");
  display->display();
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Pressao:");
  display->drawString(0 + x, 20 + y, String(pressao,1) + " hPa");
  display->display();
}

void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Umidade:");
  display->drawString(0 + x, 20 + y, String(umidade, 1) + " %");
  display->display();
}

void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Mensagem:");
  display->drawStringMaxWidth(0 + x, 20 + y, 128, mensagem);
  display->display();
}

FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5 };
int frameCount = 5;

// --- Envia dados para Firebase (simulado) ---
void enviarDadosParaFirebase() {
  // Lê os sensores
  DadosDHT leituraDHT = lerDHT();
  DadosBMP leituraBMP = lerBMP280();
  
  // Evita valores inválidos
  if (isnan(leituraBMP.pressao)) leituraBMP.pressao = 0;
  if (isnan(leituraBMP.altitude)) leituraBMP.altitude = 0;

  // Pega timestamp real
  unsigned long long timestamp = getTimestamp();

  // Cria o JSON principal
  FirebaseJson json;
  json.set("DHT/temperatura", leituraDHT.temperaturaDHT);
  json.set("DHT/umidade", leituraDHT.umidadeDHT);
  json.set("BMP280/pressao", leituraBMP.pressao);
  json.set("BMP280/altitude", leituraBMP.altitude);
  json.set("chuva", lerSensorChuva());
  json.set("timestamp", (double)timestamp);

  // Envia para /ultimo (últimos valores)
  if (Firebase.RTDB.setJSON(&fbdo, "/" ROOT_NODE "/ultimo", &json)) {
    Serial.println("Últimos dados enviados com sucesso!");
  } else {
    Serial.printf("Erro ao enviar /ultimo: %s\n", fbdo.errorReason().c_str());
  }

  // Envia para /historico (novo nó com timestamp)
  if (Firebase.RTDB.pushJSON(&fbdo, "/" ROOT_NODE "/historico", &json)) {
    Serial.println("Histórico atualizado com sucesso!");
  } else {
    Serial.printf("Erro ao enviar /historico: %s\n", fbdo.errorReason().c_str());
  }
}


// ================== SETUP / LOOP ================== //
void setup() {
  Serial.begin(115200);
  Wire.begin(D1, D2);

  initDisplay();
  connectWiFi();
  initFirebase();

  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.init();

  //Sensor de chuva
  pinMode(pin_chuva_digital, INPUT);
  pinMode(pin_chuva_analog, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  //Sensor de Humidade e Temeperatura
  dht.begin();

  // Configura NTP para pegar hora real
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando com NTP...");
  
  // Aguarda sincronização (importante!)
  time_t now = time(nullptr);
  int tentativas = 0;
  while (now < 100000 && tentativas < 20) {
    Serial.print(".");
    delay(500);
    now = time(nullptr);
    tentativas++;
  }
  
  if (tentativas < 20) {
    Serial.println("\nHora sincronizada com sucesso!");
    Serial.print("Data/Hora atual: ");
    Serial.println(ctime(&now));
  } else {
    Serial.println("\nFalha ao sincronizar hora. Continuando mesmo assim...");
  }

}

const unsigned long telaInterval = 4000; // 4 segundos por tela
unsigned long lastScreenChange = 0;

// Loop atualizado
void loop() {
  unsigned long currentMillis = millis();

  // 1️⃣ Envia dados a cada 1 minuto
  if (currentMillis - lastFirebaseUpdate > firebaseInterval) {
    lastFirebaseUpdate = currentMillis;

    enviarDadosParaFirebase();   // envia
    atualizarDadosDoFirebase();  // lê
    currentScreen = 0;           // reinicia sequência de telas
    lastScreenChange = currentMillis;
  }

  // 2️⃣ Atualiza telas
  if (currentMillis - lastScreenChange > telaInterval) {
    lastScreenChange = currentMillis;
    // Mostra próxima tela
    FrameCallback f = frames[currentScreen];
    f(&display, nullptr, 0, 0);
    currentScreen = (currentScreen + 1) % frameCount;
  }
}
