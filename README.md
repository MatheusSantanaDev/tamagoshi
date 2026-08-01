# Tamagoshi (ESP32 + E-Paper)

Bichinho virtual com ESP32 e display e-paper de 3.7" (WeAct 240x416, UC8253)
com 4 níveis de cinza. O ovo choca uma criatura base (com raridade
configurável) que evolui por linhas evolutivas, podendo atingir formas
**Mega** com cuidados constantes — ou morrer de velhice/descuido.

## Hardware

- ESP32 (SPI: SCLK=18, MOSI=23)
- E-Paper WeAct 3.7" 240x416 (UC8253) — CS=5, DC=17, RST=16, BUSY=4
- 3 botões com pull-up: FEED=13, PLAY=12, STATUS=14
- Configurável em `src/config.h`

## Como usar

```bash
cp src/config.example.h src/config.h   # preencha WIFI_SSID/WIFI_PASS
pio run -t upload                      # compila e envia via USB
```

Abra o browser em `http://<ip-do-esp32>` para os botões virtuais.

### Demo mode

Com `DEMO_MODE 1` (padrão), o dispositivo simula o jogo — ótimo para testar
sem esperar horas:

- No **ovo**: o tempo passa acelerado (1 min de jogo a cada ~4s) — o `lvl`
  sobe, o calor decai e a incubação avança; **FEED aquece** o ovo. Quando a
  incubação completa, choca sozinho.
- Fora do ovo: **FEED** = próximo estágio | **PLAY** = próxima tela |
  **STATUS** = stats | **RESET** (long-press FEED) = volta ao ovo.

### Jogando

Com `DEMO_MODE 0`, o jogo segue o tempo real:

- **FEED** alimenta (no ovo, aquece +20 de calor)
- **PLAY** brinca
- **STATUS** mostra a tela de stats
- O tempo real vira **lvl** (idade em minutos, atualiza a cada minuto com
  refresh parcial) e faz os stats decaírem; salva automaticamente na NVS.

## Ovo e incubação

O calor do ovo cai `WARMTH_DECAY` (3) por minuto e só o **FEED** aquece
(`WARM_AMOUNT` 20). O calor não mata o ovo: ele só controla a **velocidade**
da incubação (nunca regride):

| Calor          | Incubação          |
|----------------|--------------------|
| ≥ `WARMTH_FAST_MIN` (60) | normal (+1 min/min) |
| `WARMTH_SLOW_MIN` (30) a 59 | devagar (+1 min a cada 2 min) |
| < 30           | pausa              |

Com `HATCH_TIME_MINUTES` (5) de incubação acumulados, o ovo choca uma
criatura base (pesos em `HATCH_CHANCES`).

## Classes e evolução

Três classes de linha (`src/Evolution.h`):

- **Com baby**: `Egg → Baby → S1 → S2` (ex.: Pichu → Pikachu → Raichu)
- **Sem baby**: `Egg → S1 → S2 → S3` (ex.: Scyther → Scizor → Mega Scizor)
- **Sem evolução**: `Egg → Pokémon`

Tempos por classe (`CLASS_TIMES_MIN` em `src/config.h`): ovo 5 min, baby 4h,
1º estágio 24h, 2º estágio 36h, 3º estágio 48h. Evoluções seguem os pesos em
`EVOLUTION_RULES`.

### Mega

No estágio final (ex.: Scizor, Raichu), aos 36h, se os requisitos forem
atendidos, o Pokémon vira Mega:

- Felicidade ≥ 85, Saúde ≥ 85, Higiene ≥ 80, Sono ≥ 70
- No máximo `MEGA_REQ_MAX_CRITICAL_MIN` (90) minutos críticos no estágio

A Mega é **temporária e reversível**: dura no máximo 12h consecutivas e
reverte ao estágio final. São dois relógios independentes — o relógio normal
do estágio final (48h, pausa durante a Mega) e o relógio acumulado de Mega
(50h, nunca reinicia). A morte de velhice acontece quando o primeiro limite
é atingido; dá para virar Mega várias vezes.

## Personalidade e humor

