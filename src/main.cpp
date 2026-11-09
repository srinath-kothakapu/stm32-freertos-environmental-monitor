/*
  FreeRTOS Environmental Monitor on STM32 (Blue Pill, STM32F103C8T6)

  Four FreeRTOS tasks:
    SensorTask  (priority 2)  reads the DHT11 every SENSOR_PERIOD_MS and sends the reading to a queue
    AlertTask   (priority 3)  receives each reading, checks the thresholds, drives the LED and buzzer
    DisplayTask (priority 1)  shows the latest values and alert state on a 16x2 I2C LCD
    LogTask     (priority 1)  streams one CSV line per reading over UART

  Data flow:  SensorTask --queue--> AlertTask --shared state (mutex) + task notifications--> DisplayTask, LogTask

  The CSV line includes "latency_ms": the time from the moment the sensor was read to the moment the
  alert outputs were updated. This is measured on the board, so you can check the real response time.
*/

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

struct Reading {
  float temperatureC;
  float humidityPct;
  TickType_t sampleTick;   // tick count when the sensor was read
};

struct SharedState {
  Reading last;
  bool alert;
  uint32_t latencyMs;
  bool valid;
};

static DHT dht(PIN_DHT, DHT11);
static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

static QueueHandle_t sensorQueue;
static SemaphoreHandle_t stateMutex;
static TaskHandle_t displayTaskHandle, logTaskHandle;
static SharedState state = {};

// ---------------------------------------------------------------- SensorTask
static void sensorTask(void *) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    Reading r;
    r.sampleTick = xTaskGetTickCount();
    r.humidityPct = dht.readHumidity();
    r.temperatureC = dht.readTemperature();
    if (!isnan(r.humidityPct) && !isnan(r.temperatureC)) {
      xQueueSend(sensorQueue, &r, 0);   // drop the reading if the queue is full
    }
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}

// ----------------------------------------------------------------- AlertTask
static void alertTask(void *) {
  Reading r;
  for (;;) {
    if (xQueueReceive(sensorQueue, &r, portMAX_DELAY) == pdTRUE) {
      bool alert = (r.temperatureC > TEMP_ALERT_C) || (r.humidityPct > HUMIDITY_ALERT_PCT);
      digitalWrite(PIN_ALERT_LED, alert ? HIGH : LOW);
      digitalWrite(PIN_BUZZER, alert ? HIGH : LOW);
      uint32_t latency = (uint32_t)(xTaskGetTickCount() - r.sampleTick) * 1000UL / configTICK_RATE_HZ;

      xSemaphoreTake(stateMutex, portMAX_DELAY);
      state.last = r;
      state.alert = alert;
      state.latencyMs = latency;
      state.valid = true;
      xSemaphoreGive(stateMutex);

      xTaskNotifyGive(displayTaskHandle);
      xTaskNotifyGive(logTaskHandle);
    }
  }
}

// --------------------------------------------------------------- DisplayTask
static void displayTask(void *) {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Env. monitor");
  lcd.setCursor(0, 1);
  lcd.print("waiting...");
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    SharedState s;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    s = state;
    xSemaphoreGive(stateMutex);

    char line0[LCD_COLUMNS + 1], line1[LCD_COLUMNS + 1];
    snprintf(line0, sizeof(line0), "T:%5.1fC H:%3.0f%% ", s.last.temperatureC, s.last.humidityPct);
    snprintf(line1, sizeof(line1), "%-16s", s.alert ? "ALERT!" : "OK");
    lcd.setCursor(0, 0); lcd.print(line0);
    lcd.setCursor(0, 1); lcd.print(line1);
  }
}

// ------------------------------------------------------------------- LogTask
static void logTask(void *) {
  Serial.println("tick_ms,temperature_c,humidity_pct,alert,latency_ms");
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    SharedState s;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    s = state;
    xSemaphoreGive(stateMutex);

    Serial.print(s.last.sampleTick); Serial.print(',');
    Serial.print(s.last.temperatureC, 1); Serial.print(',');
    Serial.print(s.last.humidityPct, 1); Serial.print(',');
    Serial.print(s.alert ? 1 : 0); Serial.print(',');
    Serial.println(s.latencyMs);
  }
}

// --------------------------------------------------------------------- setup
void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_ALERT_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_ALERT_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  dht.begin();

  sensorQueue = xQueueCreate(4, sizeof(Reading));
  stateMutex = xSemaphoreCreateMutex();

  xTaskCreate(sensorTask,  "sensor",  256, nullptr, 2, nullptr);
  xTaskCreate(alertTask,   "alert",   256, nullptr, 3, nullptr);
  xTaskCreate(displayTask, "display", 384, nullptr, 1, &displayTaskHandle);
  xTaskCreate(logTask,     "log",     384, nullptr, 1, &logTaskHandle);

  vTaskStartScheduler();   // does not return
}

void loop() {
  // not used, the FreeRTOS scheduler runs the tasks
}
