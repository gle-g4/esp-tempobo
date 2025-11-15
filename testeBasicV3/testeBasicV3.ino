#include <Wire.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

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
//Estrutura para armazenar os dados de Temperatura e Humidade
struct DadosDHT {
  float temperaturaDHT;
  float umidadeDHT;
};

// ==== Sensor de Chuva ==== //
int pin_chuva_digital = D0; // Pino digital ligado ao sensor
int pin_chuva_analog = A0; // Pino analógico ligado ao sensor

// ==== DISPLAY ==== //
SSD1306Wire display(0x3c, SDA, SCL);
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
String lerSensorChuva() {
  int val_a = analogRead(pin_chuva_analog);
  String mensagem = "";

  if (val_a < 300) {
    mensagem = "Chuva Intensa";
  } 
  else if (val_a <= 500 && val_a >= 300) {
    mensagem = "Chuva Moderada";
  } 
  else {
    mensagem = "Sem Chuva";
  }

  // Exibe no Serial Monitor (opcional)
  Serial.print("Valor do sensor de chuva: ");
  Serial.println(val_a);
  Serial.print("Condição: ");
  Serial.println(mensagem);
  Serial.println("-----------------------------");

  // Retorna a mensagem
  return mensagem;
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
  String caminhoUltimo = "/" + String(ROOT_NODE) + "/ultimo";
  String caminhoHistorico = "/" + String(ROOT_NODE) + "/historico";
  

  estado = random(0, 2) == 0; // true ou false

  DadosDHT leitura = lerDHT();
  temperatura = leitura.temperaturaDHT; // Ex.: 100.00 a 200.00
  pressao = random(5000, 10000) / 100.0;   // Ex.: 50.00 a 100.00
  umidade = leitura.umidadeDHT;                  // 0 a 999
  mensagem = lerSensorChuva();

  FirebaseJson json;
  json.set("bool", estado);
  json.set("float", temperatura);
  json.set("double", pressao);
  json.set("int", umidade);
  json.set("string", mensagem);
  json.set("timestamp/.sv", "timestamp"); // Timestamp do servidor


  if (Firebase.RTDB.setJSON(&fbdo, caminhoUltimo.c_str(), &json)) {
    Serial.println("Dados enviados para Firebase com sucesso!");

    /*Serial.println("=== DADOS ENVIADOS ===");
    Serial.printf("Temperatura: %.2f\n", temperatura);
    Serial.printf("Pressão: %.2f\n", pressao);
    Serial.printf("Altitude: %.2f\n", altitude);
    Serial.printf("Estado: %s\n", estado ? "Ativo" : "Inativo");
    Serial.printf("Mensagem: %s\n", mensagem.c_str());*/
  } else {
    Serial.printf("Erro ao enviar dados: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.pushJSON(&fbdo, caminhoHistorico.c_str(), &json)) {
    Serial.println("Historico do Firebase atualizado com sucesso!");
  }

}

// ================== SETUP / LOOP ================== //
void setup() {
  Serial.begin(115200);
  Wire.begin();

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
