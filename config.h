#ifndef CONFIG_H
#define CONFIG_H

// Pin numbers copied from "firmware 1.0/pala_note/config.h" — same board.

#define EPD_SPI_NUM   SPI2_HOST

/* E-paper SPI pins */
#define EPD_DC_PIN    GPIO_NUM_10
#define EPD_CS_PIN    GPIO_NUM_11
#define EPD_SCK_PIN   GPIO_NUM_12
#define EPD_MOSI_PIN  GPIO_NUM_13
#define EPD_RST_PIN   GPIO_NUM_9
#define EPD_BUSY_PIN  GPIO_NUM_8

/* Power control pins */
#define EPD_PWR_PIN    GPIO_NUM_6
#define AUDIO_PWR_PIN  GPIO_NUM_42
#define VBAT_PWR_PIN   GPIO_NUM_17
#define PWR_HOLD_PIN   17            // latches the 3V3 rail on while running from battery

/* Buttons (active LOW, INPUT_PULLUP) */
#define BTN_1  0    // labelled "REC" on the board
#define BTN_2  18   // labelled "PWR" on the board

/* Battery ADC */
#define BAT_ADC_PIN  4   // ADC1_CHANNEL_3 on ESP32-S3

/* I2C bus — used by the audio codec (ES8311) init in src/codec_board */
#define ESP32_I2C_DEV_NUM     I2C_NUM_0
#define ESP32_I2C_SDA_PIN     GPIO_NUM_47
#define ESP32_I2C_SCL_PIN     GPIO_NUM_48
#define I2C_RTC_DEV_Address   0x51
#define I2C_SHTC3_DEV_Address 0x70

/* Audio */
#define SAMPLE_RATE  16000

#endif // CONFIG_H
