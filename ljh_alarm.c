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
#include <time.h>

#include "module.h"

// global variable
bool power = true;  //해당 프로그램의 모든 쓰레드는 무한루프를 돈다. 쓰레드 일괄 종료 위해 선언

pthread_mutex_t mtx_time;
unsigned char settingTime[5] = {'1', '3', '1', '0', '\0'};

pthread_mutex_t mtx_alarm;
bool alarm_switch = false;  //true 시 알람 동작 시작

//랜덤 지정되는 pw 배열, 정답을 키패드로 입력받는 answer 배열
unsigned char pw[4] = {9, 9, 9, 9};
unsigned char answer[4] = {0, 0, 0, 0};

//시간과 분을 unsigned char 배열로 변환해주는 함수
void convertTime(int hour, int min, unsigned char *result)
{
    unsigned char tempHourTen = (hour >> 4) & 0xF;
    unsigned char tempHour = hour & 0xF;
    unsigned char tempMinTen = (min >> 4) & 0xF;
    unsigned char tempMin = min & 0xF;
    sprintf(result, "%d%d%d%d", tempHourTen, tempHour, tempMinTen, tempMin);
}

//thread
void *th_bluetooth()
{
    // 명령어로 프로그램 조작 가능
    unsigned char c;    //한 바이트 받는 용
    while (power)
    {
        if (serialDataAvail(serial_fd)) // 읽을 데이터 존재 시
        {
            //임시 배열 생성
            unsigned char temp[5];
            temp[4] = '\0';

            //블루투스에서 불특정한 시간에 불특정한 값이 하나씩 들어옴으로 인해
            //명령어를 통한 조작으로 변경하였고
            //명령어 식별 이후 동작 기능을 나누기 위해 먼저 1바이트를 읽어왔음.
            c = serialRead();
            switch (c)  //명령어 식별문
            {
            case 'q': // 블루투스를 통한 종료 기능
                power = false;  //모든 쓰레드 일괄 종료
                return NULL;

            case 't': // ex) t 1500 -> 15시 00분으로 알람 설정
                //알람 시각을 unsigned char 배열로 전달받기
                for (int i = 0; i < 5; i++)
                {
                    c = serialRead();
                    if (i != 0) // 공백 처리
                    {
                        temp[i - 1] = c;
                        serialWrite(c); // Echo
                    }
                }
                //lock 획득 후, settingTime 배열에 복사하여 알람 시간 설정
                pthread_mutex_lock(&mtx_time);
                strcpy(settingTime, temp);
                pthread_mutex_unlock(&mtx_time);
                break;

            default:    //불특정한 값이 들어왔을 때 아무것도 안함
                break;
            }
        }
    }
    return NULL;
}

void *th_speaker()
{
    while (power)
    {
        if (alarm_switch) ring();
    }
    return NULL;
}

void *th_rtc()
{
    unsigned char mainTime[5];  //설정된 시각과 비교할 임시 문자열
    while (power)
    {
        //현재 시각 출력
        print_time(settingTime);
        //fnd 기능 이상으로 인해 콘솔에 패스워드 출력
        printf("핀 번호: ");
        for (int i = 0; i < 4; ++i)
        {
            printf("%u ", pw[i]);
        }
        printf(" 현재 입력된 번호: ");
        for (int i = 0; i < 4; ++i)
        {
            printf("%u ", answer[i]);
        }
        printf("\n");
        //현재 시각을 임시 문자열에 복사
        convertTime(hour, min, mainTime);
        if (strcmp(mainTime, settingTime) == 0 && sec == 0x00)
        { // sec 지정 이유 : 안하면 1분동안 계속 얘가 alarm_switch변수를 true로 바꿈. 
            pthread_mutex_lock(&mtx_alarm);
            alarm_switch = true;
            pthread_mutex_unlock(&mtx_alarm);
        }
        sleep(1);
    }
    return NULL;
}

void *th_servo()
{
    while (power)
    {
        if (alarm_switch)
        {
            onSwitch(); //전등 스위치 on
            sleep(70);  //한번만 동작하도록
        }
    }
    return NULL;
}

void *th_keypad()
{
    while (power)
    {
        if (alarm_switch)
        {
            // 비밀번호 생성
            srand(time(NULL));
            for (int i = 0; i < 4; i++)
            {
                pw[i] = rand() % 8 + 1;
                //키 패드의 2번 숫자가 인식이 잘 안되어서 편의를 위해 건너뜀.
                if(pw[i]==2) pw[i]++;   
            }

            // 비밀번호를 입력받는 loop
            while (1)
            {
                bool correct=true;
                //비밀번호 입력받기
                for (int i = 0; i < 4; i++)
                {
                    answer[i] = getSingleKey();
                    delay(500);
                }
                //생성된 비밀번호와 비교
                for (int i = 0; i < 4; ++i)
                {
                    if (pw[i] != answer[i]) correct=false;
                }
                if(correct) break;
            }

            pthread_mutex_lock(&mtx_alarm);
            alarm_switch = false;
            pthread_mutex_unlock(&mtx_alarm);
        }
    }
    return NULL;
}

void main() //메인함수에서는 셋업, 쓰레드의 생성 및 종료만 조작
{
    // variable space
    pthread_t bluetooth;
    pthread_t speaker;
    pthread_t RTC;
    pthread_t servo;
    pthread_t keypad;

    // set up
    total_setup();
    pthread_mutex_init(&mtx_time, NULL);
    pthread_mutex_init(&mtx_alarm, NULL);
    printf("setting complete...\n");
    delay(1000);

    // create thread
    pthread_create(&bluetooth, NULL, th_bluetooth, NULL);
    pthread_create(&speaker, NULL, th_speaker, NULL);
    pthread_create(&RTC, NULL, th_rtc, NULL);
    pthread_create(&servo, NULL, th_servo, NULL);
    pthread_create(&keypad, NULL, th_keypad, NULL);

    // exit proccess
    pthread_join(bluetooth, NULL);
    pthread_join(speaker, NULL);
    pthread_join(RTC, NULL);
    pthread_join(servo, NULL);
    pthread_join(keypad, NULL);
    pthread_mutex_destroy(&mtx_time);
    pthread_mutex_destroy(&mtx_alarm);
    return;
}