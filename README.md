# Tamagoshi (ESP32 + E-Paper)

Bichinho virtual com ESP32 e display e-paper de 3.7" (WeAct 240x416, UC8253)
com 4 níveis de cinza. O ovo choca uma criatura base (com raridade
configurável) que evolui por linhas evolutivas definidas em arquivo de
configuração.

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

Com `DEMO_MODE 1` (padrão), os botões viram navegação — ótimo para testar
sprites sem jogar:

- FEED = próximo estágio | PLAY = próxima tela | STATUS = stats

### Jogando

Com `DEMO_MODE 0`:

- **FEED** alimenta (no ovo, aquece)
- **PLAY** brinca (no ovo, aquece um pouco)
- **STATUS** mostra os stats
- Ovo: mantenha o calor ≥ `HATCH_WARMTH_MIN` por `HATCH_TIME_MINUTES` para chocar

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
     (e tempos de evolução em `EVOLVE_*_TIME`).
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
- **Fluxo de nascimento completo**: finalizar o uso das duas linhas no modo real
  (fora do demo).
- **Variações de humor**: gerar sprites SAD/HAPPY de verdade, em vez do alias
  para o mesmo sprite.
- **Bateria/baixo consumo**: deep sleep entre interações (o e-paper mantém a
  imagem sem energia).

## Estrutura

```
src/
  config.example.h   # copie para config.h (não é versionado)
  Evolution.h        # linhas evolutivas e probabilidades
  Pokemon.{h,cpp}    # lógica do jogo (stats, evolução, save NVS)
  EPDisplay.{h,cpp}  # desenho no e-paper (1bpp + 4-gray)
  main.cpp           # loop, botões, web server
  sprites/           # gerados (1bpp)
  sprites_gray/      # gerados (4-gray, 2bpp)
tools/
  rebuild_sprites.py # gerador de sprites
  sprites/           # PNGs de origem
```
