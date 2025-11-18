/*
 * TempoBô - Sistema de Monitoramento Meteorológico
 * Versão 4.0 - COM SEGURANÇA IMPLEMENTADA
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
#define WIFI_SSID "BRATTIO"
#define WIFI_PASSWORD "Adrian@68"
#define API_KEY "AIzaSyC_98m8YaKIe3QHXtNH39B8TVGWTE-vI4U"
#define DATABASE_URL "https://esp-projeto-5d4b2-default-rtdb.firebaseio.com/"

// ⚠️ IMPORTANTE: Use as credenciais do usuário que você criou no Firebase Authentication
// Se ainda não criou, vá em: Firebase Console > Authentication > Users > Add User
#define USER_EMAIL "esp8266@tempobo.com"      // Email de autenticação
#define USER_PASSWORD "TempoBo2025Seguro!"  // Senha forte (mínimo 6 caracteres)

#define ROOT_NODE "ESP-Projeto-V2"

// ============================================
// SENSOR BMP280
// ============================================
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bmp;

// Configuração NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // GMT-3 (Brasília)
const int daylightOffset_sec = 0;       // Sem horário de verão

// Struct para armazenar os dados do BMP280
struct DadosBMP {
  float temperatura;
  float pressao;
  float altitude;
  bool valido;  // ✅ NOVO: indica se a leitura foi bem-sucedida
};

// ============================================
// SENSOR DHT11
// ============================================
#include "DHT.h"
#define DHTPIN D3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Estrutura para armazenar os dados do DHT11
struct DadosDHT {
  float temperaturaDHT;
  float umidadeDHT;
  bool valido;  // ✅ NOVO: indica se a leitura foi bem-sucedida
};

// ============================================
// SENSOR DE CHUVA
// ============================================
int pin_chuva_digital = D0;
int pin_chuva_analog = A0;

// ============================================
// DISPLAY OLED
// ============================================
SSD1306Wire display(0x3c, D2, D3);
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
const unsigned long firebaseInterval = 60000; // 1 minuto

int currentScreen = 0;
bool firebaseReady = false;  // ✅ NOVO: controla estado do Firebase
int tentativasReconexao = 0;  // ✅ NOVO: contador de falhas
const int MAX_TENTATIVAS = 5;

// Variáveis para exibição no display
bool estado = false;
float pressao = 0;
float temperatura = 0;
float umidade = 0;
String mensagem = "Aguardando...";

// ============================================
// FUNÇÕES DE VALIDAÇÃO (SEGURANÇA)
// ============================================

/**
 * ✅ NOVO: Valida se um valor está dentro do range esperado
 */
bool validarLeitura(float valor, float minimo, float maximo) {
  return !isnan(valor) && !isinf(valor) && valor >= minimo && valor <= maximo;
}

/**
 * ✅ NOVO: Sanitiza valores para evitar NaN/Inf no Firebase
 */
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

/**
 * Função para pegar timestamp Unix em milissegundos
 */
unsigned long long getTimestamp() {
  time_t now = time(nullptr);
  return (unsigned long long)now * 1000ULL;
}

/**
 * ✅ MELHORADO: Lê BMP280 com validação
 */
DadosBMP lerBMP280() {
  DadosBMP dados;
  dados.valido = false;

  // Inicializa I2C
  Wire.begin(D6, D5); // SDA = D6, SCL = D5

  // Tenta inicializar sensor
  if (!bmp.begin(0x76)) {
    Serial.println("⚠️ Não achou no 0x76, tentando 0x77");
    if (!bmp.begin(0x77)) {
      Serial.println("❌ BMP280 não encontrado!");
      dados.temperatura = 0;
      dados.pressao = 0;
      dados.altitude = 0;
      return dados;
    }
  }

  // Faz leitura
  dados.temperatura = bmp.readTemperature();
  dados.pressao = bmp.readPressure();
  dados.altitude = bmp.readAltitude(1013.25);

  // ✅ NOVO: Valida leituras
  bool tempValida = validarLeitura(dados.temperatura, -50.0, 100.0);
  bool pressValida = validarLeitura(dados.pressao, 30000.0, 110000.0);
  bool altValida = validarLeitura(dados.altitude, -500.0, 9000.0);

  dados.valido = tempValida && pressValida && altValida;

  // Log
  Serial.println("📡 Leitura do BMP280:");
  Serial.printf("  Temperatura: %.2f°C %s\n", dados.temperatura, tempValida ? "✓" : "✗");
  Serial.printf("  Pressão: %.2f Pa %s\n", dados.pressao, pressValida ? "✓" : "✗");
  Serial.printf("  Altitude: %.2f m %s\n", dados.altitude, altValida ? "✓" : "✗");
  Serial.printf("  Status: %s\n", dados.valido ? "VÁLIDO" : "INVÁLIDO");
  Serial.println("--------------------------");

  return dados;
}

