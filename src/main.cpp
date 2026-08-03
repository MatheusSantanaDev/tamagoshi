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
static const char INDEX_HEAD[] PROGMEM = R"rawliteral(
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
  h2 { font-size: 1.1rem; margin: 24px 0 8px; color: #a6e3a1; }
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
  .dev-label { width: 100%; max-width: 400px; text-align: left; font-size: .9rem;
               margin: 14px 0 6px; color: #cdd6f4; font-weight: bold; }
  .dev-grid { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center;
              max-width: 400px; }
  .dev-btn { padding: 10px 14px; font-size: .95rem; font-weight: bold; color: #fff;
             background: #45475a; border: none; border-radius: 10px;
             cursor: pointer; touch-action: manipulation; }
  .dev-btn:active { filter: brightness(0.85); transform: scale(0.97); }
  .dev-note { margin-top: 16px; font-size: .8rem; color: #a6adc8;
              max-width: 400px; text-align: center; }
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
)rawliteral";

// Guia de testes - modo dev (sempre servida; e o canal de interferencia
// do desenvolvedor no app em modo normal)
static const char DEV_GUIDE_HTML[] PROGMEM = R"rawliteral(
<hr style="width:100%; max-width:400px; border-color:#313244;">
<h2>Guia de Testes (Dev)</h2>
<div class="dev-label">Pokemon (estagio):</div>
<div class="dev-grid">
  <button class="dev-btn" onclick="press('stage','0')">Ovo</button>
  <button class="dev-btn" onclick="press('stage','1')">Scyther</button>
  <button class="dev-btn" onclick="press('stage','2')">Scizor</button>
  <button class="dev-btn" onclick="press('stage','3')">Kleavor</button>
  <button class="dev-btn" onclick="press('stage','4')">MegaScizor</button>
  <button class="dev-btn" onclick="press('stage','5')">Pichu</button>
  <button class="dev-btn" onclick="press('stage','6')">Pikachu</button>
  <button class="dev-btn" onclick="press('stage','7')">Raichu</button>
  <button class="dev-btn" onclick="press('stage','8')">MegaRaichuX</button>
  <button class="dev-btn" onclick="press('stage','9')">MegaRaichuY</button>
  <button class="dev-btn" onclick="press('stage','10')">Elekid</button>
  <button class="dev-btn" onclick="press('stage','11')">Electabuzz</button>
  <button class="dev-btn" onclick="press('stage','12')">Electivire</button>
  <button class="dev-btn" onclick="press('stage','13')">Magby</button>
  <button class="dev-btn" onclick="press('stage','14')">Magmar</button>
  <button class="dev-btn" onclick="press('stage','15')">Magmortar</button>
  <button class="dev-btn" onclick="press('stage','16')">Rhyhorn</button>
  <button class="dev-btn" onclick="press('stage','17')">Rhydon</button>
  <button class="dev-btn" onclick="press('stage','18')">Rhyperior</button>
  <button class="dev-btn" onclick="press('stage','19')">Onix</button>
  <button class="dev-btn" onclick="press('stage','20')">Steelix</button>
  <button class="dev-btn" onclick="press('stage','21')">MegaSteelix</button>
  <button class="dev-btn" onclick="press('stage','22')">Tangela</button>
  <button class="dev-btn" onclick="press('stage','23')">Tangrowth</button>
</div>
<div class="dev-label">Coco (sujeira):</div>
<div class="dev-grid">
  <button class="dev-btn" onclick="press('dirt','0')">0 coco</button>
  <button class="dev-btn" onclick="press('dirt','25')">1 coco</button>
  <button class="dev-btn" onclick="press('dirt','50')">2 cocos</button>
  <button class="dev-btn" onclick="press('dirt','75')">Sujeira alta</button>
  <button class="dev-btn" onclick="press('dirt','100')">Sujinho</button>
</div>
<div class="dev-label">Barras (energia/higiene):</div>
<div class="dev-grid">
  <button class="dev-btn" onclick="press('bar','0','-20')">Energia -20</button>
  <button class="dev-btn" onclick="press('bar','0','20')">Energia +20</button>
  <button class="dev-btn" onclick="press('bar','1','-20')">Higiene -20</button>
  <button class="dev-btn" onclick="press('bar','1','20')">Higiene +20</button>
</div>
<div class="dev-label">Sono:</div>
<div class="dev-grid">
  <button class="dev-btn" onclick="press('sleep','0')">DORMIR</button>
  <button class="dev-btn" onclick="press('sleep','1')">POKEMON (dormindo)</button>
</div>
<div class="dev-note">Acoes dev voltam para a tela do pet. USE o botao LIMPAR
para zerar os cocos depois do teste. DORMIR entra no sono direto na tela
Zzz central; POKEMON (dormindo) pula para a tela do pokemon com o Zzz na
cabeca (ele continua dormindo/recoverando); qualquer acao acorda.</div>
)rawliteral";

