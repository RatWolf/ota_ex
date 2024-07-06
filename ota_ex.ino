#include <FreeRTOS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <FastLED.h>

# define debug_mode true
#define OTA_TIME_LIMIT 180000
#define Status_LED 21
#define Status_LED_count 1
#define WiFi_Pin 14
#define OTA_Pin 16

QueueHandle_t ErrorQueue;
WiFiManager wifiManager;
CRGB internal_LED[Status_LED_count];

unsigned long currentMillis = 0;
unsigned long Status_LED_start_mil = 0;
unsigned long Status_LED_brightness = 0;
unsigned long Status_LED_maxBrightness = 50;
unsigned long Status_LED_fadeDuration = 5000;
unsigned long Status_LED_timeInCycle = 0;
unsigned long Status_LED_distanceFromMiddle = 0;
unsigned long otaStartTime = 0;
int OTA_Handler = false;
int isOTAUpdating = false;
bool otaStarted = false;
int ErrorCode = 0;
int errCode = 0;

void setup() {
    Serial.begin(9600);
    //while(!Serial);
    Serial.println("Start");

    ErrorQueue = xQueueCreate(10, sizeof(int));
    if (ErrorQueue == NULL) {
        Serial.println("Queue konnte nicht erstellt werden");
    }

    pinMode(WiFi_Pin, INPUT_PULLUP);
    pinMode(OTA_Pin, INPUT_PULLUP);

    Status_LED_start_mil = millis();
    FastLED.addLeds<WS2812, Status_LED>(internal_LED, Status_LED_count);
    FastLED.setBrightness(Status_LED_maxBrightness);
    xTaskCreatePinnedToCore(blinkLED, "blinkLED", 10000, NULL, 4, NULL, 0); // Erstes Null ist pvParameters, da xTaskCreatePinnedToCore ohne ein parameter nicht funktioniert?
    xTaskCreatePinnedToCore(WiFi_Connect, "WiFi_Connect", 10000, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(OTATask, "OTATask", 10000, NULL, 2, NULL, 1);
    ErrorCode = 1;
    if (xQueueSend(ErrorQueue, &ErrorCode, portMAX_DELAY) != pdPASS) {
        Serial.println("Fehler beim Senden zur Queue");
    }
}

/* Blink codes:
    1 = Gelb 2 Blinken 1sek (Power on)
    2 = Blau 3sek (WiFi connected)
    3 = OTA updating
    4 = OTA finished
    5 = Error
*/
void blinkLED(void *pvParameters) {
    #if (debug_mode == true)
        int count = 0;
        bool countend = false;
        for (;;) {
            currentMillis = millis();
            if (currentMillis >= Status_LED_start_mil + Status_LED_fadeDuration) {
                Status_LED_start_mil = currentMillis;
                if (count > 0) {
                    count -= 1;
                    if (count == 0) {
                        countend = true;
                    }                   
                }
            }
            Status_LED_timeInCycle = currentMillis % Status_LED_fadeDuration;

            float phase = PI / Status_LED_fadeDuration;
            float sinValue = sin(phase * Status_LED_timeInCycle);
            Status_LED_brightness = map((int)(sinValue * 100), 0, 100, 0, Status_LED_maxBrightness);

            if (xQueuePeek(ErrorQueue, &errCode, 0) == pdPASS) {
                switch (errCode) {
                    case 1: // Power on boot (Grün)
                        Status_LED_fadeDuration = 500;
                        internal_LED[0] = CRGB(Status_LED_brightness, Status_LED_brightness, 0);
                        if (countend){
                            xQueueReceive(ErrorQueue, &errCode, 0);
                            Status_LED_fadeDuration = 5000;
                        } else if (count == 0) {
                            count = 2;
                            countend = false;
                        }
                        Serial.println("Boot" + String(count));
                        break;
                    case 2: // WiFi connected (Blau)
                        internal_LED[0] = CRGB(0,0,Status_LED_brightness);
                        break;
                    case 3: // WiFi Manager (Türkis)
                        internal_LED[0] = CRGB(0,Status_LED_brightness,Status_LED_brightness);
                        break;
                    case 4: // OTA updating (Lila)
                        internal_LED[0] = CRGB(Status_LED_brightness, 0, Status_LED_brightness);
                        break;
                    default: // Error (rot-statisch)
                        internal_LED[0] = CRGB(255, 0, 0);
                        break;
                }
            } else {
                internal_LED[0] = CRGB(0, Status_LED_brightness, 0);
            }
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    #endif
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

        if (wifiManager.getConfigPortalActive()) {
            ErrorCode = 3;
            if (xQueueSend(ErrorQueue, &ErrorCode, portMAX_DELAY) != pdPASS) {
                Serial.println("Fehler beim Senden zur Queue");
            }
        } else if (xQueuePeek(ErrorQueue, &errCode, 0) == pdPASS) {
            if (errCode == 3) {
                xQueueReceive(ErrorQueue, &errCode, 0);
            }
        }
        
        if (xQueuePeek(ErrorQueue, &errCode, 0) == pdPASS) {
            if (WiFi.status() == WL_CONNECTED && errCode != 2) {
                ErrorCode = 2;
                if (xQueueSend(ErrorQueue, &ErrorCode, portMAX_DELAY) != pdPASS) {
                    Serial.println("Fehler beim Senden zur Queue");
                }
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
            otaStarted = true;
        }
        if (digitalRead(OTA_Pin) == LOW && otaStartTime == 0 && otaStarted == true) {
            Serial.println("OTA Pin pressed");
            isOTAUpdating = true;
            otaStartTime = millis();
        }
        if (otaStartTime > 0 && millis() - otaStartTime < OTA_TIME_LIMIT && otaStarted == true) {
            ArduinoOTA.handle();
        } else {
            isOTAUpdating = false;
            otaStartTime = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void loop() {
}