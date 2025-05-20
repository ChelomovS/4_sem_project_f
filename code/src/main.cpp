#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "index.h"

#define LED_PIN 8
#define SENSOR_PIN 0
#define PUMP_PIN 21

#define PUMP_ON HIGH
#define PUMP_OFF LOW

uint32_t humidity_threshgold = 2048;
uint32_t water_time = 5000;
uint32_t wait_time = 5000;
uint32_t manual_water_time = 5000;

const char *ssid = "frkt_botalka";
const char *password = "FRTKbotalka";
const char *hostname = "watering_system";

enum STATUSES
{
    STATUS_WAITING_TIMEOUT,
    STATUS_WATERING,
    STATUS_WAITING_HUMIDITY,
    STATUS_MANUAL_CONTROL,
    STATUS_MANUAL_WATERING,
};

const char *auto_mode_name = "Auto";
const char *manual_mode_name = "Manual";
const char *manual_mode_on_name = "Manual ON";
const char *manual_mode_off_name = "Manual OFF";
const char *unknown_mode_name = "-";

STATUSES status = STATUS_WAITING_HUMIDITY;
STATUSES prev_status = STATUS_WAITING_HUMIDITY;
uint32_t timer;

const char *targetIP = "192.168.1.145";

WebServer server(80);

void turn_pump_off()
{
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);
}

void turn_pump_on()
{
    pinMode(PUMP_PIN, INPUT);
    status = STATUS_MANUAL_CONTROL;
}

const char *get_mode_name()
{
    switch (status)
    {
    case STATUS_WAITING_TIMEOUT:
    case STATUS_WAITING_HUMIDITY:
    case STATUS_WATERING:
        return auto_mode_name;
    case STATUS_MANUAL_WATERING:
        return manual_mode_name;
    case STATUS_MANUAL_CONTROL:
        if (digitalRead(PUMP_PIN) == PUMP_ON)
            return manual_mode_on_name;
        else
            return manual_mode_off_name;
    default:
        return unknown_mode_name;
    }
}

void handle_index()
{
    server.send(200, "text/html", (const char *)web_dashboard_index_min_html);
}

void handle_on()
{
    turn_pump_on();
    server.send(200, "text/plaintext", "OK");
}

void handle_auto()
{
    status = STATUS_WAITING_HUMIDITY;
    server.send(200, "text/plaintext", "OK");
}

void handle_off()
{
    turn_pump_off();
    status = STATUS_MANUAL_CONTROL;
    server.send(200, "text/plaintext", "OKHYUAFDG");
}

void handle_manual()
{
    timer = millis();
    turn_pump_on();
    prev_status = status;
    status = STATUS_MANUAL_WATERING;
    server.send(200, "text/plaintext", "OK");
}

void handle_info()
{
    char buffer[512];
    int size = snprintf(buffer, sizeof(buffer), "%d\n%d\n%s", analogRead(SENSOR_PIN), status, get_mode_name());
    server.send(200, "text/plaintext", buffer);
}

void handle_set_parameters()
{
    sscanf(server.arg(0).c_str(),
           "%d %d %d %d",
           &humidity_threshgold,
           &water_time,
           &wait_time,
           &manual_water_time);

    char buffer[512];
    int size = snprintf(buffer, sizeof(buffer), "A: %d, B: %d, C: %d, D: %d", humidity_threshgold, water_time, wait_time, manual_water_time);
    server.send(200, "text/plaintext", buffer);
}

void setup()
{
    pinMode(SENSOR_PIN, ANALOG);
    pinMode(LED_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
    }

    server.on("/", handle_index);
    server.on("/set_mode/on", handle_on);
    server.on("/set_mode/auto", handle_auto);
    server.on("/set_mode/off", handle_off);
    server.on("/info", handle_info);
    server.on("/set_mode/manual", handle_manual);
    server.on("/set_parameters", HTTP_POST, handle_set_parameters);

    MDNS.begin(hostname);
    server.begin();

    digitalWrite(LED_PIN, HIGH);
}

void loop()
{
    switch (status)
    {
    case STATUS_WAITING_HUMIDITY:
        if (analogRead(SENSOR_PIN) > humidity_threshgold)
        {
            turn_pump_on();
            status = STATUS_WATERING;
            timer = millis();
        }
        break;
    case STATUS_WATERING:
        if (millis() - timer > water_time)
        {
            turn_pump_off();
            status = STATUS_WAITING_TIMEOUT;
            timer = millis();
        }
        break;
    case STATUS_WAITING_TIMEOUT:
        if (millis() - timer > wait_time)
        {
            status = STATUS_WAITING_HUMIDITY;
            timer = millis();
        }
        break;
    case STATUS_MANUAL_WATERING:
        if (millis() - timer > manual_water_time)
        {
            status = prev_status;
            turn_pump_off();
            timer = millis();
        }
        break;
    }
    server.handleClient();
}