/**
 * ✅ MELHORADO: Lê DHT11 com validação
 */
DadosDHT lerDHT() {
  DadosDHT dados;
  dados.valido = false;

  dados.umidadeDHT = dht.readHumidity();
  dados.temperaturaDHT = dht.readTemperature();

  // ✅ NOVO: Valida leituras
  bool tempValida = validarLeitura(dados.temperaturaDHT, -50.0, 100.0);
  bool umidValida = validarLeitura(dados.umidadeDHT, 0.0, 100.0);

  dados.valido = tempValida && umidValida;

  // Log
  Serial.println("📡 Leitura do DHT11:");
  Serial.printf("  Temperatura: %.2f°C %s\n", dados.temperaturaDHT, tempValida ? "✓" : "✗");
  Serial.printf("  Umidade: %.2f%% %s\n", dados.umidadeDHT, umidValida ? "✓" : "✗");
  Serial.printf("  Status: %s\n", dados.valido ? "VÁLIDO" : "INVÁLIDO");
  Serial.println("--------------------------");

  return dados;
}

/**
 * ✅ MELHORADO: Lê sensor de chuva com validação
 */
int lerSensorChuva() {
  int val_chuva = analogRead(pin_chuva_analog);
  
  // ✅ NOVO: Valida range (0-1024 para ESP8266/ESP32)
  if (val_chuva < 0 || val_chuva > 1024) {
    Serial.printf("⚠️ Valor de chuva fora do range: %d\n", val_chuva);
    val_chuva = 1024; // Assume "sem chuva" em caso de erro
  }

  Serial.printf("🌧️ Sensor de chuva: %d ", val_chuva);
  if (val_chuva < 300) {
    Serial.println("(Chuva Intensa)");
  } else if (val_chuva <= 500) {
    Serial.println("(Chuva Moderada)");
  } else {
    Serial.println("(Sem Chuva)");
  }

  return val_chuva;
}

// ============================================
// FUNÇÕES DE CONEXÃO
// ============================================

/**
 * ✅ MELHORADO: Conecta WiFi com retry e timeout
 */
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 Conectando ao WiFi");
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi conectado com sucesso!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("❌ ERRO: Falha ao conectar WiFi");
    Serial.println("  Verifique SSID e senha");
  }
}

/**
 * ✅ MELHORADO: Inicializa Firebase com autenticação segura
 */
void initFirebase() {
  Serial.println("\n🔥 Configurando Firebase...");
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // ✅ AUTENTICAÇÃO COM EMAIL/SENHA
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.printf("  Cliente Firebase v%s\n", FIREBASE_CLIENT_VERSION);
  Serial.println("⏳ Aguardando autenticação...");

  // Aguarda autenticação (máximo 30 segundos)
  unsigned long inicio = millis();
  while (!Firebase.ready() && (millis() - inicio < 30000)) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println();
  
  if (Firebase.ready()) {
    Serial.println("✓ Firebase autenticado com sucesso!");
    Serial.print("  UID: ");
    Serial.println(auth.token.uid.c_str());
    firebaseReady = true;
  } else {
    Serial.println("❌ ERRO: Falha na autenticação Firebase");
    Serial.println("  Verifique:");
    Serial.println("  1. Email e senha corretos");
    Serial.println("  2. Usuário existe no Firebase Authentication");
    Serial.println("  3. Regras de segurança configuradas");
    firebaseReady = false;
  }
}

/**
 * ✅ NOVO: Verifica e reconecta WiFi se necessário
 */
void verificarConexaoWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi desconectado! Reconectando...");
    WiFi.disconnect();
    delay(1000);
    connectWiFi();
  }
}

// ============================================
// FUNÇÕES DO DISPLAY
// ============================================

/**
 * Inicializa display OLED
 */
void initDisplay() {
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  ui.setTargetFPS(24);
}

/**
 * Overlay para mostrar tempo de execução
 */
void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()/1000) + "s");
}
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

