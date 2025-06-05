## Ficha de Proposta de Projeto

**Nome do Aluno:** Daniel Silva de Souza  
**Polo:** Bom Jesus da Lapa  
**Data:** 04/06/2025  

**Título do Projeto:**  
Sistema de Cortinas Inteligentes IoT

### Objetivo Geral

Desenvolver um sistema IoT utilizando a placa BitDogLab com o microcontrolador RP2040 para simular o controle de cortinas e iluminação em uma casa inteligente, ajustando a iluminação ambiente de dois cômodos ("sala" e "quarto1") para um valor-alvo (padrão 65%) via sensor LDR. O sistema utiliza o protocolo MQTT para comunicação, com interface no IoT MQTT Panel e MQTT Explorer, e oferece feedback visual e automação para conforto e eficiência energética.

### Descrição Funcional

#### Funcionamento do Projeto

O projeto implementa um sistema de automação residencial que simula o controle de cortinas e iluminação em dois cômodos ("sala" e "quarto1") utilizando a placa BitDogLab. A cortina é simulada pela matriz de LEDs WS2812, onde o brilho (0–100%) representa a claridade entrando no cômodo, e a iluminação é simulada por LEDs RGB, que acendem em branco para indicar luz ligada. O sistema mede a iluminação ambiente com um LDR e ajusta automaticamente os componentes para atingir um valor-alvo configurável (padrão 65%). Ele opera em três modos: manual, automático e dormir, com controle via protocolo MQTT e interface no IoT MQTT Panel (Android) e MQTT Explorer (PC).


#### Modos de Operação

##### Modo Manual

Permite controle direto via comandos MQTT:
- `/casa/[comodo]/janela/set <valor>`: Ajusta o brilho da matriz WS2812 (0–100%), simulando a abertura da cortina.
- `/casa/[comodo]/janela/abrir on/off`: Liga (100%) ou desliga (0%) a matriz.
- `/casa/[comodo]/luz/ligar on/off`: Acende (branco) ou apaga os LEDs RGB.  
Comandos são enviados via IoT MQTT Panel ou MQTT Explorer.

##### Modo Automático

Ajusta o brilho da matriz e o estado dos LEDs RGB com base na leitura do LDR para atingir `iluminacao_alvo` (padrão 65%):
- Se a iluminação ambiente for menor que o alvo, aumenta o brilho da matriz (simulando abertura da cortina).
- Se ainda insuficiente com matriz no máximo, acende os LEDs RGB.
- Se a iluminação for maior que o alvo, reduz o brilho da matriz e desliga os LEDs RGB.  
Ativado via `/casa/[comodo]/modo auto`.

##### Modo Dormir

Simula um ambiente escuro, desativando a matriz WS2812 e LEDs RGB:
- Ativado via `/casa/[comodo]/modo_dormir on`.
- Desativa o modo automático e ignora comandos manuais.
- Desativado via `/casa/[comodo]/modo_dormir off`, retornando ao modo automático.  
Cada cômodo opera de forma independente.

#### Seleção de Cômodo

O comando `/casa/select <comodo>` alterna entre "sala" e "quarto1", definindo o cômodo ativo para automação e publicação de estados.

#### Monitoramento e Interface

O sistema publica estados periodicamente (a cada 2 segundos) nos tópicos:
- `/casa/[comodo]/estado`: JSON com iluminação, brilho da matriz, estado dos LEDs RGB, modo, e `iluminacao_alvo`.
- `/casa/[comodo]/janela/pos`: Brilho da matriz (0–100%).
- `/casa/[comodo]/janela/estado`: "on" ou "off".
- `/casa/[comodo]/luz/estado`: "on" ou "off".
- `/casa/[comodo]/luz`: Iluminação ambiente medida pelo LDR.

A interface é feita via:
- **IoT MQTT Panel**: Interface gráfica no Android para enviar comandos e visualizar estados.
- **MQTT Explorer**: Monitoramento detalhado de tópicos no PC.

**Nota**: Uma foto anexada exibe a interface do IoT MQTT Panel e MQTT Explorer, mostrando os tópicos publicados e os controles configurados para "sala" e "quarto1".

#### Lógica por Trás das Funcionalidades

