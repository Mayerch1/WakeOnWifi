
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include <ESP8266TimerInterrupt.h>
#include <ESP8266_ISR_Timer.h>


#include "stm.h"


#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50



HTTPClient http; 
WiFiClient client;

ESP8266Timer ITimer;
ESP8266_ISR_Timer ISR_Timer;

enum States stm;


unsigned int timer_yellow = ESP8266_ISR_Timer::MAX_TIMERS;
unsigned int timer_red = ESP8266_ISR_Timer::MAX_TIMERS;
unsigned int timer_white = ESP8266_ISR_Timer::MAX_TIMERS;
unsigned int timer_blue = ESP8266_ISR_Timer::MAX_TIMERS;
unsigned int timer_green = ESP8266_ISR_Timer::MAX_TIMERS;



void ICACHE_RAM_ATTR timer_handler(){
    ISR_Timer.run();
}


void ICACHE_RAM_ATTR flash_yellow(){
    const char duty_limit = 8;
    static int duty_cycle = 0;

    flash_led(LED_YELLOW, &duty_cycle, duty_limit);
}


void ICACHE_RAM_ATTR flash_red(){

    const char duty_limit = 20;
    static int duty_cycle = 0;

    flash_led(LED_RED, &duty_cycle, duty_limit);
}


void ICACHE_RAM_ATTR blink_yellow(){
    static char led_state = LOW;

    blink_led(LED_YELLOW, &led_state);
}

void ICACHE_RAM_ATTR blink_red(){
    static char led_state = LOW;
    
    blink_led(LED_RED, &led_state);
}

void ICACHE_RAM_ATTR blink_white(){
    static char led_state = LOW;

    blink_led(LED_WHITE, &led_state);
}


void ICACHE_RAM_ATTR blink_blue(){
    static char led_state = LOW;

    blink_led(LED_BLUE, &led_state);
}


void ICACHE_RAM_ATTR blink_green(){
    static char led_state = LOW;

    blink_led(LED_GREEN, &led_state);
}



void flash_led(int pin, int *const duty_cycle, const char duty_limit){

    if(++(*duty_cycle) >= duty_limit){
        *duty_cycle = 0;
        digitalWrite(pin, HIGH);
    }
    else{
        digitalWrite(pin, LOW);
    }
}

void blink_led(int pin, char *const state){

    digitalWrite(pin, *state);
    *state = !(*state);
}



int init_wifi() {
    int retries = 0;
    char led_high = LOW;
    int timer;

    Serial.println("Connecting to WiFi AP..........");
    timer_blue = ISR_Timer.setInterval(WIFI_BLINK, blink_blue);


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

    ISR_Timer.deleteTimer(timer);
    timer_blue = ESP8266_ISR_Timer::MAX_TIMERS;

    return WiFi.status(); // return the WiFi connection status
}

/* setup wifi and handle led/restart on failure 
 * restart uC on WiFi failure
 */

void setup_wifi(){


    if (init_wifi() == WL_CONNECTED) {
        Serial.print("Connetted to ");
        Serial.print(SSID_WiFi);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());

        digitalWrite(LED_BLUE, HIGH);
    }
    else {
        Serial.print(" Error connecting to: ");
        Serial.println(SSID_WiFi);
        Serial.println("Restarting in 10s");

        ISR_Timer.setInterval(WIFI_FAIL_BLINK, blink_blue);
        delay(10000);
        
        ESP.restart();
    }
}



int kill_remote(){

    int ret;

    ISR_Timer.deleteTimer(timer_red);
    timer_red = ISR_Timer.setInterval(START_BLINK, blink_red);


    http.begin(client, TARGET_ADDR"kill");
    http.addHeader("secret", HTTP_SECRET);
    ret = http.GET();

    http.end();

    if(ret == 200){
        /* give pc time to react to request */
        delay(5000);
        return 0;
    }
    else if(ret == 412){
        return 1;
    }
    else{
        // TODO: replace with t1
        return ret;
    }    
}