// --- FRAMES DE EXIBIÇÃO --- //
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Estado:");
  
  // ✅ MELHORADO: Mostra status do Firebase
  String statusTexto = firebaseReady ? "Online" : "Offline";
  display->drawString(0 + x, 20 + y, statusTexto);
  
  // ✅ NOVO: Mostra ícone de WiFi
  if (WiFi.status() == WL_CONNECTED) {
    display->drawString(0 + x, 40 + y, "WiFi: OK");
  } else {
    display->drawString(0 + x, 40 + y, "WiFi: X");
  }
  display->display();
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Temperatura:");
  display->drawString(0 + x, 20 + y, String(temperatura, 1) + " C");
  display->display();
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 0 + y, "Pressao:");
  display->drawString(0 + x, 20 + y, String(pressao, 1) + " hPa");
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

// ============================================
// FUNÇÕES DO FIREBASE
// ============================================

/**
 * ✅ MELHORADO: Atualiza dados do Firebase com tratamento de erros
 */
void atualizarDadosDoFirebase() {
  if (!Firebase.ready()) {
    Serial.println("⚠️ Firebase não está pronto para leitura");
    return;
  }

  String caminho = "/" + String(ROOT_NODE) + "/ultimo";
  Serial.println("📥 Lendo dados do Firebase: " + caminho);

  if (Firebase.RTDB.getJSON(&fbdo, caminho.c_str())) {
    FirebaseJson json = fbdo.to<FirebaseJson>();
    FirebaseJsonData result;

    if (json.get(result, "bool")) estado = result.to<bool>();
    if (json.get(result, "float")) temperatura = result.to<float>();
    if (json.get(result, "double")) pressao = result.to<float>();
    if (json.get(result, "int")) umidade = result.to<float>();
    if (json.get(result, "string")) mensagem = result.to<String>();

    Serial.println("✓ Dados recebidos com sucesso");
  } else {
    Serial.printf("❌ Erro ao ler dados: %s\n", fbdo.errorReason().c_str());
  }
}

/**
 * ✅ MELHORADO: Envia dados para Firebase com validação completa
 */
