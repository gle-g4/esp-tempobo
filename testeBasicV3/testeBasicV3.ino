/*
 * TempoBô - Sistema de Monitoramento Meteorológico
 * Versão 4.2 
 */
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

// ============================================
// CONFIGURAÇÕES DE REDE E FIREBASE
// ============================================
#define WIFI_SSID "INSIRA_SEU_WIFI"
#define WIFI_PASSWORD "INSIRA_SENHA_WIFI"
#define API_KEY "INSIRA_SUA_API_KEY"
#define DATABASE_URL "https://INSIRA_SUA_URL.firebaseio.com/"
#define USER_EMAIL "INSIRA_SEU_EMAIL_DO_FIREBASE"
#define USER_PASSWORD "INSIRA_SUA_SENHA_DO_FIREBASE"
#define ROOT_NODE "INSIRA_SEU_NO_RAIZ"


// ============================================
// SENSOR BMP280
// ============================================
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bmp;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

struct DadosBMP {
  float temperatura;
  float pressao;
  float altitude;
  bool valido;
};

// ============================================
// SENSOR DHT11
// ============================================
#include "DHT.h"
#define DHTPIN D3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

struct DadosDHT {
  float temperaturaDHT;
  float umidadeDHT;
  bool valido;
};

// ============================================
// SENSOR DE CHUVA
// ============================================
int pin_chuva_digital = D0;
int pin_chuva_analog = A0;

// ============================================
// DISPLAY OLED (Barramento I2C #1)
// ============================================
SSD1306Wire display(0x3c, D2, D1); // SDA=D2, SCL=D1
OLEDDisplayUi ui(&display);

// ============================================
// FIREBASE
// ============================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ============================================
// VARIÁVEIS GLOBAIS
// ============================================
unsigned long lastFirebaseUpdate = 0;
const unsigned long firebaseInterval = 60000;
unsigned long currentMillis = 0;
bool firebaseReady = false;
int tentativasReconexao = 0;
const int MAX_TENTATIVAS = 5;

// Variáveis dos sensores
float temperaturaAtual = 0.0;
float pressaoAtual = 0.0;
float umidadeAtual = 0.0;
float altitudeAtual = 0.0; // ✅ NOVO: Armazena altitude
int chuvaAtual = 1024;

// Strings formatadas para exibição
String temperaturaStr = "Lendo...";
String pressaoStr = "Lendo...";
String umidadeStr = "Lendo...";
String altitudeStr = "Lendo..."; // ✅ NOVO: String formatada da altitude
String chuvaStr = "Lendo...";

// ============================================
// FUNÇÕES DE VALIDAÇÃO
// ============================================
bool validarLeitura(float valor, float minimo, float maximo) {
  return !isnan(valor) && !isinf(valor) && valor >= minimo && valor <= maximo;
}

float sanitizarValor(float valor, float padrao = 0.0) {
  if (isnan(valor) || isinf(valor)) {
    Serial.println("⚠️ Valor inválido detectado, usando padrão");
    return padrao;
  }
  return valor;
}

// ============================================
// FUNÇÕES DE LEITURA DOS SENSORES
// ============================================
unsigned long long getTimestamp() {
  time_t now = time(nullptr);
  return (unsigned long long)now * 1000ULL;
}

