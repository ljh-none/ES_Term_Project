#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <softTone.h>
#include <wiringPiI2C.h>
#include <wiringSerial.h>
#include <wiringShift.h>

#include "module.h"

// servo
#define SERVO 13
#define RANGE 2000 // 서보모터는 50hz에서 동작. 이에 따른 범위와 클럭 지정
#define CLOCK 192
#define MAX 246 // duty값 범위.
#define MEAN 150
#define MIN 54
#define SERVO_ON 200
#define SERVO_OFF 100

// bluetooth
#define BAUD_RATE 115200
int serial_fd;
static const char *UART2_DEV = "/dev/ttyAMA1"; // UART2 장치 파일

// RTC
int i2c_fd;
int sec, min, hour, day, date, month, year;
#define SLAVE_ADDR_01 0x68                 // 슬레이브 주소
static const char *I2C_DEV = "/dev/i2c-1"; // I2C 연결을 위한 장치 파일

// speaker
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

//keypad
#define SCL 21
#define SDA 20

//fnd
#define SDI 5
#define RCLK 4
#define SRCLK 1
const int placePin[] = {12, 3, 2, 0};
unsigned char number[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};

// RTC
void _init_RTC()
{
    i2c_fd = wiringPiI2CSetupInterface(I2C_DEV, SLAVE_ADDR_01);

    // 시간 형식 설정
    wiringPiI2CWriteReg8(i2c_fd, 0x01, 0x06);        // 분
    wiringPiI2CWriteReg8(i2c_fd, 0x02, 0x40 | 0x13); // 0x40 -> 24시간 체계
    wiringPiI2CWriteReg8(i2c_fd, 0x03, 0x04);        // 요일
    wiringPiI2CWriteReg8(i2c_fd, 0x04, 0x26);        // 날짜
    wiringPiI2CWriteReg8(i2c_fd, 0x05, 0x10);        // 월
    wiringPiI2CWriteReg8(i2c_fd, 0x06, 0x23);        // 년

    //현재 시간을 RTC모듈에 입력
    time_t rawtime;
    struct tm *timeInfo;
    time(&rawtime);
    timeInfo = localtime(&rawtime);

    wiringPiI2CWriteReg8(i2c_fd, 0x00, ((timeInfo->tm_sec / 10) << 4) + (timeInfo->tm_sec % 10));
    wiringPiI2CWriteReg8(i2c_fd, 0x01, ((timeInfo->tm_min / 10) << 4) + (timeInfo->tm_min % 10));
    wiringPiI2CWriteReg8(i2c_fd, 0x02, ((timeInfo->tm_hour / 10) << 4) + (timeInfo->tm_hour % 10));
    wiringPiI2CWriteReg8(i2c_fd, 0x03, timeInfo->tm_wday);
    wiringPiI2CWriteReg8(i2c_fd, 0x04, timeInfo->tm_mday);
    wiringPiI2CWriteReg8(i2c_fd, 0x05, (((timeInfo->tm_mon + 1) / 10) << 4) + ((timeInfo->tm_mon + 1) % 10));

    printf("RTC set\n");
}

void print_time(unsigned char *settingTime)
{
    // 24시간 체계, 매초 업뎃, 현재시각으로 변경.
    sec = wiringPiI2CReadReg8(i2c_fd, 0x00);  //%x 출력 그대로
    min = wiringPiI2CReadReg8(i2c_fd, 0x01);  //%x 출력 그대로
    hour = wiringPiI2CReadReg8(i2c_fd, 0x02);
    day = wiringPiI2CReadReg8(i2c_fd, 0x03);   // 16진수에 따라 요일 선택
    date = wiringPiI2CReadReg8(i2c_fd, 0x04);  //%x 출력 그대로
    month = wiringPiI2CReadReg8(i2c_fd, 0x05); //%x 출력 그대로
    year = wiringPiI2CReadReg8(i2c_fd, 0x06);  //%x 출력 그대로
    
    //출력
    system("clear");
    switch (day)
    {
    case 0x01:
        printf("20%x년 %x월 %x일 월요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x02:
        printf("20%x년 %x월 %x일 화요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x03:
        printf("20%x년 %x월 %x일 수요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x04:
        printf("20%x년 %x월 %x일 목요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x05:
        printf("20%x년 %x월 %x일 금요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x06:
        printf("\r20%x년 %x월 %x일 토요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    case 0x07:
        printf("20%x년 %x월 %x일 일요일 %x시 %x분 %x초, 알람 시각 : %s\n",
               year, month, date, hour, min, sec, settingTime);
        break;
    }
}

// servo
void _init_servo()
{
    pinMode(SERVO, PWM_OUTPUT);
    pwmSetMode(PWM_MODE_MS);
    pwmSetRange(RANGE);
    pwmSetClock(CLOCK);
    pwmWrite(SERVO, MEAN);
    delay(100);
    printf("servo set\n");
}

void onSwitch()
{
    pwmWrite(SERVO, SERVO_ON);
    return;
}

void offSwitch()
{
    pwmWrite(SERVO, SERVO_OFF);
    return;
}

// speaker
void _init_speaker()
{
    softToneCreate(TONE);
    printf("speaker set\n");
}

void ring()
{   //음악 함수
    softToneWrite(TONE, EE);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, DD);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, EE);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, DD);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, EE);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, B);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, DD);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, CC);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, A);
    usleep(300000); // 300 밀리초
    softToneWrite(TONE, 0);
    return;
}

