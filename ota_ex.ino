#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <WiFi.h>

#define OTA_TIME_LIMIT 360000
#define WiFi_Pin 14
#define OTA_Pin 16

QueueHandle_t ErrorQueue;
WiFiManager wifiManager;

unsigned long currentMillis = 0;
unsigned long otaStartTime = 0;
int OTA_Handler = false;
int isOTAUpdating = false;
bool otaStarted = false;

void setup() {
    Serial.begin(9600);
    //while(!Serial);
    Serial.println("Start");

    pinMode(WiFi_Pin, INPUT_PULLUP);
    pinMode(OTA_Pin, INPUT_PULLUP);
    xTaskCreatePinnedToCore(WiFi_Connect, "WiFi_Connect", 10000, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(OTATask, "OTATask", 10000, NULL, 2, NULL, 1);
}

void WiFi_Connect(void *pvParameters) {
    Serial.println("Void WiFi");
    for (;;) {
        if (digitalRead(WiFi_Pin) == LOW) {
            wifiManager.resetSettings();
            wifiManager.setConfigPortalTimeout(180);
        }
        if (WiFi.status() != WL_CONNECTED) {
            wifiManager.setConfigPortalTimeout(180); // Set timeout to 180 seconds
            if (!wifiManager.autoConnect("AutoConnectAP")) {
                ESP.restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void OTATask(void *pvParameters) {
    for (;;) {        
        if (WiFi.status() == WL_CONNECTED && otaStarted == false) {
            ArduinoOTA.begin();
            ArduinoOTA.setHostname("myesp32"); // Set the hostname
            ArduinoOTA.setPassword("asdf"); // Set the password
            Serial.println("ota started");
            otaStarted = true;
        }
        if (digitalRead(OTA_Pin) == LOW && otaStartTime == 0 && otaStarted == true) {
            Serial.println("OTA Pin pressed");
            otaStartTime = millis();
        }
        if (otaStartTime > 0 && millis() - otaStartTime < OTA_TIME_LIMIT) {
            Serial.print(".-");
            ArduinoOTA.handle();
        } else if (otaStarted == true && millis() - otaStartTime > OTA_TIME_LIMIT)
        {
            Serial.println("ota reset");
            otaStartTime = 0;
        }


        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
            } else { // U_SPIFFS
            type = "filesystem";
            }
            Serial.println("OTA Start: " + type);
        });

        ArduinoOTA.onEnd([]() {
            Serial.println("\nOTA Ende");
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("OTA Fortschritt: %u%%\r", (progress / (total / 100)));
        });

        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Fehler [%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Fehler");
            } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Fehler");
            } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Fehler");
            } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Fehler");
            } else if (error == OTA_END_ERROR) {
            Serial.println("End Fehler");
            }
        });


        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void loop() {
}