DadosBMP lerBMP280() {
  DadosBMP dados;
  dados.valido = false;
  
  static bool bmpInicializado = false;
  static bool bmpDisponivel = true; // Assume que está disponível
  
  // ✅ CRÍTICO: BMP280 usa barramento I2C separado (D6=SDA, D5=SCL)
  if (!bmpInicializado && bmpDisponivel) {
    Serial.println("🔍 Inicializando BMP280 em D6(SDA)/D5(SCL)...");
    
    // Inicializa barramento I2C secundário para BMP280
    Wire.begin(D6, D5); // SDA=D6, SCL=D5
    delay(100);
    
    // Tenta 0x76 (encontrado no diagnóstico)
    if (bmp.begin(0x76)) {
      Serial.println("✓ BMP280 inicializado em 0x76!");
      bmpInicializado = true;
    } 
    // Tenta 0x77 como fallback
    else if (bmp.begin(0x77)) {
      Serial.println("✓ BMP280 inicializado em 0x77!");
      bmpInicializado = true;
    } 
    else {
      Serial.println("❌ BMP280 não encontrado");
      Serial.println("⚠️ Verifique se está conectado em D6(SDA)/D5(SCL)");
      bmpDisponivel = false;
      dados.temperatura = 0;
      dados.pressao = 0;
      dados.altitude = 0;
      return dados;
    }
  }
  
  // Se não está disponível, retorna dados inválidos
  if (!bmpDisponivel) {
    dados.temperatura = 0;
    dados.pressao = 0;
    dados.altitude = 0;
    return dados;
  }
  
  // Se chegou aqui, sensor está inicializado - faz leitura
  if (bmpInicializado) {
    // ✅ IMPORTANTE: Seleciona barramento do BMP280 antes de ler
    Wire.begin(D6, D5);
    delay(10);
    
    dados.temperatura = bmp.readTemperature();
    dados.pressao = bmp.readPressure();
    dados.altitude = bmp.readAltitude(1013.25);
    
    bool tempValida = validarLeitura(dados.temperatura, -50.0, 100.0);
    bool pressValida = validarLeitura(dados.pressao, 30000.0, 110000.0);
    bool altValida = validarLeitura(dados.altitude, -500.0, 9000.0);
    
    dados.valido = tempValida && pressValida && altValida;
    
    Serial.printf("📡 BMP280: %.2f°C, %.2f hPa, %.2fm %s\n", 
      dados.temperatura, dados.pressao/100.0, dados.altitude, dados.valido ? "✓" : "✗");
    
    // ✅ Volta para o barramento do display
    Wire.begin(D2, D1);
  }
  
  return dados;
}

DadosDHT lerDHT() {
  DadosDHT dados;
  dados.valido = false;
  
  dados.umidadeDHT = dht.readHumidity();
  dados.temperaturaDHT = dht.readTemperature();
  
  bool tempValida = validarLeitura(dados.temperaturaDHT, -50.0, 100.0);
  bool umidValida = validarLeitura(dados.umidadeDHT, 0.0, 100.0);
  
  dados.valido = tempValida && umidValida;
  
  Serial.printf("📡 DHT11: %.2f°C, %.2f%% %s\n", 
    dados.temperaturaDHT, dados.umidadeDHT, dados.valido ? "✓" : "✗");
  
  return dados;
}

int lerSensorChuva() {
  int val_chuva = analogRead(pin_chuva_analog);
  
  if (val_chuva < 0 || val_chuva > 1024) {
    val_chuva = 1024;
  }
  
  Serial.printf("🌧️ Chuva: %d ", val_chuva);
  if (val_chuva < 300) {
    Serial.println("(Intensa)");
  } else if (val_chuva <= 500) {
    Serial.println("(Moderada)");
  } else {
    Serial.println("(Sem chuva)");
  }
  
  return val_chuva;
}

// ✅ Atualiza variáveis globais com dados dos sensores
void atualizarVariaveisDisplay() {
  Serial.println("\n🔄 Lendo sensores...");
  
  // ✅ TIMEOUT: Se travar, não bloqueia para sempre
  unsigned long inicioLeitura = millis();
  const unsigned long TIMEOUT_LEITURA = 5000; // 5 segundos máximo
  
  DadosDHT leituraDHT = lerDHT();
  
  // Verifica timeout
  if (millis() - inicioLeitura > TIMEOUT_LEITURA) {
    Serial.println("⚠️ Timeout ao ler DHT11!");
    leituraDHT.valido = false;
  }
  
  DadosBMP leituraBMP = lerBMP280();
  
  // Verifica timeout
  if (millis() - inicioLeitura > TIMEOUT_LEITURA) {
    Serial.println("⚠️ Timeout ao ler BMP280!");
    leituraBMP.valido = false;
  }
  
  int valorChuva = lerSensorChuva();
  
  // Atualiza variáveis numéricas
  if (leituraDHT.valido) {
    temperaturaAtual = leituraDHT.temperaturaDHT;
    umidadeAtual = leituraDHT.umidadeDHT;
    temperaturaStr = String(temperaturaAtual, 1) + " C";
    umidadeStr = String(umidadeAtual, 0) + " %";
  } else {
    temperaturaStr = "Erro DHT";
    umidadeStr = "Erro DHT";
  }
  
  if (leituraBMP.valido) {
    pressaoAtual = leituraBMP.pressao / 100.0;
    altitudeAtual = leituraBMP.altitude; // ✅ NOVO: Armazena altitude
    pressaoStr = String(pressaoAtual, 2) + " hPa";
    altitudeStr = String(altitudeAtual, 1) + " m"; // ✅ NOVO: Formata altitude
  } else {
    // Se BMP280 não retornou dados válidos
    pressaoAtual = 0;
    altitudeAtual = 0;
    pressaoStr = "Erro BMP";
    altitudeStr = "Erro BMP"; // ✅ NOVO
  }
  
  chuvaAtual = valorChuva;
  
  if (valorChuva < 300) {
    chuvaStr = "Chuva Intensa";
  } else if (valorChuva <= 500) {
    chuvaStr = "Chuva Moderada";
  } else {
    chuvaStr = "Sem Chuva";
  }
  
  Serial.printf("✅ Leitura concluída em %lu ms\n", millis() - inicioLeitura);
}