static const char INDEX_FOOT[] PROGMEM = R"rawliteral(
<script>
  function press(cmd, v, d) {
    var s = document.getElementById('status');
    var url = '/action?cmd=' + cmd;
    if (v !== undefined) url += '&v=' + v;
    if (d !== undefined) url += '&d=' + d;
    s.textContent = 'Enviando: ' + cmd + '...';
    fetch(url).then(function(r) {
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
volatile int pendingValue = 0;  // [DEV] valor das acoes dev (estagio/sujeira/barra)

void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(INDEX_HEAD);
    server.sendContent_P(DEV_GUIDE_HTML);
    server.sendContent_P(INDEX_FOOT);
    server.sendContent("");
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
    } else if (cmd == "stage") {
        pendingValue = server.arg("v").toInt();
        pendingAction = ACTION_DEV_STAGE;
    } else if (cmd == "dirt") {
        pendingValue = server.arg("v").toInt();
        pendingAction = ACTION_DEV_DIRT;
    } else if (cmd == "bar") {
        int idx = server.arg("v").toInt();
        int delta = server.arg("d").toInt();
        pendingValue = (idx << 8) | (delta & 0xFF);
        pendingAction = ACTION_DEV_BAR;
    } else if (cmd == "sleep") {
        // v=0 -> tela Zzz central; v=1 -> tela do pokemon (ainda dormindo)
        pendingValue = server.arg("v").toInt();
        pendingAction = ACTION_DEV_SLEEP;
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
bool sleepShowZzz = true;          // modo dormir: true = Zzz, false = pokemon
unsigned long lastSleepSwitch = 0; // inicio do passo atual do ciclo de sono

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

#if DEMO_MODE
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

    Serial.println("Pronto! Pressione os botoes para interagir.");
}

// ============================================================
void loop() {
    unsigned long now = millis();

    server.handleClient();

    ButtonAction action = input.read();
    if (action == ACTION_NONE && pendingAction != ACTION_NONE) {
        action = pendingAction;
        pendingAction = ACTION_NONE;
    }

#if DEMO_MODE
    // ========================================
    // DEMO: só navega pelas telas/estágios
    // ========================================
    static int demoStage = STAGE_EGG;
    static int demoScreen = 0;

    if (action == ACTION_FEED) {
        if (pet.isEgg()) {
            pet.feed();
            Serial.printf("[DEMO] Aqueceu o ovo! Calor: %d\n", pet.getWarmth());
        } else {
            demoStage = (demoStage + 1) % (STAGE_MEGARAICHUY + 1);
            pet.forceStage((PokemonStage)demoStage);
            Serial.printf("[DEMO] Estagio: %s\n", pet.getStageName());
        }
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
        pet.resetEgg();
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

    // Ovo no demo: o tempo passa (lvl sobe, calor decai, incubacao avanca)
    if (pet.isEgg()) {
        static unsigned long lastEggSim = 0;
        if (now - lastEggSim >= 2000) {
            lastEggSim = now;
            pet.update(30000);  // 1 min de jogo a cada 4s reais
            EvolutionResult evo = pet.checkEvolution();
            if (evo != EVO_NONE) {
                Serial.printf("[DEMO] Chocou! Novo: %s (person: %s)\n",
                              pet.getStageName(), pet.getPersonalityName());
            }
        }
    }

    // Tela do pet: barras/cocos/idade com refresh parcial.
    // No modo dev, os cliques da guia de testes mexem nos stats e o
    // drawPetUpdates atualiza so as regioes alteradas.
    if (gameState == STATE_IDLE) {
        display.drawPetUpdates(pet);
    }

    delay(10);
    return;
#endif

    // ========================================
    // 1. INPUT
    // ========================================
    bool devAction = false;          // clique da guia de testes
    bool wasPetScreen = true;        // tela do pet antes da acao
    bool devFromOtherScreen = false; // dev veio de stats/sono/aviso -> redraw total

    if (action != ACTION_NONE) {
        lastInteraction = now;
        sleeping = false;
        devAction = (action == ACTION_DEV_STAGE ||
                     action == ACTION_DEV_DIRT ||
                     action == ACTION_DEV_BAR ||
                     action == ACTION_DEV_SLEEP);
        wasPetScreen = (gameState == STATE_IDLE);
        devFromOtherScreen = devAction && !wasPetScreen;

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

            // ========================================
            // [DEV] Guia de testes da pagina web
            // ========================================
            case ACTION_DEV_STAGE:
                pet.forceStage((PokemonStage)pendingValue);
                Serial.printf("[DEV] Estagio: %s\n", pet.getStageName());
                gameState = STATE_IDLE;
                break;

            case ACTION_DEV_DIRT:
                pet.devSetDirt(pendingValue);
                Serial.printf("[DEV] Sujeira: %d\n", pet.getDirt());
                gameState = STATE_IDLE;
                break;

            case ACTION_DEV_BAR:
                pet.devChangeBar(pendingValue >> 8, (int8_t)(pendingValue & 0xFF));
                Serial.printf("[DEV] Barra ajustada\n");
                gameState = STATE_IDLE;
                break;

            case ACTION_DEV_SLEEP:
                // Forca o modo dormir na hora (mesmo ciclo do sono natural).
                // pendingValue: 0 = tela Zzz central, 1 = pokemon dormindo.
                sleeping = true;
                gameState = STATE_SLEEPING;
                sleepShowZzz = (pendingValue != 1);
                lastSleepSwitch = now;
                Serial.printf("[DEV] Dormindo! Fase: %s\n",
                              sleepShowZzz ? "Zzz" : "Pokemon");
                break;
        }

        // Marca para atualizar display
        lastDisplayUpdate = 0;
    }

    // ========================================
    // 2. UPDATE ESTADO
    // ========================================
    if (!pet.isDead() && (now - lastStatsUpdate >= STATS_UPDATE_MS)) {
        // Tempo real decorrido desde o ultimo disparo: garante 1:1 com o
        // relogio real mesmo se o loop ficar lento (refresh do e-paper,
        // interacoes). O delta da iteracao atual nao reflete isso.
        unsigned long elapsed = now - lastStatsUpdate;
        lastStatsUpdate = now;

        // Dormindo: recupera energia (e custa fome/felicidade).
        // Vale tambem no sono iniciado na tela de status (sem trocar de tela).
        if (sleeping) {
            pet.sleepRecovery(elapsed);
        }

        // Acumula delta e atualiza
        pet.update(elapsed);

        // Verifica evolução
        EvolutionResult evo = pet.checkEvolution();
        if (evo != EVO_NONE) {
            gameState = STATE_EVOLVING;
            display.drawEvolution(pet);
            delay(3000); // Mostra por 3 segundos
            gameState = STATE_IDLE;
            lastDisplayUpdate = 0;
            switch (evo) {
                case EVO_MEGA:
                    Serial.println("MEGA EVOLUCAO!");
                    break;
                case EVO_REVERT:
                    Serial.println("Mega acabou - voltou ao estagio anterior!");
                    break;
                default:
                    Serial.printf("Evoluiu para %s!\n", pet.getStageName());
                    break;
            }
        }

        // Verifica alertas. O aviso NUNCA rouba a tela escolhida pelo
        // usuario (STATUS/SONO...) e so aparece na tela do pet depois de
        // WARNING_GRACE_MS sem interacao: sem isso, com o pet critico,
        // qualquer acao (alimentar, status, trocar pokemon via dev) era
        // desfeita em < 1s, travando os inputs.
        if (pet.isCritical()) {
            if (gameState == STATE_IDLE &&
                (now - lastInteraction >= WARNING_GRACE_MS)) {
                gameState = STATE_WARNING;
                lastDisplayUpdate = 0;
            }
        } else if (gameState == STATE_WARNING) {
            gameState = STATE_IDLE;
            lastDisplayUpdate = 0;
        }
    }

    // ========================================
    // 3. DISPLAY
    // ========================================
    // Feed/play/clean na tela do pet nao forcam rebuild: o drawPetUpdates
    // (parcial) atualiza so as regioes afetadas (barras/cocos/frase) e, se
    // o sprite mudar, ele mesmo faz o redraw completo. Rebuild completo so
    // quando a tela muda (STATUS/RESET) ou a acao veio de outra tela.
    bool forceUpdate;
    if (devAction) {
        forceUpdate = devFromOtherScreen;
    } else if (action == ACTION_STATUS || action == ACTION_RESET) {
        forceUpdate = true;
    } else {
        forceUpdate = !wasPetScreen;
    }
    bool needRedraw = forceUpdate;

    if (gameState == STATE_STATS) {
        // Tela de status: redesenha so quando os dados mudam (1x/min).
        // Os segundos do relogio sao atualizados via refresh parcial.
        uint32_t fp = statsFingerprint();
        if (fp != lastStatsFp) {
            lastStatsFp = fp;
            needRedraw = true;
        }
    } else if (gameState == STATE_SLEEPING) {
        // Modo dormir: alterna Zzz (SLEEP_ZZZ_MIN) e a tela do pokemon
        // (SLEEP_PET_MIN) enquanto nao ha interacao. Sem refresh periodico:
        // so redesenha na troca (ou se o pet morrer dormindo).
        if (pet.isDead()) {
            gameState = STATE_DEAD;
            lastDisplayUpdate = 0;
            needRedraw = true;
        } else {
            unsigned long sleepStepMs =
                (sleepShowZzz ? SLEEP_ZZZ_MIN : SLEEP_PET_MIN) * 60000UL;
            if (now - lastSleepSwitch >= sleepStepMs) {
                sleepShowZzz = !sleepShowZzz;
                lastSleepSwitch = now;
                lastDisplayUpdate = 0;
                needRedraw = true;
            }
        }
    } else if (lastDisplayUpdate == 0 && gameState != STATE_IDLE) {
        // Transicao de tela sem acao (aviso critico, dormir forçado via
        // dev, morte...): desenha a nova tela uma vez, sem refresh
        // periodico. IDLE fica de fora para o refresh parcial continuar
        // sendo usado nas acoes dev na tela do pet.
        needRedraw = true;
    }

    if (needRedraw) {
        lastDisplayUpdate = now;

        switch (gameState) {
            case STATE_IDLE:
                display.setSleepZzz(false);
                display.drawPet(pet);
                break;
            case STATE_STATS:
                display.drawStats(pet, epochNow());
                break;
            case STATE_WARNING:
                display.drawWarning(pet);
                break;
            case STATE_SLEEPING:
                if (sleepShowZzz) {
                    display.setSleepZzz(false);
                    display.drawSleeping();
                } else {
                    display.setSleepZzz(true);
                    display.drawPet(pet);
                }
                break;
            case STATE_DEAD:
                display.drawDead(pet);
                break;
            default:
                break;
        }
    }

    // Tela do pet: barras/cocos/idade com refresh parcial (tambem na fase
    // "pokemon" do modo dormir, para mostrar a recuperacao ao vivo)
    if (gameState == STATE_IDLE || (sleeping && !sleepShowZzz)) {
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
    // O pet comeca a dormir apos o tempo de inatividade, seja na tela do
    // pet ou na de status. Na tela do pet entra o ciclo Zzz normal; na
    // tela de status o sono comeca (recuperacao ativa) mas a tela NAO
    // troca: o usuario continua vendo o status.
    if (!sleeping && !pet.isDead() && !pet.isEgg() &&
        (gameState == STATE_IDLE || gameState == STATE_STATS) &&
        (now - lastInteraction >= SLEEP_AFTER_MS)) {
        sleeping = true;
        lastSleepSwitch = now;
        if (gameState == STATE_IDLE) {
            gameState = STATE_SLEEPING;
            sleepShowZzz = true;      // comeca sempre na tela de Zzz
        }
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
