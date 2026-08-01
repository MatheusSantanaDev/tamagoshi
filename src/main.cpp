#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "config.h"
#include "Pokemon.h"
#include "EPDisplay.h"
#include "Input.h"
#include "TimeKeeper.h"

Pokemon pet;
EPDisplay display;
Input input;
WebServer server(80);

// ============================================================
// Pagina web com os botoes virtuais
// ============================================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Tamagotchi ESP32</title>
<style>
  body { font-family: system-ui, sans-serif; background: #1e1e2e; color: #fff;
         display: flex; flex-direction: column; align-items: center;
         min-height: 100vh; margin: 0; padding: 16px; box-sizing: border-box; }
  h1 { font-size: 1.4rem; margin: 8px 0; }
  .status-box { background: #2a2a3e; padding: 8px 16px; border-radius: 20px;
                font-size: .9rem; margin-bottom: 20px; }
  .btn { width: 100%; max-width: 400px; padding: 22px; margin: 10px 0;
         font-size: 1.3rem; font-weight: bold; color: #fff; border: none;
         border-radius: 16px; cursor: pointer; touch-action: manipulation; }
  .btn:active { filter: brightness(0.85); transform: scale(0.97); }
  .btn-feed  { background: #e08c3a; }
  .btn-play  { background: #3a9ee0; }
  .btn-clean { background: #27ae60; }
  .btn-stat  { background: #6c5ce7; }
  .btn-reset { background: #d63031; }
</style>
</head>
<body>
<h1>Tamagotchi</h1>
<div class="status-box" id="status">Conectado</div>
<button class="btn btn-feed"  onclick="press('feed')">ALIMENTAR</button>
<button class="btn btn-play"  onclick="press('play')">BRINCAR</button>
<button class="btn btn-clean" onclick="press('clean')">LIMPAR</button>
<button class="btn btn-stat"  onclick="press('status')">STATUS</button>
<button class="btn btn-reset" onclick="press('reset')">RESETAR</button>
<script>
  function press(cmd) {
    var s = document.getElementById('status');
    s.textContent = 'Enviando: ' + cmd + '...';
    fetch('/action?cmd=' + cmd).then(function(r) {
      s.textContent = 'Enviado: ' + cmd + ' (' + r.status + ')';
    }).catch(function() {
      s.textContent = 'Falha ao conectar';
    });
  }
</script>
</body>
</html>
)rawliteral";

// Acoes pendentes vindas da pagina web
volatile ButtonAction pendingAction = ACTION_NONE;

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleAction() {
    String cmd = server.arg("cmd");
    if (cmd == "feed") {
        pendingAction = ACTION_FEED;
    } else if (cmd == "play") {
        pendingAction = ACTION_PLAY;
    } else if (cmd == "clean") {
        pendingAction = ACTION_CLEAN;
    } else if (cmd == "status") {
        pendingAction = ACTION_STATUS;
    } else if (cmd == "reset") {
        pendingAction = ACTION_RESET;
    }
    server.send(200, "text/plain", "ok");
}

void startWebServer() {
    server.on("/", handleRoot);
    server.on("/action", handleAction);
    server.begin();
    Serial.println("Servidor web no ar (botoes virtuais).");
}

// ============================================================
// State machine
// ============================================================
enum GameState {
    STATE_IDLE,
    STATE_STATS,
    STATE_EVOLVING,
    STATE_WARNING,
    STATE_SLEEPING,
    STATE_DEAD,
    STATE_STARTUP
};

GameState gameState = STATE_STARTUP;

// ============================================================
// Timing
// ============================================================
unsigned long lastStatsUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSave = 0;
unsigned long lastInteraction = 0;
unsigned long previousMillis = 0;
unsigned long accumulatedDelta = 0;  // Delta acumulado para update do Pokémon

// Assinatura dos dados exibidos na tela de status. Quando muda (minuto a
// minuto), a tela e redesenhada; os segundos do relogio usam refresh parcial.
uint32_t lastStatsFp = 0;

static uint32_t statsFingerprint() {
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)pet.getStage()) * 16777619u;
    h = (h ^ (uint32_t)pet.getAge()) * 16777619u;
    h = (h ^ (uint32_t)pet.getWarmth()) * 16777619u;
    h = (h ^ (uint32_t)pet.getIncubationProgress()) * 16777619u;
    h = (h ^ (uint32_t)pet.getHunger()) * 16777619u;
    h = (h ^ (uint32_t)pet.getHappiness()) * 16777619u;
    h = (h ^ (uint32_t)pet.getHealth()) * 16777619u;
    h = (h ^ (uint32_t)pet.getEnergy()) * 16777619u;
    h = (h ^ (uint32_t)pet.getSleep()) * 16777619u;
    h = (h ^ (uint32_t)pet.getHygiene()) * 16777619u;
    h = (h ^ (uint32_t)pet.getDirt()) * 16777619u;
    h = (h ^ (uint32_t)pet.getLostCount()) * 16777619u;
    h = (h ^ (uint32_t)pet.getWinCount()) * 16777619u;
    h = (h ^ (uint32_t)pet.getShortestLife()) * 16777619u;
    h = (h ^ (uint32_t)pet.getLongestLife()) * 16777619u;
    return h;
}

// ============================================================
// Sleep mode (futuro: deep sleep)
// ============================================================
bool sleeping = false;

// ============================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n=== Pokemon Tamagotchi ESP32 ===");
    Serial.println("Inicializando...");

    // Conecta no WiFi (para os botoes virtuais)
    timeKeeperInit();
    Serial.print("Conectando WiFi ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long wifiTimeout = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi OK: http://%s\n", WiFi.localIP().toString().c_str());
        startWebServer();

        // Sincroniza o relogio via NTP (se falhar, fica no relogio padrao)
        Serial.print("Sincronizando relogio NTP ");
        unsigned long ntpTimeout = millis() + 10000;
        while (!timeIsSynced() && millis() < ntpTimeout) {
            timeKeeperTrySync(250);
            Serial.print(".");
        }
        if (timeIsSynced()) {
            struct tm tm;
            time_t nowt = epochNow();
            localtime_r(&nowt, &tm);
            Serial.printf("\nRelogio OK: %02d:%02d:%02d\n",
                          tm.tm_hour, tm.tm_min, tm.tm_sec);
        } else {
            Serial.println("\nNTP falhou - usando relogio padrao");
        }
    } else {
        // Fallback: ESP32 cria o proprio hotspot (modo AP)
        Serial.println("\nWiFi falhou - criando hotspot proprio...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        Serial.printf("Conecte o celular na rede WiFi '%s' (senha: %s)\n",
                      AP_SSID, AP_PASS);
        Serial.println("Depois abra: http://192.168.4.1");
        startWebServer();
    }

    // Inicializa componentes
    input.begin();
    display.begin();

#ifdef DEMO_MODE
    pet.resetEgg();
    pet.forceStage(STAGE_EGG);
    gameState = STATE_IDLE;
    Serial.println("DEMO MODE: FEED=estagio | PLAY=tela | STATUS=stats");
#else
    pet.begin(epochNow());
#endif

    // Tela inicial
    if (pet.isDead()) {
        gameState = STATE_DEAD;
        display.drawDead(pet);
    } else {
        gameState = STATE_IDLE;
        display.drawPet(pet);
    }

    // Inicializa timestamps
    unsigned long now = millis();
    lastStatsUpdate = now;
    lastDisplayUpdate = now;
    lastSave = now;
    lastInteraction = now;
    previousMillis = now;

    Serial.println("Pronto! Pressione os botoes para interagir.");
}

