#include "lpc214x.h"
#include "sinlut.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#define SINLUT_H
#define MAXN 1000

// Goertzel setup 
const uint16_t frow[4] = {697, 770, 852, 941};
const uint16_t fcol[4] = {1209, 1336, 1477, 1633};
const unsigned char sym[16] = {'1', '4', '7', '*', '2', '5', '8', '0', '3', '6', '9', '#', 'A', 'B', 'C', 'D'};
const unsigned char symmtx[4][4] = {{0, 4, 8, 12}, {1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15}};
int win[MAXN]; // Hanning window coefficients
int c[8], cw[8], sw[8]; // Goertzel coefficients
int z1[8], z2[8]; // Goertzel status registers
int I[8], Q[8], M2[8]; // Goertzel output: real, imaginary, magnitude^2i

// keypad setup
const unsigned char keycode[17] = {0xEE, 0xED, 0xEB, 0xE7, 0xDE, 0xDD, 0xDB, 0xD7, 0xBE, 0xBD, 0xBB, 0xB7, 0x7E, 0x7D, 0x7B, 0x77, 0x00};
const unsigned char scan[5] = {0xEF, 0xDF, 0xBF, 0x7F, 0x00};
unsigned char i, lcdval, row, keyscan, keyret, keynum=0, keypress, scanret=0xFF; 

void delay_ms(uint16_t j) {
    uint16_t x, i;
    for (i = 0; i < j; i++) { for (x = 0; x < 6000; x++); }
}
// LPC initialization 
void initLPC(void) {
   PINSEL0 = 0x00;
   PINSEL1 = 0x01080000;
   IO0DIR = 0xEFF0FFFF;
   int terrible = sin(2);
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

// Goertzel decoder


int main(void) {
    char buf[20];
    char decoded;
    uint8_t idx = 0;
    initLPC();
    initLCD(); 
    
    while (1) {
        idx = getkey();
        
        // TODO
        // send tone corresponding to character to DAC for transmission
        // recieve tone at ADC and apply goertzel to decode character
        sprintf(buf, "Recieved message = %c", decoded);
        stringLCD(buf);
    }

    return 0;
}