Na chocagem o ovo sorteia uma **personalidade oculta** (9 tipos em
`PERSONALITIES`), que multiplica os decaimentos e efeitos das ações
(ex.: Guloso come mais rápido, Dorminhoco dorme melhor). Só aparece no log
serial — o jogador descobre pelos números.

O **humor** é um campo único e priorizado (Doente > Faminto > Cansado >
Irritado > Triste > Feliz > Neutro), calculado dos stats via limiares
`MOOD_*`. A tela de status mostra Pokémon, Idade, Humor e o histórico
(perdidos/vitórias/vidas curta/longa).

## Linhas evolutivas

`src/Evolution.h` é o arquivo de configuração das linhas:

- `HATCH_CHANCES`: pesos de nascimento — maior peso = mais comum
- `EVOLUTION_RULES`: para cada estágio, a lista de alvos com pesos
  (soma dos pesos = probabilidade)

## Como adicionar uma sprite ao jogo

1. **Adicione o PNG** em `tools/sprites/` (ex.: `meuwm.png`), de preferência com
   fundo branco e a arte preenchendo o quadro.
2. **Registre o estágio** no topo de `tools/rebuild_sprites.py`:
   - `STAGES`: linha `("meuwm", "MEUWM", (160, 160))` (base do arquivo, prefixo C,
     tamanho).
   - `GRAY_MODES`: modo de conversão para cinza — `("lum", wthr, gthr)` para
     sprites claros (limiares de luminância) ou `("semantic", ...)` para sprites
     escuros (mapeamento semântico + contornos cel-shading). Parâmetros extras
     (`band_head`/`band_zone`) evitam borrão em regiões de detalhe.
   - Opcional: `CROP` para recortar margens, `BOTTOM_ALIGN` para arte em
     paisagem.
3. **Regenere**: `python3 tools/rebuild_sprites.py` — gera
   `src/sprites/meuwm.h` (1bpp) e `src/sprites_gray/meuwm.h` (4-gray).
4. **Integre no jogo**:
   - `src/config.h`: novo valor no enum `PokemonStage` + nome em `STAGE_NAMES`
     e, se for estágio final, em `isFinalStage`/`isMegaStage` (Pokemon.cpp).
   - `src/Evolution.h`: entrada no `HATCH_CHANCES` (se for base) ou
     `EVOLUTION_RULES` (se for evolução).
   - `src/Pokemon.cpp`: casos nos `switch` de `getCurrentSprite`/
     `getCurrentGraySprite`.
   - `src/EPDisplay.cpp`: caso no `getSpriteSize`.

## Próximos passos

- **Animações de evolução**: transição visual no momento da evolução (atualmente
  troca direta de sprite).
- **Sono e ciclo dia/noite**: o bichinho dorme e os stats decaem conforme horário
  real.
- **Níveis e XP**: interações dão experiência e liberam evoluções alternativas.
- **Variações de humor**: gerar sprites SAD/HAPPY de verdade, em vez do alias
  para o mesmo sprite.
- **Bateria/baixo consumo**: deep sleep entre interações (o e-paper mantém a
  imagem sem energia).
- **Balanceamento**: calibrar requisitos de Mega e multiplicadores de
  personalidade jogando o modo real.

## Estrutura

```
src/
  config.example.h   # copie para config.h (não é versionado)
  Evolution.h        # linhas evolutivas e probabilidades
  Pokemon.{h,cpp}    # lógica do jogo (stats, evolução, Mega, NVS)
  EPDisplay.{h,cpp}  # desenho no e-paper (1bpp + 4-gray, refresh parcial)
  EPGray.{h,cpp}     # camada de cinza 2bpp + janelas parciais
  TimeKeeper.{h,cpp} # relógio de tempo real (NTP + fallback)
  Input.{h,cpp}      # botões (curto/longo)
  main.cpp           # loop, estados, web server
  sprites/           # gerados (1bpp)
  sprites_gray/      # gerados (4-gray, 2bpp)
tools/
  rebuild_sprites.py # gerador de sprites
  sprites/           # PNGs de origem
```