// ============================================
// FUNÇÕES DE CONEXÃO
// ============================================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 Conectando WiFi");
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi conectado!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("❌ WiFi falhou");
  }
}

void initFirebase() {
  Serial.println("🔥 Configurando Firebase...");
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.println("⏳ Autenticando...");
  
  unsigned long inicio = millis();
  while (!Firebase.ready() && (millis() - inicio < 30000)) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println();
  
  if (Firebase.ready()) {
    Serial.println("✓ Firebase OK!");
    firebaseReady = true;
  } else {
    Serial.println("❌ Firebase falhou");
    firebaseReady = false;
  }
}

void verificarConexaoWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi perdido! Reconectando...");
    WiFi.disconnect();
    delay(1000);
    connectWiFi();
  }
}

// ============================================
// FRAMES DO DISPLAY (UI AUTOMÁTICO)
// ============================================

// Overlay - mostra tempo de execução
void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()/1000) + "s");
}

// Frame 1: Temperatura (DHT11)
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Temperatura:");
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 20 + y, temperaturaStr);
}

// Frame 2: Pressão (BMP280) ✅ REATIVADO
void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Pressao:");
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 20 + y, pressaoStr);
}

// Frame 3: Umidade (DHT11)
void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Umidade:");
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 20 + y, umidadeStr);
}

// Frame 4: Altitude (BMP280) ✅ NOVO
void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Altitude:");
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 20 + y, altitudeStr);
}

// Frame 5: Chuva
void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Status Chuva:");
  display->setFont(ArialMT_Plain_16);
  display->drawStringMaxWidth(0 + x, 20 + y, 128, chuvaStr);
}

// ✅ Registra frames e overlays (agora são 5 frames)
FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5 };
int frameCount = 5;
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

