#include <matrizled.c>
#include "pico/stdlib.h"
#include <time.h>
// AQUI ESTA O MAPEAMENTO DE TODOS OS NUMEROS DE 0 A 9
// por padrao foi definido a exibicao dos nuemros na cor VERMELHA

#define intensidade 1

void printNum()
{
    npWrite();
    // sleep_ms(2000);
    npClear();
}



int OFF[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};



int Cor_resistor[5][5][3];

int cores_led[10][3] = {
    {0, 0, 0},      // preto
    {77, 56, 128},    // marrom
    {1, 0, 0},      // vermelho
    {71, 56, 0},    // laranja
    {97, 100, 0},   // amarelo
    {0, 1, 0},      // verde
    {0, 0, 1},      // azul
    {1, 0, 1},      // lilás (violeta)
    {100, 98, 100}, // cinza
    {1, 1, 1}       // branco
};

void Enviar_cores(int cor1, int cor2, int cor3)
{
    int cores[3][3] = {
        {cores_led[cor1][0], cores_led[cor1][1], cores_led[cor1][2]},
        {cores_led[cor2][0], cores_led[cor2][1], cores_led[cor2][2]},
        {cores_led[cor3][0], cores_led[cor3][1], cores_led[cor3][2]}
    };

    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            if (coluna % 2 == 0) {
                // coluna par: colocar cor
                int qual_cor = coluna / 2; // 0 para cor1, 1 para cor2, 2 para cor3
                Cor_resistor[linha][coluna][0] = cores[qual_cor][0]; // R
                Cor_resistor[linha][coluna][1] = cores[qual_cor][1]; // G
                Cor_resistor[linha][coluna][2] = cores[qual_cor][2]; // B
            } else {
                // coluna ímpar: preto (apagado)
                Cor_resistor[linha][coluna][0] = 0;
                Cor_resistor[linha][coluna][1] = 0;
                Cor_resistor[linha][coluna][2] = 0;
            }
        }
    }

    printNum();
    desenhaSprite(Cor_resistor, 1);
}


void DesligaMatriz()
{
    desenhaSprite(OFF, intensidade);
    printNum();
}