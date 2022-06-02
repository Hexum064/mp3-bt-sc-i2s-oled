#ifndef DEFINES_H
#define DEFINES_H

#define DEBUG

#define CONFIG_SPEAKER_NAME "Q50"
#define BT_SINK_NOT_SELECTED_NAME "<NOT SELECTED>"
#define BYTE_SAMPLES_PER_FRAME (MINIMP3_MAX_SAMPLES_PER_FRAME * 2) //MINIMP3_MAX_SAMPLES_PER_FRAME is number of short samples so this is for total bytes


// speaker settings - if using I2S
#define I2S_SPEAKER_SERIAL_CLOCK GPIO_NUM_4
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK GPIO_NUM_5
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_18



#define DISPLAY_SDA	GPIO_NUM_21
#define DISPLAY_SCL GPIO_NUM_22
#define DISPLAY_RST GPIO_NUM_NC

#define FRONT_DISPLAY_PIN GPIO_NUM_27

#define OLED_DISP_INT 250

#define MOUNT_POINT "/sdcard"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  GPIO_NUM_12
#define PIN_NUM_MOSI  GPIO_NUM_13
#define PIN_NUM_CLK   GPIO_NUM_14
#define PIN_NUM_CS    GPIO_NUM_15
#define PIN_NUM_CD    GPIO_NUM_27
#define SPI_DMA_CHAN    2


#define SCROLL_HOLD 4

#define NYAN_BASE_PATH "/nyan_data"
#define NYAN_MP3_PATH "/nyan_data/Nyan3.mp3"

#define ADC_BATT_ATTEN ADC_ATTEN_DB_6

#define FIRMWARE_VERSION "Firmware v 1.0  "

#define ADC_BATT_CHANNEL ADC1_CHANNEL_0
#define BATT_100 2000	//approx 6v with a 100K/22K divider
#define BATT_0	1500	//approx 4.1v with a 100K/22K divider
#define BATT_ADC_RANGE (BATT_100 - BATT_0)
#define BATT_ROLLING_AVG_CNT 50

#define MAX_VOL 4096
#define VOL_STEP (MAX_VOL / 16)
#define MAX_DISPLAY_MODES 7
#define SECONDS_TILL_RESET_CURRENT_SONG 2

#define BUTTON_BIT_0 GPIO_NUM_2
#define BUTTON_BIT_1 GPIO_NUM_16
#define BUTTON_BIT_2 GPIO_NUM_17

#define SHORT_BUTTON_PUSH 50000ULL
#define LONG_BUTTON_HOLD 750000ULL

#define VOL_UP_BUTTON 7
#define VOL_DOWN_BUTTON 6
#define DISPLAY_BUTTON 5
#define OUTPUT_BUTTON 4
#define NEXT_BUTTON 3
#define PAUSE_BUTTON 2
#define PREVIOUS_BUTTON 1

#define LEFT_LED_COUNT 250
#define RIGHT_LED_COUNT 252
#define RGB_LED_COUNT (LEFT_LED_COUNT+RIGHT_LED_COUNT)
#define RGB_LED_BYTE_COUNT (RGB_LED_COUNT * 3)
#define RIGHT_SCROLL_LEN 131
#define DAFT_PUNK_SCROLL_LEN 80
#define FFT_SAMPLE_SIZE_POWER 11
#define FFT_SAMPLE_SIZE (1 << FFT_SAMPLE_SIZE_POWER) 
#define FFT_BINS 8
#define VAL_OFFSET 1024

#endif //DEFINES_H