// ============================================
// FIREBASE
// ============================================
void enviarDadosParaFirebase() {
  if (!Firebase.ready()) {
    Serial.println("⚠️ Firebase não pronto");
    return;
  }
  
  Serial.println("📤 Enviando dados...");
  
  float tempDHT = sanitizarValor(temperaturaAtual, 0.0);
  float umidDHT = sanitizarValor(umidadeAtual, 0.0);
  float pressBMP = sanitizarValor(pressaoAtual * 100.0, 101325.0);
  
  unsigned long long timestamp = getTimestamp();
  
  FirebaseJson json;
  json.set("DHT/temperatura", tempDHT);
  json.set("DHT/umidade", umidDHT);
  json.set("BMP280/pressao", pressBMP);
  json.set("BMP280/altitude", altitudeAtual); // ✅ NOVO: Envia altitude para Firebase
  json.set("BMP280/disponivel", pressBMP > 0);
  json.set("chuva", chuvaAtual);
  json.set("timestamp", (double)timestamp);
  
  // ✅ Informações de qualidade dos dados
  json.set("sensores/dht11_ok", !isnan(tempDHT) && tempDHT != 0);
  json.set("sensores/bmp280_ok", pressBMP > 0);
  json.set("sensores/chuva_ok", true);
  
  bool ok1 = Firebase.RTDB.setJSON(&fbdo, "/" ROOT_NODE "/ultimo", &json);
  bool ok2 = Firebase.RTDB.pushJSON(&fbdo, "/" ROOT_NODE "/historico", &json);
  
  if (ok1 && ok2) {
    Serial.println("✓ Dados enviados!");
    Serial.printf("  Temp: %.1f°C | Umid: %.0f%% | Press: %.2f hPa | Alt: %.1fm | Chuva: %d\n", 
      tempDHT, umidDHT, pressBMP/100.0, altitudeAtual, chuvaAtual);
    tentativasReconexao = 0;
  } else {
    Serial.println("✗ Erro ao enviar");
    tentativasReconexao++;
    
    if (tentativasReconexao >= MAX_TENTATIVAS) {
      Serial.println("Reiniciando Firebase...");
      initFirebase();
      tentativasReconexao = 0;
    }
  }
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("   TempoBô v4.2 - Display Corrigido");
  Serial.println("========================================\n");
  
  // Inicializa pinos dos sensores
  pinMode(pin_chuva_digital, INPUT);
  pinMode(pin_chuva_analog, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // ✅ CRÍTICO: Inicializa I2C para Display (Barramento #1)
  Wire.begin(D2, D1); // SDA=D2, SCL=D1 (Display OLED)
  Serial.println("🔌 I2C Barramento #1: SDA=D2, SCL=D1 (Display OLED)");
  
  // ✅ Inicializa display
  display.init();
  display.clear();
  // ✅ INVERTIDO: Agora o display fica de cabeça para baixo
  // Remove a linha abaixo se quiser voltar ao normal
  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  // Mensagem inicial
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, "TempoBo v4.2");
  display.drawString(64, 15, "Iniciando...");
  display.display();
  delay(1000);
  
  // Barra de progresso
  for (int i = 0; i <= 100; i += 5) {
    display.clear();
    display.drawProgressBar(4, 32, 120, 10, i);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 20, String(i) + "%");
    display.display();
    delay(50);
  }
  
  // Conecta WiFi
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Conectando WiFi...");
  display.display();
  
  connectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    display.clear();
    display.drawString(0, 0, "ERRO: WiFi");
    display.display();
    while (true) delay(1000);
  }
  
  display.drawString(0, 15, "WiFi OK!");
  display.display();
  delay(1000);
  
  // Inicializa Firebase
  display.clear();
  display.drawString(0, 0, "Conectando");
  display.drawString(0, 15, "Firebase...");
  display.display();
  
  initFirebase();
  
  // Inicializa sensores
  Serial.println("🔧 Inicializando DHT11...");
  dht.begin();
  delay(2000); // ✅ DHT11 precisa de tempo para estabilizar
  Serial.println("✓ DHT11 pronto");
  
  // ✅ BMP280 será inicializado na primeira leitura (usa barramento separado)
  Serial.println("🔧 BMP280 será inicializado em D6(SDA)/D5(SCL) na primeira leitura");
  
  // Configura NTP
  Serial.println("🕐 Sincronizando NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // ✅ CRUCIAL: Configura UI e inicia
  Serial.println("📺 Configurando display UI...");
  ui.setTargetFPS(30);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.setTimePerFrame(4000); // 4 segundos por frame
  
  // Remove indicadores (bolinhas) na parte inferior
  ui.disableAllIndicators();
  
  ui.init(); // ✅ INICIALIZA A UI
  Serial.println("✓ Display UI pronto");
  
  // Primeira leitura dos sensores
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Lendo sensores...");
  display.drawString(0, 15, "Aguarde...");
  display.display();
  
  Serial.println("\n🔬 Primeira leitura dos sensores:");
  atualizarVariaveisDisplay();
  
  // Mensagem final
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "Sistema OK!");
  display.display();
  delay(2000);
  
  Serial.println("\n✓ Setup completo!\n");
}

// ============================================
// LOOP PRINCIPAL
// ============================================
void loop() {
  currentMillis = millis();
  
  // ✅ CRÍTICO: Atualiza a UI automaticamente
  int remainingTimeBudget = ui.update();
  
  // Verifica WiFi a cada 30s
  static unsigned long ultimaVerificacaoWiFi = 0;
  if (currentMillis - ultimaVerificacaoWiFi > 30000) {
    ultimaVerificacaoWiFi = currentMillis;
    verificarConexaoWiFi();
  }
  
  // Atualiza dados a cada 1 minuto
  if (currentMillis - lastFirebaseUpdate >= firebaseInterval) {
    lastFirebaseUpdate = currentMillis;
    
    Serial.println("\n═══ CICLO DE ATUALIZAÇÃO ═══");
    atualizarVariaveisDisplay();
    enviarDadosParaFirebase();
    Serial.println("═══ FIM DO CICLO ═══\n");
  }
  
  // ✅ Usa o tempo restante para evitar lag
  if (remainingTimeBudget > 0) {
    delay(remainingTimeBudget);
  }
}
