# Sistema de Cortinas Inteligentes com Controle de Luz

**Autor**: Daniel Silva de Souza  
**Polo**: Bom Jesus da Lapa  
**Data**: 04/06/2025  

Este repositório contém o código base para o projeto de **Automação Residencial: Sistema de Cortinas Inteligentes com Controle de Luz**, desenvolvido como parte da disciplina de IoT. O projeto será implementado na placa **BitDogLab** com o microcontrolador **Raspberry Pi Pico W**, utilizando o protocolo **MQTT** para comunicação. O sistema controlará cortinas de forma inteligente, ajustando sua abertura em níveis intermediários (0% a 100%) com base na intensidade de luz ambiente medida por um sensor LDR, com modos automático e manual. Este é o repositório inicial, contendo o código base fornecido pelo professor (publicação de temperatura e controle de LED via MQTT) e as bibliotecas necessárias para os periféricos. O desenvolvimento do sistema de cortinas será realizado a partir deste ponto.

## Descrição do Projeto

O objetivo é criar um sistema IoT que simule cortinas residenciais inteligentes, ajustando a abertura para regular a luz ambiente. O sistema usará:

- **Sensor LDR** (GPIO 28, ADC2): Mede a luz ambiente (0-100%).
- **LED RGB** (GPIOs 11, 12, 13): Indica o estado da cortina (ex.: azul para fechada, verde para aberta).
- **Matriz de LEDs WS2812** (GPIO 7): Simula o movimento e nível de abertura da cortina.
- **Botões** (GPIOs 5, 6): Alternam modos (automático/manual) e confirmam ajustes.
- **Joystick** (GPIOs 26, 27): Ajusta limites de luz ou abertura manual.
- **Display OLED** (I2C, GPIOs 14, 15): Exibe luz, abertura da cortina e status.
- **Buzzer** (GPIO 10): Fornece feedback sonoro para transições e erros.
- **Wi-Fi/MQTT** (Pico W): Publica dados de luz (`/casa/luz`), estado da cortina (`/casa/cortina/estado`) e recebe comandos (`/casa/cortina/controle`) via IoT MQTT Panel.

O código base atual (adaptado de [pico-examples](https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/mqtt)) publica a temperatura do microcontrolador no tópico `/pico_cortinas/temperature` e controla o LED onboard via `/pico_cortinas/led`. Ele será adaptado para implementar as funcionalidades do sistema de cortinas.

## Estrutura do Repositório

```
├── lib/
│   ├── font.h            # Definições de fontes para display OLED
│   ├── matrizled.c       # Funções para controlar a matriz de LEDs WS2812
│   ├── matrizled.h       # Cabeçalho para matrizled.c
│   ├── ssd1306.c         # Funções para controlar o display OLED SSD1306
│   ├── ssd1306.h         # Cabeçalho para ssd1306.c
│   ├── ws2812.pio        # Programa PIO para LEDs WS2812
├── CMakeLists.txt        # Configuração de compilação com Pico SDK
├── lwipopts_examples_common.h  # Configurações LWIP comuns
├── lwipopts.h            # Configurações LWIP específicas
├── mbedtls_config_examples_common.h  # Configurações MBedTLS comuns
├── mbedtls_config.h      # Configurações MBedTLS específicas
├── main.c                # Código base (publica temperatura, controla LED)
├── README.md             # Documentação do projeto
```

## Periféricos Configurados

Os seguintes periféricos da placa BitDogLab estão mapeados para os pinos abaixo, prontos para uso com as bibliotecas fornecidas:

- **Sensor LDR**: GPIO 28 (ADC2) para leitura de luz ambiente.
- **LED RGB**:
  - Vermelho: GPIO 13
  - Verde: GPIO 11
  - Azul: GPIO 12
- **Matriz de LEDs WS2812**: GPIO 7 (8 LEDs).
- **Botões**:
  - Botão A: GPIO 5 (alternar modos automático/manual)
  - Botão B: GPIO 6 (confirmar ajustes ou entrar em modo BOOTSEL)
- **Joystick**:
  - Eixo X: GPIO 26 (ADC0)
  - Eixo Y: GPIO 27 (ADC1)
