#ifndef CONFIG_H
#define CONFIG_H

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

// Novos stats (0-100)
#define ENERGY_DECAY      1   // Energia cai 1/min (sobe dormindo)
#define SLEEP_DECAY       1   // Sono cai 1/min (sobe dormindo)
#define HYGIENE_DECAY     1   // Higiene cai 1/min
#define DIRT_ACCUM_PER_MIN 1  // Coco acumula 1/min (e mais ao alimentar)

// Custo/efeitos das acoes
#define PLAY_ENERGY_COST  10  // Brincar gasta energia
#define DIRT_PER_FEED     15  // Cada alimentacao gera coco

// Recuperacao dormindo (por minuto)
#define SLEEP_RECOVERY_ENERGY 15
#define SLEEP_RECOVERY_SLEEP  20

// Limiares criticos
#define ENERGY_CRITICAL   15  // Abaixo disso, felicidade cai mais rapido
#define SLEEP_CRITICAL    15  // Abaixo disso, felicidade cai mais rapido
#define HYGIENE_CRITICAL  15  // Abaixo disso, saude cai
#define DIRT_CRITICAL     60  // Acima disso, saude cai

// Cocos na tela do pet: nivel = sujeira / DIRT_LEVEL_STEP, capado em 2
// (0 = sem coco, 1 = 1 coco, 2+ = 1 coco de cada lado)
#define DIRT_LEVEL_STEP   25

// ============================================================
// CLASSES E TEMPOS DE EVOLUCAO (minutos)
// ============================================================
// Linhas com baby:  Egg -> Baby -> Stage 1 -> Stage 2
// Linhas sem baby:  Egg -> Stage 1 -> Stage 2 -> Stage 3
// A evolucao normal ocorre ao permanecer o tempo da classe no estagio.
enum PokemonClass {
    CLASS_EGG = 0,
    CLASS_BABY,
    CLASS_STAGE1,
    CLASS_STAGE2,
    CLASS_STAGE3
};

static const int CLASS_TIMES_MIN[] = {
    5,        // CLASS_EGG:    choca em 5 min
    4 * 60,   // CLASS_BABY:   4 h
    24 * 60,  // CLASS_STAGE1: 24 h
    36 * 60,  // CLASS_STAGE2: 36 h
    48 * 60,  // CLASS_STAGE3: 48 h
};

// ============================================================
// MEGA EVOLUTION - requisitos para evoluir do estagio final
// ============================================================
#define MEGA_REQ_HAPPINESS 85
#define MEGA_REQ_HEALTH    85
#define MEGA_REQ_HYGIENE   80
#define MEGA_REQ_SLEEP     70
// "Poucos periodos criticos": maximo de minutos em estado critico
// acumulados no estagio (reseta a cada entrada em Mega)
#define MEGA_REQ_MAX_CRITICAL_MIN 90

// ============================================================
// RELOGIOS DE VIDA - o estagio final normal e a Mega possuem
// relogios de vida INDEPENDENTES:
// - Estagio final normal (ex.: Raichu): vida de FINAL_STAGE_LIFE_MIN.
//   PAUSA enquanto o Pokemon esta em Mega; volta a contar ao
//   destransformar. Atingir o limite = morte por velhice.
// - Mega: vida total acumulada de MEGA_LIFE_TOTAL_MIN, contando APENAS
//   o tempo transformado (pausa fora da Mega, nao reinicia). Atingir
//   o limite = morte por velhice como Mega.
// - Cada transformacao Mega dura no maximo MEGA_MAX_CONTINUOUS_MIN
//   consecutivos; ao fim, reverte ao estagio final e pode entrar
//   de novo (o acumulado da Mega continua de onde parou).
// Morre o que acontecer primeiro entre os dois relogios.
// ============================================================
#define FINAL_STAGE_LIFE_MIN   (48 * 60)   // 48 h
#define MEGA_LIFE_TOTAL_MIN    (50 * 60)   // 50 h
#define MEGA_MAX_CONTINUOUS_MIN (12 * 60)  // 12 h

// ============================================================
// HUMOR (prioridade: Doente > Faminto > Cansado > Irritado >
// Triste > Feliz > Neutro)
// ============================================================
#define MOOD_HEALTH_LOW   20  // Saude baixa     -> Doente
#define MOOD_HUNGER_LOW   15  // Fome muito alta -> Faminto
#define MOOD_ENERGY_LOW   20  // Energia baixa + sono baixo -> Cansado
#define MOOD_SLEEP_LOW    20
#define MOOD_HYGIENE_LOW  20  // Higiene baixa   -> Irritado
#define MOOD_HAPPY_LOW    35  // Felicidade baixa + fome alta -> Triste
#define MOOD_HUNGER_MID   35
#define MOOD_HAPPY_HIGH   60  // Felicidade alta + fome baixa + energia alta -> Feliz
#define MOOD_HUNGER_OK    60
#define MOOD_ENERGY_HIGH  60

