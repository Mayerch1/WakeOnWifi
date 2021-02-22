
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <ESP8266TimerInterrupt.h>


#include "stm.h"


#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50






HTTPClient http; 
WiFiClient client;

ESP8266Timer ITimer;

enum States stm;



void flash_yellow(){

    static int duty_cylce = 0;
    duty_cylce++;

    if(duty_cylce >= 8){
        duty_cylce = 0;
        digitalWrite(LED_YELLOW, HIGH);
    }
    else{
        digitalWrite(LED_YELLOW, LOW);
    }

}


void flash_red(){

    static int duty_cylce = 0;
    duty_cylce++;

    if(duty_cylce >= 20){
        duty_cylce = 0;
        digitalWrite(LED_RED, HIGH);
    }
    else{
        digitalWrite(LED_RED, LOW);
    }

}


void blink_yellow(){
    static char led_state = LOW;

    digitalWrite(LED_YELLOW, led_state);
    led_state = !led_state;
}

void blink_red(){
    static char led_state = LOW;

    digitalWrite(LED_RED, led_state);
    led_state = !led_state;
}

void blink_white(){
    static char led_state = LOW;

    digitalWrite(LED_WHITE, led_state);
    led_state = !led_state;
}


void blink_blue(){
    static char led_state = LOW;

    digitalWrite(LED_BLUE, led_state);
    led_state = !led_state;
}


void blink_green(){
    static char led_state = LOW;

    digitalWrite(LED_GREEN, led_state);
    led_state = !led_state;
}



int init_wifi() {
    int retries = 0;
    char led_high = LOW;

    Serial.println("Connecting to WiFi AP..........");
    ITimer.attachInterruptInterval(WIFI_BLINK * 1000, blink_blue);

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID_WiFi, PASSWD_WiFi);
    // check the status of WiFi connection to be WL_CONNECTED
    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
        retries++;
        delay(WIFI_RETRY_DELAY);

        // digitalWrite(LED_BLUE, led_high);

        // led_high = !led_high;
        Serial.print("#");
    }

    ITimer.detachInterrupt();

    return WiFi.status(); // return the WiFi connection status
}

/* setup wifi and handle led/restart on failure 
 * restart uC on WiFi failure
 */

void setup_wifi(){

    char led_state;
    int restart_delay;

    if (init_wifi() == WL_CONNECTED) {
        Serial.print("Connetted to ");
        Serial.print(SSID_WiFi);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());

        digitalWrite(LED_BLUE, HIGH);
    }
    else {
        Serial.print("Error connecting to: ");
        Serial.println(SSID_WiFi);
        Serial.println("Restarting in 10s");


        restart_delay = 0;
        led_state = LOW;

        ITimer.attachInterruptInterval(WIFI_FAIL_BLINK * 1000, blink_blue);
        delay(10000);
        
        ESP.restart();
    }
}



void kill_remote(){

  
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(KILL_BLINK * 1000, blink_red);  

    // TODO: rest request  
    delay(3000);
}


void start_remote(){

    
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(START_BLINK * 1000, blink_yellow);    

    // TODO: rest request
    delay(3000);
}


void setup() {

    int ret;
    char led_state;
    String payload;

    stm = ST_INIT;

    Serial.begin(9600);
    while(!Serial){delay(5);}

    /* LED setup, configure all leds as desired */

    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_WHITE, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);

    pinMode(SWITCH_ARM, INPUT_PULLUP);
    pinMode(SWITCH_TRIGGER, INPUT_PULLUP);


    trans_init();
    setup_wifi(); /* does not return or failure */
    stm = ST_WIFI_ESTAB;
}

