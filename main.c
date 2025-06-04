
/* Sistema de Cortinas Inteligentes com Controle de Luz - Automação Residencial
 * Autor: Daniel Silva de Souza
 * Polo: Bom Jesus da Lapa
 * Data: 04/06/2025
 *
 * Código para projeto IoT utilizando protocolo MQTT, controlando iluminação ambiente
 * em 4 cômodos (Sala, 2 Quartos, Cozinha) com LDR, servo motor para janelas, e relé
 * para luzes. Mantém iluminação-alvo (padrão 65%) ajustando janelas e luzes, com modos
 * automático, manual e dormir via interface IoT. Usa placa BitDogLab.
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"
#include "matrizled.h"
#include <math.h>

#define WIFI_SSID "Tesla"
#define WIFI_PASSWORD "123456788"
#define MQTT_SERVER "10.0.0.196"
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "admin"
#define MQTT_DEVICE_NAME "pico_cortinas"
#define MQTT_UNIQUE_TOPIC 0
#define ADC_PIN 28
#define R_CONHECIDO 10000
#define ADC_VREF 3.3f
#define ADC_RESOLUTION 4095
#define botaoB 6
#define SERVO_PIN 15          // Servo para janela
#define LIGHT_PIN 14          // Relé/LED para luz
#define ILUMINACAO_ALVO 65.0f // Iluminação padrão (65%)
#define JANELA_STEP 5         // Incremento de abertura da janela (5%)

#define WS2812_PIN 7 // GPIO para matriz de LEDs WS2812
#define LED_BLUE_PIN 12  // GPIO12 - LED azul
#define LED_GREEN_PIN 11 // GPIO11 - LED verde
#define LED_RED_PIN 13   // GPIO13 - LED vermelho
#define BUZZER_PIN 10    // Pino do buzzer

#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C'
#endif

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

float sala_janela = 0;

typedef struct
{
    mqtt_client_t *mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

// Estado de um cômodo
typedef struct
{
    const char *nome;      // Ex.: "sala"
    float iluminacao_alvo; // Ex.: 65% (ajustável pelo usuário)
    float janela_pos;      // 0-100% (abertura)
    bool luz_ligada;       // Luz on/off
    bool modo_auto;        // Automático ou manual
    bool modo_dormir;      // Modo dormir ativo
} Comodo;

static Comodo comodo_atual = {.nome = "sala", .iluminacao_alvo = ILUMINACAO_ALVO, .janela_pos = 0.0f, .luz_ligada = false, .modo_auto = true, .modo_dormir = false};

#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

#define TEMP_WORKER_TIME_S 10
#define MQTT_KEEP_ALIVE_S 60
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

static float read_onboard_temperature(const char unit);
static void pub_request_cb(__unused void *arg, err_t err);
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);
static void control_led(MQTT_CLIENT_DATA_T *state, bool on);
static void publish_temperature(MQTT_CLIENT_DATA_T *state);
static void sub_request_cb(void *arg, err_t err);
static void unsub_request_cb(void *arg, err_t err);
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t temperature_worker = {.do_work = temperature_worker_fn};
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void start_client(MQTT_CLIENT_DATA_T *state);
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);
static float read_ldr();
static void publish_light(MQTT_CLIENT_DATA_T *state);
static void gpio_irq_handler(uint gpio, uint32_t events);
static void init_servo(void);
static void set_janela(float pos);
static void set_luz(bool on);
static void automacao_iluminacao(MQTT_CLIENT_DATA_T *state);
static void publish_all_states(MQTT_CLIENT_DATA_T *state);
static void publish_estado(MQTT_CLIENT_DATA_T *state);
static void publish_horario(MQTT_CLIENT_DATA_T *state);
static void publish_janela_estado(MQTT_CLIENT_DATA_T *state);
static void publish_janela_pos(MQTT_CLIENT_DATA_T *state);
static void publish_luz_estado(MQTT_CLIENT_DATA_T *state);

int main(void)
{
    stdio_init_all();
    INFO_printf("mqtt client starting\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(ADC_PIN);

    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    init_servo();
    gpio_init(LIGHT_PIN);
    gpio_set_dir(LIGHT_PIN, GPIO_OUT);
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    set_luz(false);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, false);

    static MQTT_CLIENT_DATA_T state;

    if (cyw43_arch_init())
    {
        panic("Failed to initialize CYW43");
    }

    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for (int i = 0; i < sizeof(unique_id_buf) - 1; i++)
    {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
                                                                                client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    if (err == ERR_OK)
    {
        start_client(&state);
    }
    else if (err != ERR_INPROGRESS)
    {
        panic("dns request failed");
    }

    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst))
    {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
        acender_matriz_janela(sala_janela);
    }

    INFO_printf("mqtt client exiting\n");
    return 0;
}

static float read_onboard_temperature(const char unit)
{
    adc_select_input(4);
    const float conversionFactor = 3.3f / (1 << 12);
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C' || unit != 'F')
    {
        return tempC;
    }
    else if (unit == 'F')
    {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

static void pub_request_cb(__unused void *arg, err_t err)
{
    if (err != 0)
    {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name)
{
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

static void control_led(MQTT_CLIENT_DATA_T *state, bool on)
{
    const char *message = on ? "On" : "Off";
    if (on)
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    else
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/led/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

static void publish_temperature(MQTT_CLIENT_DATA_T *state)
{
    static float old_temperature;
    const char *temperature_key = full_topic(state, "/temperature");
    float temperature = read_onboard_temperature(TEMPERATURE_UNITS);
    if (temperature != old_temperature)
    {
        old_temperature = temperature;
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
        INFO_printf("Publishing %s to %s\n", temp_str, temperature_key);
        mqtt_publish(state->mqtt_client_inst, temperature_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

static void sub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

static void unsub_request_cb(void *arg, err_t err)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (err != 0)
    {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;

    if (state->subscribe_count <= 0 && state->stop_client)
    {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub)
{
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/led"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/select"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/luz/set"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/janela/set"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/janela/abrir"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/luz/ligar"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/modo"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/casa/sala/modo_dormir"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    DEBUG_printf("After processing %s: %s, %s\n", state->topic, state->data, basic_topic);

    if (strcmp(basic_topic, "/led") == 0)
    {
        if (lwip_stricmp((const char *)state->data, "on") == 0 || strcmp((const char *)state->data, "1") == 0)
        {
            INFO_printf("Received /led: %s\n", state->data);
            control_led(state, true);
        }
        else if (lwip_stricmp((const char *)state->data, "off") == 0 || strcmp((const char *)state->data, "0") == 0)
        {
            INFO_printf("Received /led: %s\n", state->data);
            control_led(state, false);
        }
    }
    else if (strcmp(basic_topic, "/print") == 0)
    {
        INFO_printf("Received /print: %.*s\n", len, state->data);
        INFO_printf("%.*s\n", len, state->data);
    }
    else if (strcmp(basic_topic, "/ping") == 0)
    {
        INFO_printf("Received /ping\n");
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buffer, strlen(buffer), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
    else if (strcmp(basic_topic, "/exit") == 0)
    {
        INFO_printf("Received /exit\n");
        state->stop_client = true;
        sub_unsub_topics(state, false);
    }
    else if (strcmp(basic_topic, "/casa/sala/select") == 0)
    {
        if (!comodo_atual.modo_dormir && strcmp((const char *)state->data, "on") == 0)
        {
            INFO_printf("Received /casa/sala/select: %s\n", state->data);
            comodo_atual.nome = "sala";
            publish_all_states(state);
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/luz/set") == 0)
    {
        if (!comodo_atual.modo_dormir && !comodo_atual.modo_auto)
        {
            float nova_alvo = atof((const char *)state->data);
            if (nova_alvo >= 0.0f && nova_alvo <= 100.0f)
            {
                INFO_printf("Received /casa/sala/luz/set: %.2f\n", nova_alvo);
                comodo_atual.iluminacao_alvo = nova_alvo;
                publish_estado(state);
            }
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/janela/set") == 0)
    {
        if (!comodo_atual.modo_dormir && !comodo_atual.modo_auto)
        {
            float nova_pos = atof((const char *)state->data);
            if (nova_pos >= 0.0f && nova_pos <= 100.0f)
            {
                INFO_printf("Received /casa/sala/janela/set: %.2f\n", nova_pos);
                comodo_atual.janela_pos = nova_pos;
                set_janela(nova_pos);
                publish_all_states(state);
                sala_janela = nova_pos;
            }
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/janela/abrir") == 0)
    {
        if (!comodo_atual.modo_dormir && !comodo_atual.modo_auto)
        {
            if (lwip_stricmp((const char *)state->data, "on") == 0)
            {
                INFO_printf("Received /casa/sala/janela/abrir: on\n");
                comodo_atual.janela_pos = 100.0f;
                set_janela(100.0f);
                sala_janela = 100.0f;
            }
            else if (lwip_stricmp((const char *)state->data, "off") == 0)
            {
                INFO_printf("Received /casa/sala/janela/abrir: off\n");
                comodo_atual.janela_pos = 0.0f;
                set_janela(0.0f);
                sala_janela = 0.0f;
            }
            publish_all_states(state);
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/luz/ligar") == 0)
    {
        if (!comodo_atual.modo_dormir && !comodo_atual.modo_auto)
        {
            if (lwip_stricmp((const char *)state->data, "on") == 0)
            {
                INFO_printf("Received /casa/sala/luz/ligar: on\n");
                set_luz(true);
                comodo_atual.luz_ligada = true;
            }
            else if (lwip_stricmp((const char *)state->data, "off") == 0)
            {
                INFO_printf("Received /casa/sala/luz/ligar: off\n");
                set_luz(false);
                comodo_atual.luz_ligada = false;
            }
            publish_all_states(state);
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/modo") == 0)
    {
        if (!comodo_atual.modo_dormir)
        {
            if (lwip_stricmp((const char *)state->data, "auto") == 0)
            {
                INFO_printf("Received /casa/sala/modo: auto\n");
                comodo_atual.modo_auto = true;
            }
            else if (lwip_stricmp((const char *)state->data, "manual") == 0)
            {
                INFO_printf("Received /casa/sala/modo: manual\n");
                comodo_atual.modo_auto = false;
            }
            publish_all_states(state);
        }
    }
    else if (strcmp(basic_topic, "/casa/sala/modo_dormir") == 0)
    {
        if (strcmp((const char *)state->data, "on") == 0)
        {
            INFO_printf("Received /casa/sala/modo_dormir: on\n");
            comodo_atual.modo_dormir = true;
            set_luz(false);
            comodo_atual.luz_ligada = false;
            publish_luz_estado(state);
            set_janela(0.0f);
            comodo_atual.janela_pos = 0.0f;
            comodo_atual.modo_auto = false;
            sala_janela = 0.0f;
            publish_all_states(state);
        }
        else if (strcmp((const char *)state->data, "off") == 0)
        {
            INFO_printf("Received /casa/sala/modo_dormir: off\n");
            comodo_atual.modo_dormir = false;
            comodo_atual.modo_auto = true;
            publish_all_states(state);
        }
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)worker->user_data;
    publish_temperature(state);
    publish_light(state);
    publish_horario(state);
    if (comodo_atual.modo_auto && !comodo_atual.modo_dormir)
    {
        automacao_iluminacao(state);
    }
    async_context_add_at_time_worker_in_ms(context, worker, TEMP_WORKER_TIME_S * 1000);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        state->connect_done = true;
        sub_unsub_topics(state, true);

        if (state->mqtt_client_info.will_topic)
        {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        publish_all_states(state); // Publicar estados iniciais
        temperature_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &temperature_worker, 0);
    }
    else if (status == MQTT_CONNECT_DISCONNECTED)
    {
        if (!state->connect_done)
        {
            panic("Failed to connect to mqtt server");
        }
    }
    else
    {
        panic("Unexpected status");
    }
}

static void start_client(MQTT_CLIENT_DATA_T *state)
{
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst)
    {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK)
    {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T *)arg;
    if (ipaddr)
    {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    }
    else
    {
        panic("dns request failed");
    }
}

static float read_ldr()
{
    adc_select_input(2);
    float soma = 0.0f;
    for (int i = 0; i < 100; i++)
    {
        soma += adc_read();
        sleep_ms(1);
    }
    float media = soma / 100.0f;

    float adc_min = 100.0f;
    float adc_max = 4000.0f;
    float light = 100.0f * (adc_max - media) / (adc_max - adc_min);
    light = light < 0.0f ? 0.0f : light > 100.0f ? 100.0f : light;

    return light;
}

static void publish_light(MQTT_CLIENT_DATA_T *state)
{
    static float old_light = -1.0f;
    char light_key[MQTT_TOPIC_LEN];
    snprintf(light_key, sizeof(light_key), "/casa/%s/luz", comodo_atual.nome);
    float light = read_ldr();
    if (fabs(light - old_light) > 0.5f)
    {
        old_light = light;
        char light_str[16];
        snprintf(light_str, sizeof(light_str), "%.2f", light);
        INFO_printf("Publishing %s to %s\n", light_str, light_key);
        mqtt_publish(state->mqtt_client_inst, light_key, light_str, strlen(light_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

static void gpio_irq_handler(uint gpio, uint32_t events)
{
    if (gpio == botaoB && events & GPIO_IRQ_EDGE_FALL)
    {
        reset_usb_boot(0, 0);
    }
}

static void init_servo(void)
{
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f); // 1MHz
    pwm_config_set_wrap(&config, 20000);    // 20ms
    pwm_init(slice_num, &config, true);
    set_janela(0.0f); // Inicia fechada
}

static void set_janela(float pos)
{
    pos = pos < 0.0f ? 0.0f : pos > 100.0f ? 100.0f : pos;
    uint16_t pulse = 500 + (pos * 2000 / 100); // 500us (0°) a 2500us (180°)
    pwm_set_gpio_level(SERVO_PIN, pulse);
    comodo_atual.janela_pos = pos;
}

static void set_luz(bool on)
{
    gpio_put(LIGHT_PIN, on ? 1 : 0);
    if (on)
    {
        gpio_put(LED_RED_PIN, 1);
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_BLUE_PIN, 1);
    }
    else
    {
        gpio_put(LED_RED_PIN, 0);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 0);
    }
    comodo_atual.luz_ligada = on;
}

static void automacao_iluminacao(MQTT_CLIENT_DATA_T *state)
{
    float luz_atual = read_ldr();
    float alvo = comodo_atual.iluminacao_alvo;
    float tolerancia = 2.0f; // ±2% de tolerância

    if (luz_atual < (alvo - tolerancia) && comodo_atual.janela_pos < 100.0f)
    {
        comodo_atual.janela_pos += JANELA_STEP;
        if (comodo_atual.janela_pos > 100.0f) comodo_atual.janela_pos = 100.0f;
        set_janela(comodo_atual.janela_pos);
        sala_janela = comodo_atual.janela_pos;
        publish_all_states(state);
    }
    else if (luz_atual < (alvo - tolerancia) && comodo_atual.janela_pos >= 100.0f && !comodo_atual.luz_ligada)
    {
        set_luz(true);
        publish_all_states(state);
    }
    else if (luz_atual > (alvo + tolerancia) && comodo_atual.janela_pos > 0.0f)
    {
        comodo_atual.janela_pos -= JANELA_STEP;
        if (comodo_atual.janela_pos < 0.0f) comodo_atual.janela_pos = 0.0f;
        set_janela(comodo_atual.janela_pos);
        sala_janela = comodo_atual.janela_pos;
        if (luz_atual > (alvo + tolerancia) && comodo_atual.luz_ligada && comodo_atual.janela_pos == 0.0f)
        {
            set_luz(false);
        }
        publish_all_states(state);
    }
}

static void publish_all_states(MQTT_CLIENT_DATA_T *state)
{
    publish_estado(state);
    publish_janela_estado(state);
    publish_janela_pos(state);
    publish_luz_estado(state);
}

static void publish_estado(MQTT_CLIENT_DATA_T *state)
{
    char estado_key[MQTT_TOPIC_LEN];
    snprintf(estado_key, sizeof(estado_key), "/casa/%s/estado", comodo_atual.nome);
    char estado_str[128];
    snprintf(estado_str, sizeof(estado_str),
             "{\"luz\":%.2f,\"janela\":%.2f,\"luz_ligada\":%d,\"modo\":\"%s\",\"modo_dormir\":%d,\"iluminacao_alvo\":%.2f}",
             read_ldr(), comodo_atual.janela_pos, comodo_atual.luz_ligada, comodo_atual.modo_auto ? "auto" : "manual", comodo_atual.modo_dormir, comodo_atual.iluminacao_alvo);
    mqtt_publish(state->mqtt_client_inst, estado_key, estado_str, strlen(estado_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

static void publish_janela_estado(MQTT_CLIENT_DATA_T *state)
{
    char janela_estado_key[MQTT_TOPIC_LEN];
    snprintf(janela_estado_key, sizeof(janela_estado_key), "/casa/%s/janela/estado", comodo_atual.nome);
    const char *estado = (comodo_atual.janela_pos > 0.0f) ? "on" : "off";
    INFO_printf("Publishing to %s: %s\n", janela_estado_key, estado);
    mqtt_publish(state->mqtt_client_inst, janela_estado_key, estado, strlen(estado), MQTT_PUBLISH_QOS, 1, pub_request_cb, state);
}

static void publish_janela_pos(MQTT_CLIENT_DATA_T *state)
{
    char janela_pos_key[MQTT_TOPIC_LEN];
    snprintf(janela_pos_key, sizeof(janela_pos_key), "/casa/%s/janela/pos", comodo_atual.nome);
    char pos_str[16];
    snprintf(pos_str, sizeof(pos_str), "%.2f", comodo_atual.janela_pos);
    INFO_printf("Publishing to %s: %s\n", janela_pos_key, pos_str);
    mqtt_publish(state->mqtt_client_inst, janela_pos_key, pos_str, strlen(pos_str), MQTT_PUBLISH_QOS, 1, pub_request_cb, state);
}

static void publish_luz_estado(MQTT_CLIENT_DATA_T *state)
{
    char luz_estado_key[MQTT_TOPIC_LEN];
    snprintf(luz_estado_key, sizeof(luz_estado_key), "/casa/%s/luz/estado", comodo_atual.nome);
    const char *estado = comodo_atual.luz_ligada ? "on" : "off";
    INFO_printf("Publishing to %s: %s\n", luz_estado_key, estado);
    mqtt_publish(state->mqtt_client_inst, luz_estado_key, estado, strlen(estado), MQTT_PUBLISH_QOS, 1, pub_request_cb, state);
}

static void publish_horario(MQTT_CLIENT_DATA_T *state)
{
    absolute_time_t now = get_absolute_time();
    uint64_t us = to_us_since_boot(now);
    uint32_t seconds = us / 1000000;
    uint32_t hours = (seconds / 3600) % 24;
    uint32_t minutes = (seconds / 60) % 60;
    char horario_str[16];
    snprintf(horario_str, sizeof(horario_str), "%02u:%02u", hours, minutes);
    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/casa/horario"), horario_str, strlen(horario_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}
