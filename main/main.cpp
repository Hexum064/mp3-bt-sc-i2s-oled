//TODO: Fix found BT device list scrolling

#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include "SDCard.h"
#include "esp_log.h"
#include "BT_a2dp.h"
#include "MP3Player.h"
#include "FileNavi.h"
#include <driver/gpio.h>
#include "I2SOutput.h"
#include "ssd1306_i2c.h"
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include <math.h>
#include "esp_spiffs.h"
#include "utilities.h"
#include "button_handling.h"
#include "rgb_led_displays.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "oled_display.h"
#include "sdkconfig.h"
extern "C"
{
	#include "btc_a2dp_source.h"
}

TaskHandle_t buttonsTaskHandle = NULL;
TaskHandle_t oledTaskHandle = NULL;
TaskHandle_t i2sTaskHandle = NULL;



short buff0[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
short buff1[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
uint8_t buff_num = 0;
int buff_pos = 0;

bool has_started = false;

bool f_muted = false;

Output *output = NULL;

i2s_pin_config_t i2s_speaker_pins;

esp_vfs_spiffs_conf_t spiffs_cfg;

SSD1306_t oled_display;


void init_button_input()
{
	//Button input is expected to be a multipled 7 to 3 signal where the 3 is a binary number from 0 to 7. 0 represents no buttons and 1 - 7 represent button 1 through 7.
	gpio_config_t io_conf = {
		.pin_bit_mask = ((1ULL << BUTTON_BIT_0) | (1ULL << BUTTON_BIT_1) | (1ULL << BUTTON_BIT_2) | (1ULL << PIN_NUM_CD)) ,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	gpio_config(&io_conf);

}

void init_audio_out()
{
	// i2s speaker pins
i2s_pin_config_t i2s_speaker_pins = {
	.mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
    .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPEAKER_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE};

	output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);

}

void init_display()
{


	ESP_LOGI("main", "INTERFACE is i2c");
	ESP_LOGI("main", "CONFIG_SDA_GPIO=%d",DISPLAY_SDA);
	ESP_LOGI("main", "CONFIG_SCL_GPIO=%d",DISPLAY_SCL);
	ESP_LOGI("main", "CONFIG_RESET_GPIO=%d",DISPLAY_RST);
	i2c_master_init(&oled_display, (gpio_num_t)DISPLAY_SDA, (gpio_num_t)DISPLAY_SCL, (gpio_num_t)DISPLAY_RST);


	ESP_LOGI("main", "Panel is 128x64");
	ssd1306_init(&oled_display, 128, 64);
	ssd1306_clear_screen(&oled_display, false);
	ssd1306_contrast(&oled_display, 0xff);


	if (bt_discovery_mode)
	{
		return;
	}

	char bfb[] = "BigFuckingBadge";
	char foxs_random[] = "Fox's Random";
	char access_memories[] = " Access Memories";
	char dc30[] = "DC30";

	ssd1306_display_text(&oled_display, 0, bfb, 15, false);
	ssd1306_display_text(&oled_display, 1, foxs_random, 12, false);
	ssd1306_display_text(&oled_display, 2, access_memories, 16, false);
	ssd1306_display_text(&oled_display, 3, dc30, 4, false);
	ssd1306_display_text(&oled_display, 7, FIRMWARE_VERSION, 16, false);
	
	vTaskDelay(pdMS_TO_TICKS(2000));
}


void init_rgb_led_spi(spi_dma_chan_t channel)
{


    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_23,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = GPIO_NUM_19,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000
    };
   	printf("SPI init: %d\n", spi_bus_initialize(SPI3_HOST, &bus_cfg, channel));
    spi_device_interface_config_t devcfg={
        .mode = 0,          //SPI mode 0        
		.clock_speed_hz = 640000, //Should be 800KHz, slowed down because our spi to async RGB is too slow
        .spics_io_num = -1,
        .queue_size = 1
    };

	printf("SPI add device: %d\n", spi_bus_add_device( SPI3_HOST, &devcfg, &rgb_led_spi_handle));

	//Set this up to clear all the lights
	memset(rgb_led_spi_tx_buff, 0, RGB_LED_BYTE_COUNT);

	rgb_led_spi_trans.length = RGB_LED_BYTE_COUNT * 8; //length in bits
	rgb_led_spi_trans.tx_buffer = rgb_led_spi_tx_buff;
	// trans.flags = SPI_TRANS_MODE_OCT;
	 printf("SPI queue transmit: %d\n", spi_device_queue_trans(rgb_led_spi_handle, &rgb_led_spi_trans, portMAX_DELAY));	
	//printf("SPI transmit: %d\n", spi_device_transmit(rgb_led_spi_handle, &rgb_led_spi_trans));	
}

void init_spiffs()
{
	spiffs_cfg = {
		.base_path = NYAN_BASE_PATH,
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = false
	};	
}


void init_bt_device_info()
{
	size_t temp_len;
	int get_name_res = nvs_get_str(bt_control->get_nvs_handle(), bt_sink_name_key, bt_sink_name, &temp_len);
	uint8_t addr;
	
	bt_sink_addr_key[6] = '0';
	printf("Reading addr 0: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[0] = addr;

	bt_sink_addr_key[6] = '1';
	printf("Reading addr 1: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[1] = addr;

	bt_sink_addr_key[6] = '2';
	printf("Reading addr 2: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[2] = addr;

	bt_sink_addr_key[6] = '3';
	printf("Reading addr 3: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[3] = addr;

	bt_sink_addr_key[6] = '4';
	printf("Reading addr 4: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[4] = addr;

	bt_sink_addr_key[6] = '5';
	printf("Reading addr 5: %d\n", nvs_get_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, &addr));
	bt_sink_addr[5] = addr;

	printf("Get results. Name: %d\n", get_name_res);

	if (get_name_res != ESP_OK)
	{
		strcpy(bt_sink_name, BT_SINK_NOT_SELECTED_NAME);	
		bt_enabled = false;			
	}

}

void init_adc()
{
	ESP_LOGI("main", "Starting ADC Init\n");
	printf("adc1 config width: %d\n", adc1_config_width((adc_bits_width_t)ADC_WIDTH_BIT_DEFAULT));
    printf("adc1 config atten: %d\n", adc1_config_channel_atten(ADC_BATT_CHANNEL, ADC_BATT_ATTEN));

	//Do a quick battery reading to prime the output

	for (int i = 0; i < BATT_ROLLING_AVG_CNT; i++)
	{
		vTaskDelay(pdMS_TO_TICKS(5));
		get_batt_percent();
		
	}
}





static uint8_t esp_a2d_found_devices_cb(bt_device_param *devices, uint8_t count)
{
	bt_discovered_devices = devices;
	bt_discovered_count = count;
	for (uint8_t i = 0; i < count; i++)
	{
		printf("Found %s at address %02x:%02x:%02x:%02x:%02x:%02x\n", devices[i].name, devices[i].address[0], devices[i].address[1], devices[i].address[2], devices[i].address[3], devices[i].address[4], devices[i].address[5]);
	}
	return 0;
}

static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len)
{
	uint32_t val = 0;
	short * buff = buff0;
	int shortLen = len >> 1;
	//static uint16_t sample_rate = 0;

	//sd card detect
	if (!gpio_get_level(PIN_NUM_CD))
	{
		playing = false;
	}

	if (!(playing) || i2s_output) // || !has_started)
	{
		//ESP_LOGI("Main", "waiting");

		if (data)
		{
			memset(data, 0, len);
		}
		return len;
	}

	if (buff_num)
	{
		buff = buff1;
	} 

	//We can do this because the sample size is 2304 x 16bit, or 4608 x 8bit, and the len size is 512 x 8bit and, since 512 divides evenly in 4608, we will just rely on that
	for (int i = 0; i < shortLen; i++ )
	{
		val = buff[buff_pos + i] + 0x7FFF ;
		val *= f_muted ? 0 : (output_volume / MAX_VOL);
		val >>= 1;
		data[(i << 1)] = val & 0xff;
		data[(i << 1) + 1] = (val >> 8) & 0xff;	
	}
	buff_pos += shortLen;
	if (buff_pos == MINIMP3_MAX_SAMPLES_PER_FRAME * 2)
	{
		//ESP_LOGI("main", "buff swap");
		if (buff_num)
		{
			buff_num = 0;
		}
		else
		{
			buff_num = 1;
		}

		buff_pos = 0;		
		vTaskResume(mp3TaskHandle);	
	}

	return len;

}

void vI2SOutput( void * pvParameters )
{	
	init_audio_out();
	short * buff = buff0;

	ESP_LOGI("main", "Starting I2S task");

	while (1)
	{
		//sd card detect
		if (!gpio_get_level(PIN_NUM_CD))
		{
			playing = false;
		}

		if (playing && i2s_output && has_started)
		{
			
			buff = buff0;
			if (buff_num)
			{
				buff = buff1;
			} 

			//ESP_LOGI("main", "Triggering next task");
			vTaskResume(mp3TaskHandle);	

			//ESP_LOGI("main", "Outputting I2S");
			output->set_volume(f_muted ? 0 : output_volume / MAX_VOL);
			

			if (!f_change_file) //one last check. This really should be made atomic
			{
				output->write(buff, MINIMP3_MAX_SAMPLES_PER_FRAME);
			}


			if (buff_num)
			{
				buff_num = 0;
			}
			else
			{
				buff_num = 1;
			}


		}
		else
		{
			//Keep the WDT happy
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}
}

void vButtonInput( void * pvParameters )
{
	init_button_input();
	while(1)
	{
		handle_button_input();
		vTaskDelay(pdMS_TO_TICKS(25));
	}
	
}

void vMp3Decode( void * pvParameters )
{
	uint16_t sample_rate = 0;

	int sample_len = 0;
	short * fillBuff;
	uint8_t pass = 0;


	FileNavi::goto_first_mp3();
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);


	ESP_LOGI("main", "starting mp3 decode task");
	
	MP3Player* player;

	while (1)
	{

		f_change_file = false;
		has_started = false;


		if (nyan_mode)
		{
			printf("loading nyan mp3\n");
			vTaskDelay(pdMS_TO_TICKS(100)); //changing files does not work correctly without a little delay
			player = new MP3Player(NYAN_MP3_PATH, 0);
		}
		else
		{
			printf("loading file %s\n", FileNavi::get_current_full_name());
			vTaskDelay(pdMS_TO_TICKS(100)); //changing files does not work correctly without a little delay
			player = new MP3Player(FileNavi::get_current_full_name(), 0);
		}



		while(1)
		{

			if (get_a2dp_src_tx_err())
			{
				#ifdef DEBUG
					char * tasks = (char *)malloc(sizeof(char) * 1000) ;
					vTaskList(tasks);
					printf("Tasks: %s\n", tasks);
					free(tasks);
					playing = false;
				#endif		
			}
			//Only pause after we have started the output (decoding has actually happened)
			if (player->is_output_started)
			{
				vTaskSuspend( mp3TaskHandle );
			}

			pass = 0;
			fillBuff = buff1;

			if (buff_num) 
			{
				fillBuff = buff0;
			}

			if (f_change_file)
			{
				f_muted = true; //just to avoid the skipping sound when changing songs.				
				printf("Changing file\n");
				// vTaskDelay(pdMS_TO_TICKS(100));	
				break;
			}

			if (sample_len > 0 && has_started)
			{
				update_front_display(fillBuff);
			}
			
			while(pass < 2)
			{
				player->decodeSample(fillBuff + (MINIMP3_MAX_SAMPLES_PER_FRAME * pass), &sample_len);
				total_samples += sample_len;

				if (sample_len > 0 && !has_started) //make sure this only happens once
				{
		
					total_samples = 0;
					has_started = true;
					ESP_LOGI("main", "i2s output starting. Sample rate: %d, Channels: %d", player->info.hz, player->info.channels);
					output->start(player->info.hz);
					current_sample_rate = player->info.hz;
				
					ESP_LOGI("main", "i2s output started");

					if (sample_rate != current_sample_rate)
					{
						sample_rate = current_sample_rate;

						update_bt_sample_rate(bt_control);						
					}
					f_muted = false;
				}
				pass++;
			}



			//sample_len of 0 means we reached the end of the file so we can go to the next one
			if (!player->is_output_started && has_started)
			{
				if (!nyan_mode && !repeat_mode)
				{
					ESP_LOGI("main", "Done with current song. Going to next file.");
					FileNavi::goto_next_mp3();
					//For display
					full_path = FileNavi::get_current_full_name();
					full_path_len = strlen(full_path);
					//end for display				
				}

				f_change_file = true;

			}

		}


		// printf("Stopping I2S and MP3 Decoder\n");
		// vTaskDelay(pdMS_TO_TICKS(100));
		output->stop();
		
		player->dispose();
		
	}

	printf("done with decode task\n");
}


void vOLEDDisplayUpdate(void * pvParameters)
{


	char header[45];
	char str_buff[MAX_CHARS];
	int scroll_hold = 0;
	int scroll_pos = 0;


	init_display();

	ssd1306_clear_screen(&oled_display, false);


	//The check for discovery mode only happens once because the mcu is reset when switching between modes.
	if (bt_discovery_mode)
	{
		sprintf(header, "  Discovering. Current: %s", bt_sink_name);
		while (1)
		{
			
			//Scroll the text "Discovering. Current: <name>"
			
			scroll_pos++;

			if (scroll_pos > strlen(header) - MAX_CHARS)	
			{
				//use this to hold the scrolling of the file name for a moment before returning to the beginning
				if (scroll_hold < SCROLL_HOLD && scroll_pos > 0)
				{
					scroll_pos--;			
					scroll_hold++;
				}
				else 
				{
					scroll_hold = 0;
					scroll_pos = 0;
				}
			}

			scroll_text(header, strlen(header), MAX_CHARS, scroll_pos, str_buff);		
			ssd1306_display_text(&oled_display, 0, str_buff, 16, false);

			oled_display_discovery_mode(&oled_display);
			vTaskDelay(pdMS_TO_TICKS(OLED_DISP_INT));
		}
	}
	else
	{
		
		while (1)
		{

			if (nyan_mode)
			{
				ssd1306_display_text(&oled_display, 0, "  Nyan Cat MP3  ", 16, false);
			}
			else
			{
				if (!gpio_get_level(PIN_NUM_CD))
				{	
					sprintf(str_buff, "No SD Card     ");
				}	
				else
				{
					scroll_pos++;

					if (scroll_pos > full_path_len - MAX_CHARS)	
					{
						//use this to hold the scrolling of the file name for a moment before returning to the beginning
						if (scroll_hold < SCROLL_HOLD && scroll_pos > 0)
						{
							scroll_pos--;			
							scroll_hold++;
						}
						else 
						{
							scroll_hold = 0;
							scroll_pos = 0;
						}
					}

					scroll_text(full_path, full_path_len, MAX_CHARS, scroll_pos, str_buff);	
				}

				ssd1306_display_text(&oled_display, 0, str_buff, 16, false);
			}

			oled_display_normal_mode(&oled_display);




			vTaskDelay(pdMS_TO_TICKS(OLED_DISP_INT));

			
		}
	}
}



extern "C" void app_main(void)
{
	uint8_t f_discovery_mode = 0;


	initializing = true;
	bt_enabled = true;
	bt_discovery_mode = false;

	BT_a2dp bt(bt_app_a2d_data_cb);
	bt_control = &bt;
	nvs_get_u8(bt_control->get_nvs_handle(), bt_discovery_mode_flag_key, &f_discovery_mode);

	printf("Discovery Mode: %d\n", f_discovery_mode);

	if (f_discovery_mode)
	{
		//start in discovery mode
		bt_discovery_mode = true;

		//Clear the mode in flash right away
		nvs_set_u8(bt_control->get_nvs_handle(), bt_discovery_mode_flag_key, 0);
		nvs_commit(bt_control->get_nvs_handle());		
	}

	xTaskCreatePinnedToCore(vButtonInput, "BUTTON_INPUT", 2500, NULL, 1, &buttonsTaskHandle, 1);
	xTaskCreatePinnedToCore(vOLEDDisplayUpdate, "OLED_DISPLAY", 2500, NULL, 1, &oledTaskHandle, 1);

	init_bt_device_info();

	init_rgb_led_spi(SPI_DMA_CH1);

	if (bt_discovery_mode)
	{
		bt.discover_sinks(esp_a2d_found_devices_cb);
	}
	else
	{		

		init_adc();

		SDCard* sdc = new SDCard();

		if (!gpio_get_level(PIN_NUM_CD))
		{
			initializing = false;
			return;	
		}
		
		sdc->init();
		sd_initialized = true;

		init_spiffs();
		esp_vfs_spiffs_register(&spiffs_cfg);

		init_colors(pixel_colors);
		
		xTaskCreatePinnedToCore(vI2SOutput, "I2S_OUTPUT", 2500, NULL, configMAX_PRIORITIES - 5, &i2sTaskHandle, 0);
		xTaskCreatePinnedToCore(vMp3Decode, "MP3_CORE", 17500, NULL, 10, &mp3TaskHandle, 1);	

		//We can skip BT startup if the sink device is not selected because the mcu will be reset before one is selected
		if (bt_enabled)
		{
			esp_bd_addr_t addr;
			memcpy(addr, bt_sink_addr, 6);
			bt.connect_bluetooth(addr);
		}
	}
	
	initializing = false;
#ifdef DEBUG
	while(1)
	{
		//HEAP INFO DEBUG CODE





			size_t heap_block;
			UBaseType_t buttons = uxTaskGetStackHighWaterMark(buttonsTaskHandle);
			UBaseType_t oled = uxTaskGetStackHighWaterMark(oledTaskHandle);
			UBaseType_t i2s = uxTaskGetStackHighWaterMark(i2sTaskHandle);
			UBaseType_t mp3 = uxTaskGetStackHighWaterMark(mp3TaskHandle);
			heap_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
			printf("Bytes Left -> buttons: %u, oled: %u, i2s: %u, mp3: %u, total heap bytes free bytes: %d, largest block: %d\n", buttons, oled, i2s, mp3, xPortGetFreeHeapSize(), heap_block );
			vTaskDelay(pdMS_TO_TICKS(1000));
	}
#endif
}