void loop() {

    /* Periodically request the state of the selected remote pc.
     * If the pc is on -> set to kill.
     * If the pc is off -> set to start.
     */

    /* The state request is not performed in every iteration.
     * This way the button request can be in a tighter loop than the rest request,
     * w/o using interrupts or more timers.
     */

    int ret;

    switch(stm){
        case ST_WIFI_ESTAB:
        case ST_PING_ERR:
            if(cond_ping_test() == 0){
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }
            else{
                stm = ST_PING_ERR;
                trans_ping_err();
            }
        break;


        case ST_PING_ESTAB:
            ret = cond_pc_status();

            if(ret == 0){
                stm = ST_PC_OFF;
                trans_pc_off();
            }
            else if(ret == 1){
                stm = ST_PC_ON;
                trans_pc_on();
            }
            else if(ret == -2){
                stm = ST_PC_ERR;
                trans_pc_err();
            }
            else{
                stm = ST_PING_ERR;
                trans_ping_err();
            }
        break;


        case ST_PC_ON:
            if(cond_arm_switch() == 1){
                stm = ST_KILL_ARMED;
                trans_arm_kill();
            }
        break;


        case ST_PC_OFF:
            if(cond_arm_switch() == 1){
                stm = ST_START_ARMED;
                trans_arm_start();
            }
        break;


        case ST_KILL_ARMED:
            if(cond_arm_switch() != 1){
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }

            if(cond_trigger_pressed() == 1){
                kill_remote();
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }
        break;


        case ST_START_ARMED:
            if(cond_arm_switch() != 1){
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }

            if(cond_trigger_pressed() == 1){
                start_remote();
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }
        break;


        default:
            /* reached unsolved error state */
            delay(1000);

    }

    delay(50);
}



void trans_init(){
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_WHITE, LOW);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, LOW);
}


void trans_ping_err(){

    /* reset all 'higher' functions */
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_WHITE, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);

    /* keep led blinking */
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(PING_BLINK * 1000, blink_green);
    delay(1000); /* delay further pings */
}

void trans_ping_estab(){
    ITimer.detachInterrupt();
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_WHITE, LOW);
    Serial.println("ping established");
}



void trans_pc_on(){
    ITimer.detachInterrupt();
    digitalWrite(LED_WHITE, HIGH);
}

void trans_pc_off(){
    ITimer.detachInterrupt();
    digitalWrite(LED_WHITE, LOW);
}


void trans_pc_err(){
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(STATUS_BLINK * 1000, blink_white);
    digitalWrite(LED_RED, HIGH);
}



void trans_arm_kill(){

    ITimer.detachInterrupt();
    digitalWrite(LED_YELLOW, LOW);
    ITimer.attachInterruptInterval(ARMED_KILL * 1000, flash_red);

    Serial.println("kill armed");

    
}

void trans_arm_start(){

    ITimer.detachInterrupt();
    digitalWrite(LED_RED, LOW);
    ITimer.attachInterruptInterval(ARMED_START * 1000, flash_yellow);

    Serial.println("start armed");

}



int cond_ping_test(){
    int ret = 0;

    /* only blink with green led, no other blink allowed */
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(PING_BLINK * 1000, blink_green);


    http.begin(client, TARGET_ADDR"ping");
    ret = http.GET();
    http.end(); 

    if(ret == 200){
        ret = 0;
    }
    else{
        ret = -1;
    }

    return ret;
}



int cond_pc_status(){
    int ret;
    int led_status;

    String rest_resp;
    const char* status;
    DynamicJsonDocument doc(128);

    // String resp;
    ITimer.detachInterrupt();
    ITimer.attachInterruptInterval(STATUS_BLINK * 1000, blink_white);

    http.begin(client, TARGET_ADDR"status");
    ret = http.GET();

    if(ret == 200){
        String rest_resp = http.getString();
        deserializeJson(doc, rest_resp);
        
        if(doc["status"] == "on"){
            led_status = 1;
        }
        else if(doc["status"] == "off"){
            led_status = 0;
        }
        else{
            led_status = -2;
        }
    }
    else{
        led_status = -1;
    }

    http.end();
    return led_status;
}


int cond_arm_switch(){
    return (digitalRead(SWITCH_ARM) == 0);
}


int cond_trigger_pressed(){
    return (digitalRead(SWITCH_TRIGGER) == 0);
}


/* TODO: update pc state when alreay armed */
/* TODO: remote action */
/* TODO: error on remote action */
/* TODO: signal unarmed, but ready, state ?, maybe not*/