- **Display OLED**:
  - I2C: `i2c1`
  - SDA: GPIO 14
  - SCL: GPIO 15
  - Endereço: `0x3C`
- **Buzzer**: GPIO 10
- **Wi-Fi**: Configurado com:
  - SSID: `SUA_REDE_WIFI` (substituir no `main.c`)
  - Senha: `SUA_SENHA_WIFI` (substituir no `main.c`)
  - Broker MQTT: `10.0.0.156` (Termux, porta 1883)
  - Usuário: `admin`
  - Senha: `admin`

## Pré-requisitos

- **Hardware**:
  - Placa BitDogLab com Raspberry Pi Pico W
  - Sensor LDR conectado a GPIO 28 com resistor pull-down (ex.: 10kΩ)
  - Periféricos listados acima
- **Software**:
  - [Pico SDK](https://github.com/raspberrypi/pico-sdk) (versão compatível)
  - [CMake](https://cmake.org/) e compilador ARM (ex.: `arm-none-eabi-gcc`)
  - Broker MQTT (ex.: Mosquitto rodando no Termux, IP `10.0.0.156`)
  - IoT MQTT Panel para monitoramento e controle
- **Bibliotecas**:
  - Todas as bibliotecas necessárias estão na pasta `lib/` (baseadas em [BitDogLab/BitDogLab-C](https://github.com/BitDogLab/BitDogLab-C))

## Como Compilar e Executar

1. **Clone o repositório**:
   ```bash
   git clone https://github.com/SEU_USUARIO/CortinasInteligentes-IoT.git
   cd CortinasInteligentes-IoT
   ```

2. **Configure o ambiente**:
   - Instale o Pico SDK e configure a variável de ambiente `PICO_SDK_PATH`.
   - Certifique-se de que o CMake e o compilador ARM estão instalados.

3. **Atualize as credenciais**:
   - Edite `main.c` e substitua `WIFI_SSID` e `WIFI_PASSWORD` pelas credenciais da sua rede Wi-Fi.

4. **Compile o projeto**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

5. **Carregue o firmware**:
   - Conecte a placa BitDogLab ao computador via USB.
   - Copie o arquivo `main.uf2` (gerado em `build/`) para a placa em modo BOOTSEL.

6. **Teste o código base**:
   - Configure o broker MQTT no Termux (IP `10.0.0.156`, porta 1883, usuário `admin`, senha `admin`).
   - Use o IoT MQTT Panel para monitorar o tópico `/pico_cortinas/temperature` e controlar o LED via `/pico_cortinas/led` (enviando "On" ou "Off").

## Desenvolvimento Futuro

O código base será adaptado para implementar o sistema de cortinas inteligentes com as seguintes funcionalidades:

- **Modo Automático**: Ajustar a abertura da cortina (0%, 25%, 50%, 75%, 100%) com base na luz ambiente (limites de 20% e 80%).
- **Modo Manual**: Controlar a abertura via IoT MQTT Panel (ex.: "abrir:30") ou joystick.
- **Feedback**:
  - LED RGB: Cores graduais (azul para fechada, verde para aberta).
  - Matriz de LEDs: Simular abertura proporcional com animações.
  - OLED: Exibir luz (%), abertura (%), e status.
  - Buzzer: Tons para transições e erros.
- **MQTT**:
  - Publicar: `/casa/luz` (nível de luz), `/casa/cortina/estado` (abertura).
  - Assinar: `/casa/cortina/controle` (comandos como "abrir:30", "intensidade:50").

As bibliotecas na pasta `lib/`) fornecem suporte completo para os periféricos:
- `matrizled.h`: Controle da matriz WS2812.
- `ssd1306.h`: Interface com o OLED.
- `font.h`: Fontes para texto no OLED.
- `ws2812.pio`: Programa PIO para LEDs WS2812.

## Licença

Este projeto utiliza código adaptado de [pico-examples](https://github.com/raspberrypi/pico-examples) e bibliotecas do repositório [BitDogLab/BitDogLab-C](https://github.com/BitDogLab/BitDogLab-C), sob licença MIT.

## Contato

Para dúvidas ou contribuições, entre em contato com Daniel Silva de Souza via [e-mail ou outro canal].