// ============================================================
void loop() {
    unsigned long now = millis();
    unsigned long delta = now - previousMillis;
    previousMillis = now;

    server.handleClient();

    ButtonAction action = input.read();
    if (action == ACTION_NONE && pendingAction != ACTION_NONE) {
        action = pendingAction;
        pendingAction = ACTION_NONE;
    }

#ifdef DEMO_MODE
    // ========================================
    // DEMO: só navega pelas telas/estágios
    // ========================================
    static int demoStage = STAGE_EGG;
    static int demoScreen = 0;

    if (action == ACTION_FEED) {
        demoStage = (demoStage + 1) % (STAGE_MEGARAICHUY + 1);
        pet.forceStage((PokemonStage)demoStage);
        Serial.printf("[DEMO] Estagio: %s\n", pet.getStageName());
        lastDisplayUpdate = 0;
    } else if (action == ACTION_PLAY) {
        demoScreen = (demoScreen + 1) % 6;
        Serial.printf("[DEMO] Tela: %d\n", demoScreen);
        lastDisplayUpdate = 0;
    } else if (action == ACTION_STATUS) {
        gameState = (gameState == STATE_STATS) ? STATE_IDLE : STATE_STATS;
        lastDisplayUpdate = 0;
    } else if (action == ACTION_RESET) {
        demoStage = STAGE_EGG;
        demoScreen = 0;
        pet.forceStage(STAGE_EGG);
        gameState = STATE_IDLE;
        Serial.println("[DEMO] Resetado para o ovo");
    }

    // Só renderiza quando houve um clique
    if (action != ACTION_NONE) {
        switch (demoScreen) {
            case 0: gameState = STATE_IDLE;      display.drawPet(pet);       break;
            case 1: gameState = STATE_STATS;     display.drawStats(pet, epochNow()); break;
            case 2: gameState = STATE_WARNING;   display.drawWarning(pet);   break;
            case 3: gameState = STATE_SLEEPING;  display.drawSleeping();     break;
            case 4: gameState = STATE_DEAD;      display.drawDead(pet);      break;
            case 5: gameState = STATE_EVOLVING;  display.drawEvolution(pet); break;
        }
    }

    // Relogio: segundos passam com refresh parcial (sem rebuild da tela)
    if (gameState == STATE_STATS) {
        display.drawClockTick(epochNow());
    }

    // Tela do pet: barras/cocos/idade com refresh parcial
    if (gameState == STATE_IDLE) {
        // [TESTE] Barras decaindo uma por vez (Ener->Sono->Hig->Coco)
        static unsigned long lastBarTest = 0;
        if (demoScreen == 0 && now - lastBarTest >= 1000) {
            lastBarTest = now;
            pet.testCycleBars();
        }
        display.drawPetUpdates(pet);
    }

    delay(10);
    return;
#endif

    // ========================================
    // 1. INPUT
    // ========================================

    if (action != ACTION_NONE) {
        lastInteraction = now;
        sleeping = false;

        switch (action) {
            case ACTION_FEED:
                if (!pet.isDead()) {
                    pet.feed();
                    Serial.println(pet.isEgg() ? "Aqueceu o ovo!" : "Alimentou o Pokemon!");
                    gameState = STATE_IDLE;
                }
                break;

            case ACTION_PLAY:
                if (!pet.isDead()) {
                    pet.play();
                    Serial.println(pet.isEgg() ? "Ajudou o ovo!" : "Brincou com o Pokemon!");
                    gameState = STATE_IDLE;
                }
                break;

            case ACTION_CLEAN:
                if (!pet.isDead() && !pet.isEgg()) {
                    pet.clean();
                    Serial.println("Limpou o coco!");
                    gameState = STATE_IDLE;
                }
                break;

            case ACTION_STATUS:
                if (pet.isDead()) {
                    // Apenas mostrar tela de morto
                    gameState = STATE_DEAD;
                } else if (gameState == STATE_STATS) {
                    gameState = STATE_IDLE;
                } else {
                    gameState = STATE_STATS;
                }
                break;

            case ACTION_RESET:
                Serial.println("RESET solicitado!");
                pet.reset();
                gameState = STATE_IDLE;
                display.showMessage("Resetado!", "Novo ovo", "nasceu!");
                delay(2000);
                break;
        }

        // Marca para atualizar display
        lastDisplayUpdate = 0;
    }

    // ========================================
    // 2. UPDATE ESTADO
    // ========================================
    if (!pet.isDead() && (now - lastStatsUpdate >= STATS_UPDATE_MS)) {
        lastStatsUpdate = now;

        // Dormindo: recupera energia/sono (alem do decay normal)
        if (gameState == STATE_SLEEPING) {
            pet.sleepRecovery(delta);
        }

        // Acumula delta e atualiza
        accumulatedDelta += STATS_UPDATE_MS;
        pet.update(STATS_UPDATE_MS);

        // Verifica evolução
        if (pet.checkEvolution()) {
            gameState = STATE_EVOLVING;
            display.drawEvolution(pet);
            delay(3000); // Mostra por 3 segundos
            gameState = STATE_IDLE;
            lastDisplayUpdate = 0;
            Serial.printf("Evoluiu para %s!\n", pet.getStageName());
        }

        // Verifica alertas
        if (pet.isCritical()) {
            gameState = STATE_WARNING;
            lastDisplayUpdate = 0;
        } else if (gameState == STATE_WARNING) {
            gameState = STATE_IDLE;
            lastDisplayUpdate = 0;
        }
    }

    // ========================================
    // 3. DISPLAY
    // ========================================
    bool forceUpdate = (action != ACTION_NONE);
    bool needRedraw = forceUpdate;

    if (gameState == STATE_STATS) {
        // Tela de status: redesenha so quando os dados mudam (1x/min).
        // Os segundos do relogio sao atualizados via refresh parcial.
        uint32_t fp = statsFingerprint();
        if (fp != lastStatsFp) {
            lastStatsFp = fp;
            needRedraw = true;
        }
    } else if (gameState != STATE_IDLE &&
               now - lastDisplayUpdate >= DISPLAY_REFRESH_MS) {
        // Idle usa atualizacao parcial (drawPetUpdates), sem timer
        needRedraw = true;
    }

    if (needRedraw) {
        lastDisplayUpdate = now;

        switch (gameState) {
            case STATE_IDLE:
                display.drawPet(pet);
                break;
            case STATE_STATS:
                display.drawStats(pet, epochNow());
                break;
            case STATE_WARNING:
                display.drawWarning(pet);
                break;
            case STATE_SLEEPING:
                display.drawSleeping();
                break;
            case STATE_DEAD:
                display.drawDead(pet);
                break;
            default:
                break;
        }
    }

    // Tela do pet: barras/cocos/idade com refresh parcial
    if (gameState == STATE_IDLE) {
        display.drawPetUpdates(pet);
    }

    // Relogio: segundos passam com refresh parcial (sem rebuild da tela)
    if (gameState == STATE_STATS) {
        display.drawClockTick(epochNow());
    }

    // ========================================
    // 4. AUTO-SAVE
    // ========================================
    if (now - lastSave >= SAVE_INTERVAL_S * 1000) {
        lastSave = now;
        pet.save();
    }

    // ========================================
    // 5. SLEEP (economia de energia)
    // ========================================
    if (!sleeping && !pet.isDead() &&
        (now - lastInteraction >= SLEEP_AFTER_MS) &&
        gameState != STATE_EVOLVING) {
        sleeping = true;
        gameState = STATE_SLEEPING;
        lastDisplayUpdate = 0;
        Serial.println("Modo dormir...");
    }

    // ========================================
    // 6. RE-SYNC (se o WiFi voltou depois do boot sem rede)
    // ========================================
    static unsigned long lastSyncAttempt = 0;
    if (!timeIsSynced() && WiFi.status() == WL_CONNECTED &&
        (now - lastSyncAttempt >= 60000)) {
        lastSyncAttempt = now;
        if (timeKeeperTrySync(3000)) {
            Serial.println("Relogio sincronizado agora!");
            pet.catchUpFrom(epochNow());
            lastDisplayUpdate = 0;
        }
    }

    // Pequeno delay para evitar loop muito rápido
    delay(10);
}