void enviarDadosParaFirebase() {
  if (!Firebase.ready()) {
    Serial.println("⚠️ Firebase não está pronto. Tentando reconectar...");
    initFirebase();
    return;
  }

  Serial.println("\n📤 Preparando envio de dados...");

  // Lê todos os sensores
  DadosDHT leituraDHT = lerDHT();
  DadosBMP leituraBMP = lerBMP280();
  int valorChuva = lerSensorChuva();

  // ✅ NOVO: Verifica se tem pelo menos um sensor válido
  if (!leituraDHT.valido && !leituraBMP.valido) {
    Serial.println("❌ ERRO: Nenhum sensor retornou dados válidos!");
    Serial.println("  Pulando envio desta leitura");
    return;
  }

  // ✅ NOVO: Sanitiza valores antes de enviar
  float tempDHT = sanitizarValor(leituraDHT.temperaturaDHT, 0.0);
  float umidDHT = sanitizarValor(leituraDHT.umidadeDHT, 0.0);
  float pressBMP = sanitizarValor(leituraBMP.pressao, 101325.0); // Pressão padrão ao nível do mar
  float altBMP = sanitizarValor(leituraBMP.altitude, 0.0);

  // Pega timestamp
  unsigned long long timestamp = getTimestamp();

  // Cria JSON
  FirebaseJson json;
  
  // ✅ MELHORADO: Só adiciona dados válidos
  if (leituraDHT.valido) {
    json.set("DHT/temperatura", tempDHT);
    json.set("DHT/umidade", umidDHT);
  }
  
  if (leituraBMP.valido) {
    json.set("BMP280/pressao", pressBMP);
    json.set("BMP280/altitude", altBMP);
  }
  
  json.set("chuva", valorChuva);
  json.set("timestamp", (double)timestamp);

  bool sucessoUltimo = false;
  bool sucessoHistorico = false;

  // Envia para /ultimo
  Serial.print("  Enviando para /ultimo... ");
  if (Firebase.RTDB.setJSON(&fbdo, "/" ROOT_NODE "/ultimo", &json)) {
    Serial.println("✓ OK");
    sucessoUltimo = true;
  } else {
    Serial.println("✗ ERRO");
    Serial.printf("    Motivo: %s\n", fbdo.errorReason().c_str());
  }

  // Envia para /historico
  Serial.print("  Enviando para /historico... ");
  if (Firebase.RTDB.pushJSON(&fbdo, "/" ROOT_NODE "/historico", &json)) {
    Serial.println("✓ OK");
    sucessoHistorico = true;
  } else {
    Serial.println("✗ ERRO");
    Serial.printf("    Motivo: %s\n", fbdo.errorReason().c_str());
  }

  // ✅ NOVO: Gerencia contador de falhas
  if (sucessoUltimo && sucessoHistorico) {
    tentativasReconexao = 0;
    Serial.println("✓ Envio completo com sucesso!\n");
  } else {
    tentativasReconexao++;
    Serial.printf("⚠️ Falha no envio (tentativa %d/%d)\n\n", tentativasReconexao, MAX_TENTATIVAS);
    
    if (tentativasReconexao >= MAX_TENTATIVAS) {
      Serial.println("❌ Múltiplas falhas detectadas. Reiniciando Firebase...");
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
  
  Serial.println("\n\n========================================");
  Serial.println("   TempoBô - Sistema Seguro v2.0");
  Serial.println("   Monitoramento Meteorológico");
  Serial.println("========================================\n");

  Wire.begin(D1, D2);

  // Inicializa display
  initDisplay();
  display.clear();
  display.drawString(0, 0, "TempoBo v2.0");
  display.drawString(0, 15, "Iniciando...");
  display.display();

  // Conecta WiFi
  connectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    display.clear();
    display.drawString(0, 0, "ERRO:");
    display.drawString(0, 15, "WiFi falhou");
    display.display();
    Serial.println("❌ Sistema não pode continuar sem WiFi");
    while (true) delay(1000);
  }

  // Inicializa Firebase
  initFirebase();

  // Configura UI do display
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.init();

  // Configura sensores
  pinMode(pin_chuva_digital, INPUT);
  pinMode(pin_chuva_analog, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  dht.begin();

  // Configura NTP
  Serial.println("🕐 Sincronizando com NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  time_t now = time(nullptr);
  int tentativas = 0;
  while (now < 100000 && tentativas < 20) {
    Serial.print(".");
    delay(500);
    now = time(nullptr);
    tentativas++;
  }
  
  Serial.println();
  if (tentativas < 20) {
    Serial.println("✓ Hora sincronizada!");
    Serial.print("  Data/Hora: ");
    Serial.println(ctime(&now));
  } else {
    Serial.println("⚠️ Falha ao sincronizar hora");
  }

  // Mensagem final
  Serial.println("\n========================================");
  Serial.println("✓ Sistema inicializado com sucesso!");
  Serial.println("  Iniciando monitoramento...");
  Serial.println("========================================\n");

  display.clear();
  display.drawString(0, 0, "Sistema OK!");
  display.drawString(0, 15, "Monitorando...");
  display.display();
  delay(2000);
}

// ============================================
// LOOP PRINCIPAL
// ============================================

const unsigned long telaInterval = 4000; // 4 segundos por tela
unsigned long lastScreenChange = 0;

void loop() {
  unsigned long currentMillis = millis();

  // ✅ NOVO: Verifica WiFi periodicamente
  static unsigned long ultimaVerificacaoWiFi = 0;
  if (currentMillis - ultimaVerificacaoWiFi > 30000) { // A cada 30 segundos
    ultimaVerificacaoWiFi = currentMillis;
    verificarConexaoWiFi();
  }

  // 1️⃣ Envia dados a cada 1 minuto
  if (currentMillis - lastFirebaseUpdate > firebaseInterval) {
    lastFirebaseUpdate = currentMillis;

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     CICLO DE ATUALIZAÇÃO INICIADO      ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    enviarDadosParaFirebase();   // Envia dados
    atualizarDadosDoFirebase();  // Lê dados atualizados
    
    currentScreen = 0;           // Reinicia sequência de telas
    lastScreenChange = currentMillis;

    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║     CICLO DE ATUALIZAÇÃO CONCLUÍDO     ║");
    Serial.println("╚════════════════════════════════════════╝\n");
  }

  // 2️⃣ Atualiza telas do display
  if (currentMillis - lastScreenChange > telaInterval) {
    lastScreenChange = currentMillis;
    FrameCallback f = frames[currentScreen];
    f(&display, nullptr, 0, 0);
    currentScreen = (currentScreen + 1) % frameCount;
  }

  // ✅ NOVO: Pequeno delay para não sobrecarregar
  delay(100);
}
