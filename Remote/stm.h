#ifndef _STM_H_
#define _STM_H_


#include "secret.h"
#include "pins.h"


#define LED_YELLOW D6
#define LED_RED D8
#define LED_WHITE D7
#define LED_BLUE D5
#define LED_GREEN D0

#define SWITCH_ARM D1
#define SWITCH_TRIGGER D2


#define HW_TIM_INTERVAL_MS 50

#define WIFI_BLINK 350
#define WIFI_FAIL_BLINK 150
#define PING_BLINK 250
#define PING_FAIL_BLINK 200

#define STATUS_BLINK 200
#define ARMED_KILL 50
#define ARMED_START 150
#define KILL_BLINK 50
#define START_BLINK 200


enum States {ST_INIT, ST_WIFI_ESTAB, ST_PING_ERR, ST_PING_ESTAB, \
            ST_PC_ON, ST_PC_OFF, ST_PC_ERR, \
            ST_KILL_ARMED, ST_START_ARMED, \
            ST_KILL_ERR, ST_START_ERR};


#endif