O sistema utiliza uma arquitetura baseada em MQTT:
- **Leitura do LDR**: Função `read_ldr()` faz 100 amostras no ADC (GPIO 28), calcula a média, e converte para porcentagem (0–100%).
- **Automação**: Função `automacao_iluminacao()` ajusta o brilho da matriz e LEDs RGB com incrementos adaptativos (5%, 2%, 1%) para estabilidade.
- **Controle MQTT**: Função `mqtt_incoming_data_cb()` processa comandos por cômodo, aplicando restrições de modo.
- **Publicação**: Função `publish_all_states()` atualiza os estados periodicamente.

O broker Mosquitto roda no celular via Termux (IP `10.0.0.196`, usuário `admin`, senha `admin`), e a BitDogLab se conecta via Wi-Fi (SSID `Tesla`, senha `123456788`).

#### Pontos Relevantes dos Periféricos e do Código

O código, em C com Pico SDK, gerencia:
- **LDR**: Conectado ao GPIO 28 (ADC2) em divisor de tensão com resistor de 10 kΩ, lê a iluminação ambiente.
- **Matriz WS2812**: Simula a cortina, com brilho proporcional à claridade.
- **LEDs RGB**: Simulam a luz, acendendo em branco.
- **MQTT**: Tópicos estruturados com QoS 1 e retenção.
- **Interrupções**: Botão de reset com `GPIO_IRQ_EDGE_FALL`.

### Uso dos Periféricos da BitDogLab

#### Protocolo Wi-Fi

Utilizado para conectar a placa ao broker MQTT (IP `10.0.0.196`) via rede Wi-Fi (SSID `Tesla`, senha `123456788`). O módulo CYW43439 da BitDogLab gerencia a comunicação com o protocolo MQTT, enviando e recebendo comandos e estados.

#### LDR (Light Dependent Resistor)

Conectado ao **GPIO 28 (ADC2)** em um divisor de tensão com um resistor de 10 kΩ:
- Um terminal do LDR está conectado ao GPIO 28 e ao resistor de 10 kΩ.
- O outro terminal do LDR está conectado a 3.3V.
- O outro terminal do resistor está conectado ao GND.  
O divisor de tensão gera uma tensão proporcional à iluminação ambiente, que é lida pelo ADC (resolução de 12 bits, 0–4095). A função `read_ldr()` converte a leitura em uma porcentagem (0–100%) para uso na automação e publicação no tópico `/casa/[comodo]/luz`.

#### Matriz de LEDs WS2812

Conectada ao **GPIO 7**, a matriz WS2812 simula a cortina:
- O brilho (0–100%) representa a "abertura" da cortina, simulando a claridade entrando no cômodo.
- Controlada via PIO com a biblioteca `matrizled.h` (função `acender_matriz_janela()`).
- Ajustada por comandos MQTT (`/casa/[comodo]/janela/set`) ou automaticamente no modo `auto`.

#### LED RGB

Conectado aos **GPIOs 11 (verde), 12 (azul), 13 (vermelho)**, o LED RGB simula a lâmpada do cômodo:
- Acende em branco (R,G,B = 1,1,1) quando a luz está ligada.
- Apaga (R,G,B = 0,0,0) quando a luz está desligada.
- Controlado via `/casa/[comodo]/luz/ligar on/off` ou automaticamente no modo `auto`.

#### Botões

O **Botão B** (GPIO 6) é usado para reset:
- Configurado com pull-up interno e interrupção `GPIO_IRQ_EDGE_FALL`.
- Aciona o reset da placa para modo BOOTSEL, reiniciando o sistema.  
O debounce é tratado via hardware (pull-up) e software (filtro de eventos na interrupção).

#### Interrupções e Tratamento de Debounce

- **Interrupções**: O botão de reset (GPIO 6) usa `gpio_set_irq_enabled_with_callback()` para detectar borda de descida (`GPIO_IRQ_EDGE_FALL`).
- **Debounce**: Pull-up interno no GPIO 6 minimiza falsos disparos, e a função `gpio_irq_handler()` processa o evento de forma estável.


### Links para Acesso

- **GitHub:** [https://github.com/Danngas/cortinas-inteligentes-iot.git](https://github.com/Danngas/cortinas-inteligentes-iot.git)
- **Vídeo de Demonstração:** [[INSIRA O LINK AQUI](https://youtu.be/MMXU9150ziU?si=j7rvrBhLgNY5swI7)] 