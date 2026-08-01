#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// CONFIGURACAO DO PROJETO
//
// Copie este arquivo para "config.h" e ajuste os valores para o
// seu dispositivo:
//
//   cp src/config.example.h src/config.h
//
// O arquivo config.h NAO e versionado (contem credenciais).
// ============================================================

// ============================================================
// DISPLAY CONFIG - WeAct 3.7" E-Paper (240x416, UC8253)
// ============================================================
// Pinos SPI (WeAct default: SCLK=18, MOSI=23)
#define EP_CS   5   // Chip Select
#define EP_DC   17  // Data/Command
#define EP_RST  16  // Reset
#define EP_BUSY 4   // Busy

// ============================================================
// GRAY MODE - 4 niveis de cinza (0b00=preto, 0b01=cinza escuro,
// 0b10=cinza claro, 0b11=branco). Usa TSSET=0x5A (4 gray) do
// sample oficial Good Display A32-GDEY037T03-FP4G.
// ============================================================
#define GRAY_MODE 1

// ============================================================
// BUTTON CONFIG
// ============================================================
#define BTN_FEED   13  // Botão 1 - Alimentar (GPIO com pull-up)
#define BTN_PLAY   12  // Botão 2 - Brincar  (GPIO com pull-up)
#define BTN_STATUS 14  // Botão 3 - Status   (GPIO com pull-up)

// Debounce (ms)
#define DEBOUNCE_MS     50
#define LONG_PRESS_MS   1000

// ============================================================
// GAME CONFIG
// ============================================================
#define STATS_MAX       100
#define STATS_MIN       0

// Decaimento dos stats (unidades por minuto)
#define HUNGER_DECAY      2   // Fome cai 2 por minuto
#define HAPPY_DECAY       1   // Felicidade cai 1 por minuto
#define HEALTH_DECAY      5   // Saúde cai 5 por minuto (quando critico)

// ============================================================
// EGG CONFIG
// ============================================================
#define WARMTH_DECAY         3   // Calor do ovo cai 3/min
#define WARM_AMOUNT         20   // Aquecer recupera 20 de calor
#define HATCH_WARMTH_MIN    60   // Calor mínimo para chocar
#define HATCH_TIME_MINUTES   2   // Minutos aquecido necessários

// Efeito das ações
#define FEED_AMOUNT      20   // Alimentar recupera 20 de fome
#define PLAY_AMOUNT      15   // Brincar recupera 15 de felicidade

// Thresholds de evolução (em minutos)
#define EVOLVE_SCYTHER_TIME  3   // Scyther -> Scizor/Kleavor após 3 min
#define EVOLVE_SCIZOR_TIME   8   // Scizor -> Mega Scizor após 8 min
#define EVOLVE_PICHU_TIME    3   // Pichu -> Pikachu após 3 min
#define EVOLVE_PIKACHU_TIME  8   // Pikachu -> Raichu após 8 min
#define EVOLVE_RAICHU_TIME   12  // Raichu -> Mega Raichu após 12 min
#define EVOLVE_MIN_HAPPY     60  // Felicidade mínima para evoluir
#define EVOLVE_MIN_FEED      40  // Fome mínima para evoluir

// Intervalos
#define STATS_UPDATE_MS    1000   // Atualiza stats a cada 1s
#define DISPLAY_REFRESH_MS 5000   // Força refresh da tela a cada 5s
#define SAVE_INTERVAL_S    30     // Salva estado a cada 30s
#define SLEEP_AFTER_MS     60000  // Vai dormir após 60s inativo

// ============================================================
// EVOLUTION STAGES
// ============================================================
enum PokemonStage {
    STAGE_EGG = 0,
    STAGE_SCYTHER = 1,
    STAGE_SCIZOR = 2,
    STAGE_KLEAVOR = 3,
    STAGE_MEGASCIZOR = 4,
    STAGE_PICHU = 5,
    STAGE_PIKACHU = 6,
    STAGE_RAICHU = 7,
    STAGE_MEGARAICHUX = 8,
    STAGE_MEGARAICHUY = 9
};

// Nomes
static const char* STAGE_NAMES[] = {
    "Ovo",
    "Scyther",
    "Scizor",
    "Kleavor",
    "Mega Scizor",
    "Pichu",
    "Pikachu",
    "Raichu",
    "Mega Raichu X",
    "Mega Raichu Y"
};

// ============================================================
// DEMO MODE - Cicla estágios e telas com os botões (sem jogar)
// FEED = próximo estágio | PLAY = próxima tela | STATUS = Stats
// ============================================================
#define DEMO_MODE 1

// ============================================================
// WIFI - Botoes virtuais via browser (http://<ip-do-esp32>)
// ============================================================
#define WIFI_SSID   "seu-wifi"     // <-- configure aqui
#define WIFI_PASS   "sua-senha"    // <-- configure aqui

// Fallback: hotspot proprio do ESP32 (so 2.4GHz)
#define AP_SSID     "Tamagoshi"
#define AP_PASS     "troque-me"    // <-- configure aqui

// ============================================================
// RELOGIO - Tempo real via NTP (o jogo evolui mesmo desligado)
// ============================================================
#define NTP_SERVER          "pool.ntp.org"
#define TZ_OFFSET_SEC       (-3 * 3600)      // GMT-3 (Brasilia, sem horario de verao)
#define MAX_CATCHUP_MINUTES (7 * 24 * 60)    // Limite do catch-up apos dias desligado

// Relogio padrao quando nao ha internet (atualizado pelo NTP quando possivel)
#define DEFAULT_TIME_YEAR   2026
#define DEFAULT_TIME_MONTH  1
#define DEFAULT_TIME_DAY    1
#define DEFAULT_TIME_HOUR   12
#define DEFAULT_TIME_MINUTE 0
#define DEFAULT_TIME_SECOND 0

// ============================================================
// DISPLAY STATE
// ============================================================
enum DisplayMode {
    DISPLAY_PET,       // Mostra o Pokémon
    DISPLAY_STATS,     // Mostra status detalhado
    DISPLAY_EVOLVING,  // Animação de evolução
    DISPLAY_SLEEPING,  // Modo dormindo
    DISPLAY_WARNING    // Alerta de saúde baixa
};

#endif
