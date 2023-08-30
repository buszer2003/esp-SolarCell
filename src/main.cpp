// ADS1115, ACS712-20A

const char version[7] = "1.3.02";
const char built_date[9] = "20230830";
const char built_time[5] = "2118";

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_ADS1X15.h>
#include <PubSubClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ArduinoJson.h>

Adafruit_ADS1115 ads;

int16_t adc0;
int offset = 2066;
double current = 0.00;
const byte analogInput = A0;
float Vout = 0.00;
float Vin = 0.00;
float R1 = 6005.00; // resistance of R1 (10K)
float R2 = 1000.00; // resistance of R2 (1K)
int val = 0;
long sum = 0;
unsigned long time_getC;
unsigned long getBattDelay;
unsigned long publishMQTT;
unsigned long mqttESPinfo;
unsigned long reconnTime;

const char *ssid     = "BZ_IOT";
const char *password = "Password";

IPAddress local_IP(192, 168, 1, 202);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
IPAddress secondaryDNS(192, 168, 1, 13);

#define MQTT_HOST IPAddress(192, 168, 1, 3)

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);

void getC() {
    adc0 = ads.readADC_SingleEnded(0);
    double v = (adc0 / 32768.0) * 5050;
    current = (v - offset) / 100;

    if (current < 0.01) {
        current = 0;
	}
}

void getBatteryInfo() {
    val = analogRead(analogInput);
    Vout = (val * 5.00) / 1024.00;
    Vin = Vout / (R2 / (R1 + R2));

    if (Vin < 0.1) {
        Vin = 0.00;
	}
}

void connectToWifi() {
	Serial.println("Connecting to Wi-Fi...");
	if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
		Serial.println("STA Failed to configure");
	}
	WiFi.begin(ssid, password);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(true);
	Serial.print("Connected to ");
	Serial.println(WiFi.SSID());
}

void reconnect() {
    if (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP8266Client")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
        }
    }
}

void setup() {
    pinMode(analogInput, INPUT);
    Serial.begin(115200);
    Serial.println("Booting");

    connectToWifi();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266. ESP-SolarCell\nVersion: " + String(version)  + "\nBuilt Date: " + String(built_date) + "\nBuilt Time: " + String(built_time));
	});
	
	AsyncElegantOTA.begin(&server);         // Start ElegantOTA
    server.begin();                         // ElegantOTA
    client.setServer(MQTT_HOST, 1883);      // MQTT
	
    if (!ads.begin()) {
        Serial.println("ADS Failed!");
    }
    delay(1000);
}

void loop() {
    client.loop();

    if (millis() - time_getC > 200) {
        getC();
		time_getC = millis();
    }
    if (millis() - getBattDelay > 1000) {
        getBatteryInfo();
		getBattDelay = millis();
    }

    if (!client.connected()) {
		if (millis() - reconnTime > 1000) {
			reconnect();
			reconnTime = millis();
		}
	} else {
        if (millis() - publishMQTT > 60000) {
            DynamicJsonDocument docInfo(64);
            String MQTT_STR;
            docInfo["volt"] = String(Vin, 2);
            docInfo["current"] = String(current, 2);
            serializeJson(docInfo, MQTT_STR);
            client.publish("esp/solarcell/log", MQTT_STR.c_str());
            publishMQTT = millis();
        }

        if (millis() - mqttESPinfo > 1000) {
            DynamicJsonDocument docInfo(128);
            String MQTT_STR;
            docInfo["volt"] = String(Vin, 2);
            docInfo["current"] = String(current, 2);
            docInfo["rssi"] = String(WiFi.RSSI());
            docInfo["uptime"] = String(millis()/1000);
            serializeJson(docInfo, MQTT_STR);
            client.publish("esp/solarcell/info", MQTT_STR.c_str());
            mqttESPinfo = millis();
        }
    }
}
