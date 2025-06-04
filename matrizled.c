
//#include "generated/ws2812.pio.h" // programa PIO gerado para comunicação com a matriz WS2812
#include "matrizled.h"

// função auxiliar para enviar um pixel à matriz WS2812 (controla LEDs)


static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u); // envia o valor RGB ao PIO
}

// função auxiliar para converter valores RGB em formato de 32 bits para a matriz WS2812
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b); // combina R, G e B em um único valor
}


void acender_matriz_janela(float abertura) {
    int pixel_map[5][5] = {              // mapeamento de índices da matriz WS2812
        {24, 23, 22, 21, 20},            // Linha 1: índices dos LEDs
        {15, 16, 17, 18, 19},            // Linha 2: índices dos LEDs
        {14, 13, 12, 11, 10},            // Linha 3: índices dos LEDs
        {5,  6,  7,  8,  9},             // Linha 4: índices dos LEDs
        {4,  3,  2,  1,  0}              // Linha 5: índices dos LEDs
    };
    uint32_t pixels[25] = {0}; // array para armazenar os valores RGB dos LEDs

    // Garante que a abertura esteja no intervalo de 0 a 100%
    if (abertura < 0.0f) abertura = 0.0f;
    if (abertura > 100.0f) abertura = 100.0f;

    // Calcula a intensidade (0 a 255) proporcional à abertura
    uint8_t intensidade = (uint8_t)((abertura * 255.0f) / 100.0f);

    // Acende os 4 LEDs no canto superior direito com cor branca e intensidade proporcional
    pixels[pixel_map[0][3]] = urgb_u32(intensidade, intensidade, intensidade); // LED [0][3] (índice 21)
    pixels[pixel_map[0][4]] = urgb_u32(intensidade, intensidade, intensidade); // LED [0][4] (índice 20)
    pixels[pixel_map[1][3]] = urgb_u32(intensidade, intensidade, intensidade); // LED [1][3] (índice 18)
    pixels[pixel_map[1][4]] = urgb_u32(intensidade, intensidade, intensidade); // LED [1][4] (índice 19)

    // Envia os valores RGB para a matriz de LEDs
    for (int i = 0; i < 25; i++) {
        put_pixel(pixels[i]);
    }
}