int start_remote(){

    int ret;
     
    ISR_Timer.deleteTimer(timer_yellow);
    timer_yellow = ISR_Timer.setInterval(START_BLINK, blink_yellow);


    http.begin(client, TARGET_ADDR"start");
    http.addHeader("secret", HTTP_SECRET);
    ret = http.GET();

    http.end();

    if(ret == 200){
        /* give pc time to react to request */
        delay(1500);
        return 0;
    }
    else if(ret == 412){
        return 1;
    }
    else{
        return -1;
    } 
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


    ITimer.attachInterruptInterval(HW_TIM_INTERVAL_MS * 1000, timer_handler);


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
    static int timer_ms = 0;

    /* the machine gets forced to re-fetch every 10 seconds */
    if(stm != ST_WIFI_ESTAB && stm != ST_PING_ERR &&\
       stm != ST_KILL_ERR && stm != ST_START_ERR &&\
        timer_ms > 10000){

        timer_ms = 0;
        stm = ST_PING_ESTAB;            
    }


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

                ret = kill_remote();
                if(ret == 0 || ret == 1){
                    stm = ST_PING_ESTAB;
                    trans_ping_estab();
                }
                else{
                    Serial.println("kill failed with %d\n", ret);
                    //TODO: kill returned -11???
                    stm = ST_KILL_ERR;
                    trans_kill_err();
                }
            }
        break;


        case ST_START_ARMED:
            if(cond_arm_switch() != 1){
                stm = ST_PING_ESTAB;
                trans_ping_estab();
            }

            if(cond_trigger_pressed() == 1){

                ret = start_remote();
                if(ret == 0 || ret == 1){
                    stm = ST_PING_ESTAB;
                    trans_ping_estab();
                }
                else{
                    stm = ST_START_ERR;
                    trans_start_err();
                }
            }
        break;


        default:
            /* reached unsolved error state */
            Serial.println("crit error loop");
            delay(1000);

    }

    delay(50);
    timer_ms += 50;
}



void trans_init(){
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_WHITE, LOW);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, LOW);
}


void trans_ping_err(){

    ISR_Timer.deleteTimer(timer_green);
    timer_green = ISR_Timer.setInterval(PING_FAIL_BLINK, blink_green);

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;

    /* reset all 'higher' functions */
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_WHITE, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);

    delay(2500); /* delay further pings */
}

void trans_ping_estab(){

    ISR_Timer.deleteTimer(timer_green);
    timer_green = ESP8266_ISR_Timer::MAX_TIMERS;

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;

    ISR_Timer.deleteTimer(timer_red);
    timer_red = ESP8266_ISR_Timer::MAX_TIMERS;

    ISR_Timer.deleteTimer(timer_yellow);
    timer_yellow = ESP8266_ISR_Timer::MAX_TIMERS;
    
    digitalWrite(LED_GREEN, HIGH);

    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_WHITE, LOW);
}



void trans_pc_on(){

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;

    digitalWrite(LED_WHITE, HIGH);
}

void trans_pc_off(){

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;

    digitalWrite(LED_WHITE, LOW);
}


void trans_pc_err(){

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ISR_Timer.setInterval(STATUS_BLINK, blink_white);
    
    digitalWrite(LED_RED, HIGH);
}



void trans_arm_kill(){

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;

    timer_red = ISR_Timer.setInterval(ARMED_KILL, flash_red);    
}

void trans_arm_start(){

    ISR_Timer.deleteTimer(timer_white);
    timer_white = ESP8266_ISR_Timer::MAX_TIMERS;


    timer_yellow = ISR_Timer.setInterval(ARMED_START, flash_yellow);
}



void trans_kill_err(){

    ISR_Timer.deleteTimer(timer_red);

    timer_white = ISR_Timer.setInterval(STATUS_BLINK, blink_white);
    timer_red = ISR_Timer.setInterval(KILL_BLINK, blink_red);
    
    digitalWrite(LED_YELLOW, HIGH);
}


void trans_start_err(){

    ISR_Timer.deleteTimer(timer_yellow);

    timer_white = ISR_Timer.setInterval(STATUS_BLINK, blink_white);
    timer_red = ISR_Timer.setInterval(START_BLINK, blink_yellow);
    

    digitalWrite(LED_RED, HIGH);
}



int cond_ping_test(){
    int ret = 0;

    /* only blink with green led, no other blink allowed */
    timer_green = ISR_Timer.setInterval(PING_BLINK, blink_green);


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

    timer_white = ISR_Timer.setInterval(STATUS_BLINK, blink_white);

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

