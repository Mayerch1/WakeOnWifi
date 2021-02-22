#include <stdio.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#include "secret.h"
#include "pins.h"

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define POWER_SENSE D7
#define RELAIS_PIN D6


ESP8266WebServer http_rest_server(HTTP_REST_PORT);

const char* headerKeys[] = {"secret"};
int numberOfHeaders = 1;

int init_wifi() {
    int retries = 0;

    Serial.println("Connecting to WiFi AP..........");

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID_WiFi, PASSWD_WiFi);
    // check the status of WiFi connection to be WL_CONNECTED
    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    return WiFi.status(); // return the WiFi connection status
}


/*
// \brief - test if the header "secret" matches the define HTTP_SECRET
//          sends 401 to client if secret is not matching
// \return - 1 on valid secret, 0 on invalid secret
*/
char testSecret(bool on_failure_401 = true){
    String rxSecret = http_rest_server.header("secret");

    Serial.println("Received secret was " + rxSecret);

    if (rxSecret != HTTP_SECRET){
        http_rest_server.send(401);
        return 0;
    }

    return 1;
}


void toggleRelais(int delay_ms){
    digitalWrite(RELAIS_PIN, HIGH);
    delay(delay_ms);
    digitalWrite(RELAIS_PIN, LOW);
}


bool is_powered(){
    int state = digitalRead(POWER_SENSE);
    return state;
}




void browser_page(){

    if(!testSecret(false)){
        http_rest_server.send(200, "application/json", "Please enter secret");
    }
    else{
        bool state = is_powered();

        char* page;
        if(state){
            page = "<form method=\"get\" action=\"/kill\">"\
                    "   <button type=\"submit\">Kill Server</button>"\
                    "</form>"\
                    "<form method=\"get\" action=\"/status\">"\
                    "    <button type=\"submit\">Get Status</button>"\
                    "</form>"\
                    "<form method=\"get\" action=\"/ping\">"\
                    "    <button type=\"submit\">Ping Server</button>"\
                    "</form>";
        }
        else{
            page =  "<form method=\"get\" action=\"/start\">"\
                    "   <button type=\"submit\">Start Server</button>"\
                    "</form>"\
                    "<form method=\"get\" action=\"/status\">"\
                    "    <button type=\"submit\">Get Status</button>"\
                    "</form>"\
                    "<form method=\"get\" action=\"/ping\">"\
                    "    <button type=\"submit\">Ping Server</button>"\
                    "</form>";
        }
        
                       

        http_rest_server.send(200, "application/json", page);
    }

    
}


void power_on() {

    if(!testSecret())
    {return;}

    Serial.println("Power-on");

    if(digitalRead(POWER_SENSE) == 0){
        toggleRelais(500);
        http_rest_server.send(200);
    }
    else{
        // precondition failed
        http_rest_server.send(412);
    }

}


void power_suspend(){

    if(!testSecret())
    {return;}


    Serial.println("Power-suspend");

    // only if pc is Started
    // shortly press power button
    if(digitalRead(POWER_SENSE) != 0){
        toggleRelais(500);
        http_rest_server.send(200);
    }
    else{
        // precondition failed
        http_rest_server.send(412);
    }

    
}

void power_kill(){

    if(!testSecret())
    {return;}

    
    Serial.println("Power-kill");

    // only if pc is started
    if(digitalRead(POWER_SENSE) != 0){
        toggleRelais(5000);
        http_rest_server.send(200);
    }
    else{
        // precondition failed
        http_rest_server.send(412);
    }
    
}


void power_state(){
    Serial.println("Reading power state");

    bool state = is_powered();

    char* status_msg;
    if (state){
        status_msg = "{\"status\": \"on\"}";
        
    }
    else{
        status_msg = "{\"status\": \"off\"}";
    }

    http_rest_server.send(200, "application/json", status_msg);
}


void ping_client(){
    // ping to check device status
    http_rest_server.send(200);
}



void config_rest_server_routing() {
    http_rest_server.on("/", HTTP_GET, browser_page);
    http_rest_server.on("/start", HTTP_GET, power_on);
    http_rest_server.on("/suspend", HTTP_GET, power_suspend);
    http_rest_server.on("/kill", HTTP_GET, power_kill);
    http_rest_server.on("/status", HTTP_GET, power_state);
    http_rest_server.on("/ping", HTTP_GET, ping_client);
}


void setup(void) {
    Serial.begin(9600);
    while(!Serial){delay(5);}

    // setup pins for triggering relais
    // and for sensing the power state
    pinMode(POWER_SENSE, INPUT);
    pinMode(RELAIS_PIN, OUTPUT);

    digitalWrite(RELAIS_PIN, LOW);


    if (init_wifi() == WL_CONNECTED) {
        Serial.print("Connetted to ");
        Serial.print(SSID_WiFi);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
    }
    else {
        Serial.print("Error connecting to: ");
        Serial.println(SSID_WiFi);
        Serial.println("Restarting in 10s");
        delay(10000);
        ESP.restart();
    }

    config_rest_server_routing();

    http_rest_server.begin();
    http_rest_server.collectHeaders(headerKeys, numberOfHeaders);

    Serial.println("HTTP REST Server Started");
}

void loop(void) {
    http_rest_server.handleClient();
}
