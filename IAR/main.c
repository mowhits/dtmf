#include "lpc214x.h"
#include "sinlut.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#define SINLUT_H
#define N 200
#define B 12
#define FS 4000
#define TH 2000000

// Goertzel setup 
const uint16_t frow[4] = {697, 770, 852, 941};
const uint16_t fcol[4] = {1209, 1336, 1477, 1633};
const unsigned char syms[16] = {'1', '4', '7', '*', '2', '5', '8', '0', '3', '6', '9', '#', 'A', 'B', 'C', 'D'};
const unsigned char symmtx[4][4] = {{0, 4, 8, 12}, {1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15}};
int32_t win[N]; // hamming window coefficients
float w, S = 1 << B;
int32_t c[8], cw[8], sw[8]; // Goertzel coefficients
int32_t I[8], Q[8], M2[8]; // Goertzel output: real, imaginary, magnitude^2i

// keypad setup
const unsigned char keycode[17] = {0xEE, 0xED, 0xEB, 0xE7, 0xDE, 0xDD, 0xDB, 0xD7, 0xBE, 0xBD, 0xBB, 0xB7, 0x7E, 0x7D, 0x7B, 0x77, 0x00};
const unsigned char scan[5] = {0xEF, 0xDF, 0xBF, 0x7F, 0x00};
unsigned char i, lcdval, row, keyscan, keyret, keynum=0, keypress, scanret=0xFF; 

// ADC setup
float adc_buf[N];
uint8_t done;

// delay functions
void delay_ms(uint16_t j) {
    uint16_t x, i;
    for (i = 0; i < j; i++) { for (x = 0; x < 6000; x++); }
}
void delay_us(uint16_t j) {
    uint16_t x, i;
    for (i = 0; i < j; i++) { for (x = 0; x < 6; x++); }
}

// LPC initialization 
void initLPC(void) {
   PINSEL0 = 0x00;
   PINSEL1 = 0x01080000;
   IO0DIR = 0xEFF0FFFF;
   AD0CR = 0x00200402;
}
// LCD initialization and helpers
void cmdLCD(char cmd) {
    IO0CLR = 0xFF00;
    IO0SET = cmd << 8;
    IO0SET = 0x40; // EN = 1
    IO0CLR = 0x30; // RS = RW = 0
    delay_ms(2);
    IO0CLR = 0x40;
    delay_ms(5);
}

void initLCD(void) {
    cmdLCD(0x38);
    cmdLCD(0x0C);
    cmdLCD(0x06);
    cmdLCD(0x01);
    cmdLCD(0x80);
}

void charLCD(char ch) {
    IO0CLR = 0xFF00;
    IO0SET = ch << 8;
    IO0SET = 0x50; // EN = RS = 1
    IO0CLR = 0x20; // RW = 0
    delay_ms(2);
    IO0CLR = 0x40; // EN = 0
    delay_ms(5);
}

void stringLCD(char* str) {
    uint8_t i = 0;
    while (str[i] != 0) {
        charLCD(str[i]);
        i++;
    }
}

// Keypad helper
int getkey(void) {
    // returns key number from 0 to 16
    row = 0;
    while(1) {
        IO0CLR = 0xFF << 16;
        row &= 0x3;
        keyscan = scan[row];
        IO0SET = keyscan << 16;
        delay_ms(2);
        keyscan = IO0PIN >> 16 & 0xF0;
        if (keyscan != keyret) break;
        row++;
    }
    for (i = 0; i < 0x10; i++) {
        if (keycode[i] == keyret)
            return i;
    }
    return 0;
}

// DAC to speaker tx to mic rx to ADC
void tx_rx(uint8_t sym) {
    uint8_t idx;
    float res;
    for (idx = 0; idx < N; idx++) {
        DACR = sin_lut[sym][idx] << 6;
        AD0CR |= (1 << 24);
        while (!(AD0GDR & (1 << 31)));
        // check the normalization here
        adc_buf[idx] = (AD0GDR >> 6) & 0x3FF;
        delay_us(250);
    }
}

// goertzel consts init
void initGoertzel(void) {
    uint16_t idx;
    // hamming window coefficients 
    for (idx = 0; idx < N; idx++) {
        win[idx] = (int)round(S*(0.54 -0.46*cosf(2.0*M_PI*(float)idx/(float)(N-1))));
    }
    // init Goertzel constants
    for (idx = 0; idx < 4; idx++) {
        w = 2.0*M_PI*round((float)N*(float)frow[idx]/(float)FS)/(float)N;
        cw[idx] = (int)round(S*cosf(w)); c[idx] = cw[idx] << 1;
        sw[idx] = (int)round(S*sinf(w));
        w = 2.0*M_PI*round((float)N*(float)fcol[idx]/(float)FS)/(float)N;
        cw[idx+4] = (int)round(S*cosf(w)); c[idx+4] = cw[idx+4] << 1;
        sw[idx+4] = (int)round(S*sinf(w));
    }
}

// goertzel for single symbol, 200 samples.
char goertzel(void) {
    uint16_t idx, n;
    int32_t x, z0, z1[8] = {0}, z2[8] = {0};
    int8_t i1 = -1, i2 = -1;

    for (n = 0; n < N; n++) {
        x = adc_buf[n];
        // hamming windowing
        x = ((x*win[n]) >> B);
        // goertzel iteration
        for (idx = 0; idx < 8; idx++) {
            z0 = x + ((c[idx] * z1[idx]) >> B) - z2[idx];
            z2[idx] = z1[idx]; z1[idx] = z0;
        }

        for (idx = 0; idx < 8; idx++) {
            I[idx] = ((cw[idx]*z1[idx]) >> B) - z2[idx];
            Q[idx] = ((sw[idx]*z1[idx]) >> B);
            M2[idx] = I[idx]*I[idx] + Q[idx]*Q[idx];
            if (M2[idx] > TH) {
                if (idx < 4) { if (i1 == -1) i1 = idx; else i1 = 4;}
                else {if (i2 == -1) i2 = idx - 4; else i2 = 4;}
            }
        }

        if ((i1 > -1) && (i1 < 4) && (i2 > -1) && (i2 < 4)) return syms[symmtx[i1][i2]];
        else return ' ';
    }

    return 0;
}

int main(void) {
    char buf[20];
    char decoded;
    uint8_t sym = 0;

    initLPC();
    initLCD();
    initGoertzel();

    while (1) {
        done = 0;
        sym = getkey();
        tx_rx(sym);
        decoded = goertzel();
        // TODO
        sprintf(buf, "Recieved message = %c", decoded);
        stringLCD(buf);
    }

    return 0;
}
