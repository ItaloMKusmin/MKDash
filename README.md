# MKAuto Dashboard

Painel automotivo para a GUITION JC1060P470C_I_W_Y (ESP32-P4 + C6, 1024 × 600), com
leitura futura da ECU por um Vgate iCar Pro BLE 4.0. Primeiro veículo: Corsa Wind 2002 1.0 8V.

Estado atual:

- Placa funcionando: display JD9165 (painel novo, reset GPIO5), touch GT911, PSRAM 32 MB.
- Dashboard MKAuto: conta-giros inclinado, RPM, velocidade, marcha calculada e 4 cards
  configuráveis (toque longo, com o veículo parado), persistidos em NVS.
- Dados vindos de **telemetria simulada** (`CONFIG_MK_TELEMETRY_SIM`). BLE/ELM ainda não
  integrados: a placa está sem antena e o protocolo do Corsa está em levantamento.

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `src/` | `app_main`, informação de build e tela técnica do M0 |
| `lib/bsp_guition` | display, touch, backlight e LVGL da placa |
| `lib/telemetry` | catálogo de sinais, último valor, qualidade e envelhecimento |
| `lib/telemetry_sim` | ciclo de condução sintético para desenvolver a interface |
| `lib/gear`, `lib/derived` | marcha estimada pela razão RPM/velocidade |
| `lib/storage` | configurações em NVS |
| `lib/ui` | tema, conta-giros, cards, seletor de sinais, fontes e logo |
| `tools/asset_conversion` | geração das fontes e da logo (não roda no build) |
| `test/` | testes Unity no host e no dispositivo |

## Ambiente fixado

- ESP-IDF **v5.5.5**, commit `b774170ff46c393eeb5e495ea37936038d3f4f4f`.
- Target: `esp32p4`; aplicação C11.
- Instalação Windows encontrada: `C:\esp\v5.5.5\esp-idf`, ferramentas em `C:\Espressif\tools`.
- Python 3.11.15 do ambiente IDF, CMake 3.30.2, Ninja 1.12.1 e GCC RISC-V `esp-14.2.0_20260121`.
- Dependências externas de LCD/LVGL/rádio serão fixadas nos respectivos marcos.

## Build sem acessar hardware

Nesta instalação Windows, executar a partir da raiz:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -CleanConfig
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -DeviceTests
```

O script configura o ambiente apenas no processo filho e executa `reconfigure`, `build`
e `size-components`. Não executa flash, monitor, reset ou descoberta de porta serial.
Aceita `-IdfPath` e `-ToolsPath` para instalações equivalentes em outros diretórios.
`-CleanConfig` usa uma pasta nova `build-verify-*` e regenera `sdkconfig` pelos defaults,
preservando o build normal. O script usa os caminhos de ferramentas do instalador EIM.
`-DeviceTests` compila separadamente em `build-device-tests/`, com testes Unity que
executam uma vez no boot. O build normal força `MK_BUILD_TESTS=OFF` e não inclui testes.
Para outro layout, use o terminal oficial ESP-IDF 5.5.5 e os comandos abaixo:

```text
idf.py -DIDF_TARGET=esp32p4 reconfigure
idf.py build
idf.py size-components
```

Em um projeto ainda não configurado, `idf.py set-target esp32p4` também pode definir
o target. Para limpar os artefatos normais, use `idf.py fullclean`; não é necessário
apagar `sdkconfig` a cada build.

Artefatos: `build/mkauto_dashboard.bin`, `build/bootloader/bootloader.bin` e
`build/partition_table/partition-table.bin`. A aplicação tem partição única de 4 MiB;
a flash de 16 MiB é baseline documental ainda dependente de confirmação física.
PSRAM e periféricos permanecem sem configuração específica antes do M0.
O default deste IDF gera imagem para revisão mínima **3.1** do P4. Confirmar a revisão
física e ajustar a configuração antes da primeira gravação; esta imagem ainda não é
uma release validada para a unidade conectada.

## Gravação

A unidade de referência é ESP32-P4 **v1.3**; o `sdkconfig.defaults.esp32p4` seleciona
revisões < 3.0. No terminal ESP-IDF:

```text
idf.py -p COM_PORT flash
idf.py -p COM_PORT monitor
```

Substitua `COM_PORT` pela porta confirmada. O monitor pode reiniciar a placa.
Nenhum desses comandos faz parte do script de build.

## Qualidade e rastreabilidade

O código próprio compila com `-Wall -Wextra -Werror -Wformat=2 -Wshadow`.
Convenções de edição estão em `.editorconfig`. Execute `reconfigure` antes de cada
build para atualizar a identificação Git; o script já faz isso. Sem `.git` ou sem
commit disponível, commit e dirty são explicitamente `unknown`.

Testes no host: `test_build_info_*` (metadados de build), `test_telemetry` (qualidade e
envelhecimento dos sinais) e `test_gear` (estimativa de marcha). `test_build_info.c` também
roda no dispositivo com `tools/build.ps1 -DeviceTests`.
No host, os dois serviços de versão do IDF são substituídos por stubs; no dispositivo,
os testes usam o descritor real da aplicação e a API real do IDF. O host cobre
metadados de Git ausente, limpo e com alterações. Para executar no Windows instalado:

```powershell
$env:PATH = 'C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\msys64\ucrt64\bin;' + $env:PATH
cmake -S test/host -B build-host -G Ninja -DMK_IDF_PATH=C:/esp/v5.5.5/esp-idf
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Os testes no host exigem GCC nativo (neste ambiente: MSYS2 UCRT64). Em 2026-11-06 as
5 suítes passaram (`ctest`: 5/5).
Os testes de parser ELM e conversão da ECU entram junto com essas camadas.

## Referências visuais

- `assets/brand/mk_logo_source.png`: monograma MK usado na interface (cores e cortes diagonais).
- `reference/ChatGPT Image 8 de set. de 2026, 09_34_11.png`: arte original do símbolo MK.
- A foto de referência da FT450 usada para a hierarquia das informações fica apenas
  localmente (`reference/image.png`, ignorada pelo Git); nenhum elemento dela é reproduzido.

Fontes: Montserrat, SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).

Requisitos: [arquitetura](docs/ARCHITECTURE.md).
Etapas, evidências e bloqueios: [plano de implementação](docs/IMPLEMENTATION_PLAN.md).