// bluetooth
void _init_bluetooth()
{
    if ((serial_fd = serialOpen(UART2_DEV, BAUD_RATE)) < 0)
    {
        printf("Unable to open serial device.\n");
        return;
    }
    printf("bluetooth set\n");
}

unsigned char serialRead()
{
    unsigned char x;
    if (read(serial_fd, &x, 1) != 1)
        return -1;
    return x;
}

void serialWrite(const unsigned char c)
{
    write(serial_fd, &c, 1);
}

//keypad
bool _GetBit()
{
	digitalWrite(SCL, LOW);
	delayMicroseconds(2); // 500KHz
	bool retVal = !digitalRead(SDA);
	digitalWrite(SCL, HIGH);
	delayMicroseconds(2); // 500KHz
	return retVal;
}

void _init_keypad(){
    if (wiringPiSetupGpio() == -1)
        return;
    pinMode(SCL, OUTPUT);
    pinMode(SDA, INPUT);
    digitalWrite(SCL, HIGH);
    printf("keypad set\n");
}

unsigned char getSingleKey(){
    unsigned char result = 0;   //입력한 숫자 저장
    while (1)
    {
        //입력이 들어올 때 까지 대기
        while (digitalRead(SDA)); // DV LOW
	    while (!digitalRead(SDA)); // DV HIGH
	    delayMicroseconds(10);
        
        //몇 번 숫자가 눌렸는지 체크
        result = 0;
	    for (unsigned char i = 0; i < 8; i++){
            //8번의 클럭을 따라 값을 계속 받는 _GetBit함수
            //만약 3번째 클럭에서 값이 받아졌다면, 3이 클릭되는 것
            if (_GetBit()) {
                result = i + 1;
                }
        }
	    delay(2); // Tout
        break;
    }
    return result;
}

//fnd
void _init_FND()
{
    pinMode(SDI, OUTPUT);
    pinMode(RCLK, OUTPUT);
    pinMode(SRCLK, OUTPUT);

    for (int i = 0; i < 4; i++)
    {   //place pin = digit의 자리
        pinMode(placePin[i], OUTPUT);
        digitalWrite(placePin[i], HIGH);
    }
    printf("FND set\n");
}

void pickDigit(int digit)
{
    for (int i = 0; i < 4; i++)
    {
        digitalWrite(placePin[i], 1);
    }
    digitalWrite(placePin[digit], 0);
}

void hc595_shift(int8_t data)
{
    int i;
    for (i = 0; i < 8; i++)
    {
        digitalWrite(SDI, 0x80 & (data << i));
        digitalWrite(SRCLK, 1);
        delayMicroseconds(1);
        digitalWrite(SRCLK, 0);
    }
    digitalWrite(RCLK, 1);
    delayMicroseconds(1);
    digitalWrite(RCLK, 0);
}

void clearDisplay()
{
    int i;
    for (i = 0; i < 8; i++)
    {
        digitalWrite(SDI, 0);
        digitalWrite(SRCLK, 1);
        delayMicroseconds(1);
        digitalWrite(SRCLK, 0);
    }
    digitalWrite(RCLK, 1);
    delayMicroseconds(1);
    digitalWrite(RCLK, 0);
}

// module setup
void total_setup()
{
    if (wiringPiSetupGpio() == -1)
        return;
    _init_servo();
    _init_speaker();
    _init_RTC();
    _init_bluetooth();
    _init_keypad();
    //_init_FND();
}