// ============================================================
// PERSONALIDADE - sorteada na chocagem, oculta do jogador.
// Modifica discretamente os atributos (100 = normal).
// ============================================================
enum Personality {
    P_GULOSO = 0,
    P_PREGUICOSO,
    P_BRINCALHAO,
    P_AGITADO,
    P_CARINHOSO,
    P_TEIMOSO,
    P_DORMINHOCO,
    P_RESISTENTE,
    P_FRAGIL,
    P_COUNT
};

struct PersonalitySpec {
    const char* name;
    int hungerMult;      // Decaimento da fome (%)
    int happyMult;       // Decaimento da felicidade (%)
    int energyMult;      // Decaimento da energia (%)
    int sleepMult;       // Decaimento do sono (%)
    int hygieneMult;     // Decaimento da higiene (%)
    int healthMult;      // Decaimento da saude quando critico (%)
    int feedHappyMult;   // Felicidade ganha ao comer (%)
    int playHappyMult;   // Felicidade ganha ao brincar (%)
    int playEnergyMult;  // Energia gasta ao brincar (%)
    int sleepRecMult;    // Recuperacao dormindo (%)
};

static const PersonalitySpec PERSONALITIES[P_COUNT] = {
    // nome         fome fel ener sono higi saude comer brinc custo sono
    { "Guloso",      115, 100, 100, 100, 100, 100, 150, 100, 100, 100 },
    { "Preguicoso",  100, 100,  85, 100, 100, 100, 100,  75, 100, 100 },
    { "Brincalhao",  100, 100, 100, 100, 100, 100, 100, 150, 115, 100 },
    { "Agitado",     100, 120, 115, 100, 100, 100, 100, 100, 100, 100 },
    { "Carinhoso",   100,  80, 100, 100, 100,  75, 100, 100, 100, 100 },
    { "Teimoso",      85, 100, 100, 100, 100, 100, 100,  75, 100, 100 },
    { "Dorminhoco",  100, 100, 100, 120, 100, 100, 100, 100, 100, 125 },
    { "Resistente",  100, 100, 100, 100, 100,  75, 100, 100, 100, 100 },
    { "Fragil",      100, 110, 100, 100, 100, 125, 100, 100, 100, 100 },
};

// Efeitos das personalidades:
// - Guloso:      come mais, felicidade sobe mais ao comer
// - Preguicoso:  energia cai mais devagar, brinca com menos animo
// - Brincalhao:  felicidade sobe mais ao brincar (gasta mais energia)
// - Agitado:     entedia mais facil, gasta mais energia
// - Carinhoso:   felicidade cai devagar, saude resiste mais
// - Teimoso:     come menos, brinca com menos animo
// - Dorminhoco:  dorme mais (cai mais rapido e recupera mais)
// - Resistente:  saude quase nao cai
// - Fragil:      saude cai mais facil

#define FEED_HAPPY_BASE 10  // Felicidade base ganha ao comer

// ============================================================
// EGG CONFIG
// ============================================================
#define WARMTH_DECAY         3   // Calor do ovo cai 3/min
#define WARM_AMOUNT         20   // Alimentar aquece 20 de calor
#define HATCH_TIME_MINUTES  20   // Minutos de incubacao para chocar
#define WARMTH_FAST_MIN     60   // Calor >= 60: incubacao normal (+1/min)
#define WARMTH_SLOW_MIN     30   // Calor 30..59: incubacao devagar; <30: pausa

// Efeito das ações
#define FEED_AMOUNT      20   // Alimentar recupera 20 de fome
#define PLAY_AMOUNT      15   // Brincar recupera 15 de felicidade

// Intervalos
#define STATS_UPDATE_MS    1000   // Atualiza stats a cada 1s
#define SAVE_INTERVAL_S    30     // Salva estado a cada 30s
#define SLEEP_AFTER_MS     60000  // Vai dormir após 60s inativo

// Modo dormir: alterna as telas enquanto nao ha interacao
#define SLEEP_ZZZ_MIN      2      // 2 minutos mostrando "Zzzzzz..."
#define SLEEP_PET_MIN      1      // 1 minuto mostrando o pokemon

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
#define DEMO_MODE 0

// ============================================================
// WIFI - Botoes virtuais via browser (http://<ip-do-esp32>)
// ============================================================
#define WIFI_SSID   "login"
#define WIFI_PASS   "senha"

// Fallback: hotspot proprio do ESP32 (so 2.4GHz)
#define AP_SSID     "Tamagoshi"
#define AP_PASS     "tamagoshi"

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

// Relogio da tela de status: atualiza apenas a regiao dos digitos
// (refresh parcial) a cada segundo, sem reconstruir a tela inteira.
// Se o display apresentar artefatos/ghosting, desligue (0).
#define CLOCK_PARTIAL_UPDATE 1

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
