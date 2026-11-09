#pragma once
// Pin map for STM32F103C8T6 (Blue Pill)
#define PIN_DHT        PB12   // DHT11 data pin (3.3 V, 10 k pull-up to 3.3 V if the module has none)
#define PIN_ALERT_LED  PB0    // external alert LED (through a resistor)
#define PIN_BUZZER     PB1    // active buzzer
// Serial (USART1): PA9 = TX, PA10 = RX. I2C1 for the LCD: PB6 = SCL, PB7 = SDA.

#define LCD_I2C_ADDR   0x27   // common PCF8574 backpack address (some use 0x3F)
#define LCD_COLUMNS    16
#define LCD_ROWS       2

// Alert thresholds
#define TEMP_ALERT_C       30.0f
#define HUMIDITY_ALERT_PCT 70.0f

// Task timing
#define SENSOR_PERIOD_MS   2000   // DHT11 needs at least 1 s between reads
#define SERIAL_BAUD        115200
