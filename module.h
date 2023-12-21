#ifndef MODULE_H
#define MODULE_H

#include <wiringShift.h>

#define TONE 16 // 스피커 GPIO
#define C 261
#define D 293
#define E 330
#define F 349
#define G 392
#define A 440
#define B 493
#define CC 523
#define CCC 554
#define DD 587
#define DDD 622
#define EE 659

extern int serial_fd;
extern int i2c_fd;
extern int sec, min, hour, day, date, month, year;

void total_setup();

//RTC
void print_time(unsigned char *settingTime);

//speaker
void ring();

//servo
void onSwitch();
void offSwitch();

//bluetooth
unsigned char serialRead();
void serialWrite(const unsigned char c);

//keypad
unsigned char getSingleKey();

//fnd
void clearDisplay();
void hc595_shift(int8_t data);
void pickDigit(int digit);

#endif