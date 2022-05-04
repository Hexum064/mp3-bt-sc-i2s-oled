#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include "SDCard.h"
#include "esp_log.h"
#include "BT_a2db.h"
#include "MP3Player.h"
#include "FileNavi.h"
#include "Fifo.h"
#include <driver/gpio.h>
#include "I2SOutput.h"
#include "ssd1306_i2c.h"

extern "C" {
	#include "btc_av.h"
}

#define CONFIG_SPEAKER_NAME "Q50"
#define BYTE_SAMPLES_PER_FRAME (MINIMP3_MAX_SAMPLES_PER_FRAME * 2) //MINIMP3_MAX_SAMPLES_PER_FRAME is number of short samples so this is for total bytes


// speaker settings - if using I2S
#define I2S_SPEAKER_SERIAL_CLOCK GPIO_NUM_4
#define I2S_SPEAKER_LEFT_RIGHT_CLOCK GPIO_NUM_5
#define I2S_SPEAKER_SERIAL_DATA GPIO_NUM_18

#define BUTTON_BIT_0 GPIO_NUM_2
#define BUTTON_BIT_1 GPIO_NUM_16
#define BUTTON_BIT_2 GPIO_NUM_17

#define DISPLAY_SDA	GPIO_NUM_21
#define DISPLAY_SCL GPIO_NUM_22
#define DISPLAY_RST GPIO_NUM_NC

#define OLED_DISP_INT 250

#define SHORT_BUTTON_PUSH 50000ULL
#define LONG_BUTTON_HOLD 750000ULL

#define VOL_UP_BUTTON 7
#define VOL_DOWN_BUTTON 6
#define DISPLAY_BUTTON 5
#define OUTPUT_BUTTON 4
#define NEXT_BUTTON 3
#define PAUSE_BUTTON 2
#define PREVIOUS_BUTTON 1

#define MAX_VOL 4096
#define VOL_STEP (MAX_VOL / 16)
#define DEBUG

TaskHandle_t mp3TaskHandle = NULL;


short buff0[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
short buff1[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
uint8_t buff_num = 0;
float output_volume = MAX_VOL / 2;
bool playing = false;
bool i2s_output = false;
bool bt_enabled = true;
bool bt_discovery_mode = false;
bool has_started = false;
bool f_change_file = false;
Output *output = NULL;
int buff_pos = 0;
i2s_pin_config_t i2s_speaker_pins;
int total_samples = 0;
uint16_t current_sample_rate = 0;
uint64_t mp3_run_time = 0;
char * full_path;
uint16_t full_path_len = 0;

BT_a2db *bt_control;
SSD1306_t oled_display;

// static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);

// BT_a2db bt(bt_app_a2d_data_cb);

void init_button_input()
{
	//Button input is expected to be a multipled 7 to 3 signal where the 3 is a binary number from 0 to 7. 0 represents no buttons and 1 - 7 represent button 1 through 7.
	gpio_config_t io_conf = {
		.pin_bit_mask = ((1ULL << BUTTON_BIT_0) | (1ULL << BUTTON_BIT_1) | (1ULL << BUTTON_BIT_2)),
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

	// #if CONFIG_FLIP
	// 	oled_display._flip = true;
	// 	ESP_LOGW("main", "Flip upside down");
	// #endif

	ESP_LOGI("main", "Panel is 128x64");
	ssd1306_init(&oled_display, 128, 64);
	ssd1306_clear_screen(&oled_display, false);
	ssd1306_contrast(&oled_display, 0xff);




	ssd1306_display_text(&oled_display, 0, "BigFuckingBadge", 15, false);
	ssd1306_display_text(&oled_display, 1, "Fox's Random", 12, false);
	ssd1306_display_text(&oled_display, 2, " Access Memories", 16, false);
	ssd1306_display_text(&oled_display, 3, "DC30", 4, false);

}

void toggle_play_pause()
{
	//Don't want to play or pause in this mode 
	if (bt_discovery_mode)
	{
		return;
	}

	playing = !playing;



	printf(playing ? "Playing\n" : "Paused\n");
}

void toggle_output()
{
	if (bt_enabled)
	{
		i2s_output = !i2s_output;
	}
	else
	{
		i2s_output = true;
	}
}

void volume_up()
{
	if (output_volume < MAX_VOL)
	{
		output_volume += VOL_STEP;
		printf("Volume: %f\n", output_volume);
	}
}

void volume_down()
{
	if (output_volume > 0)
	{
		output_volume -= VOL_STEP;
		printf("Volume: %f\n", output_volume);
	}		
}

void play_next_song()
{
	//Don't want to change songs in this mode 
	if (bt_discovery_mode)
	{
		return;
	}

	ESP_LOGI("main", "Going to next file.");
	FileNavi::goto_next_mp3();
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);
	f_change_file = true;
	vTaskResume(mp3TaskHandle);	
}

void play_previous_song()
{
	//Don't want to change songs in this mode 
	if (bt_discovery_mode)
	{
		return;
	}

	ESP_LOGI("main", "Going to previous file.");
	//if runtime < n number of seconds, go to start of song (just don't call goto_prev_mp3), else go to previous song
	FileNavi::goto_prev_mp3();
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);
	f_change_file = true;
	//vTaskResume(mp3TaskHandle);	
}

void cycle_display()
{

}

void toggle_nayn_mode()
{

}

void toggle_bt_discovery_mode()
{
	bt_discovery_mode = !bt_discovery_mode;
}

void scroll_text(char * text, int str_len, int max_len, int start_pos, char * buffer)
{
	

	for (int i = 0; i < max_len; i++)
	{
		if (i + start_pos > str_len - 1)
			buffer[i] = ' ';
		else
			buffer[i] = text[i + start_pos];
	}

}

static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len)
{
	uint16_t val = 0;
	short * buff = buff0;
	int shortLen = len >> 1;
	//static uint16_t sample_rate = 0;

	if (!(playing) || i2s_output) // || !has_started)
	{
		//ESP_LOGI("Main", "waiting");
		memset(data, 0, len);
		return len;
	}



	if (buff_num)
	{
		buff = buff1;
	} 

	//We can do this because the sample size is 2304 x 16bit, or 4608 x 8bit, and the len size is 512 x 8bit and, since 512 divides evenly in 4608, we will just rely on that
	for (int i = 0; i < shortLen; i++ )
	{
			val = buff[buff_pos + i] + 0x7fff;
			val *= (output_volume / MAX_VOL) / 2;
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

		if (playing && i2s_output && has_started)
		{
			
			buff = buff0;
			if (buff_num)
			{
				buff = buff1;
			} 

			//ESP_LOGI("main", "Triggering next task");
			vTaskResume(mp3TaskHandle);	

			if (f_change_file)
			{
				continue;
			}
			//ESP_LOGI("main", "Outputting I2S");
			output->set_volume(output_volume / MAX_VOL);
			output->write(buff, MINIMP3_MAX_SAMPLES_PER_FRAME);

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
			//ESP_LOGI("main", "delay");
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}
}



void vButtonInput( void * pvParameters )
{
	uint8_t input = 0;
	uint8_t last_input = 0;
	bool held = false;
	uint64_t start_time = 0;

	init_button_input();

	for( ;; )
	{
		input = gpio_get_level(BUTTON_BIT_0) | (gpio_get_level(BUTTON_BIT_1) << 1) | (gpio_get_level(BUTTON_BIT_2) << 2);

		if (input) // if input is not 0
		{
			if (last_input != input && !(held))
			{
				last_input = input;
				start_time = esp_timer_get_time();
				printf("button down: %d\n", input);
			}
			else 
			{
				if (esp_timer_get_time() - start_time > LONG_BUTTON_HOLD && !(held)) //Long Hold
				{
					printf("button long down: %d\n", last_input);
					
					last_input = 0;
					start_time = 0;
					held = true;					
					
					switch(input)
					{
						case PREVIOUS_BUTTON:						
							break;
						case PAUSE_BUTTON:													
							break;
						case NEXT_BUTTON:						
							break;
						case OUTPUT_BUTTON:						
							break;
						case DISPLAY_BUTTON:					
							break;
						case VOL_DOWN_BUTTON:
							volume_down();			
							held = false;
							last_input = input;
							start_time = esp_timer_get_time();
							break;
						case VOL_UP_BUTTON:
							volume_up();
							held = false;
							last_input = input;
							start_time = esp_timer_get_time();
							break;							
					}


				}
			}
		}
		else
		{
			//check if we were pushing a button and clear everything

			if (last_input && esp_timer_get_time() - start_time > SHORT_BUTTON_PUSH) // if a button was being pushed
			{
				printf("button short up: %d\n", last_input);

				switch(last_input)
				{
					case PREVIOUS_BUTTON:
						play_previous_song();
						break;
					case PAUSE_BUTTON:
						toggle_play_pause();
						break;
					case NEXT_BUTTON:
						play_next_song();
						break;
					case OUTPUT_BUTTON:
						toggle_output();
						break;
					case DISPLAY_BUTTON:
						break;
					case VOL_DOWN_BUTTON:
						volume_down();						
						break;
					case VOL_UP_BUTTON:
						volume_up();
						break;							
				}

			}

			last_input = 0;
			start_time = 0;
			held = false;
		}

	

		

	
		vTaskDelay(pdMS_TO_TICKS(25));
	}
}



void vMp3Decode( void * pvParameters )
{
	static uint16_t sample_rate = 0;
	
	FileNavi::goto_first_mp3();
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);

	ESP_LOGI("main", "starting mp3 decode task");
	
	MP3Player* player;

	while (1)
	{
		has_started = false;
		
		int sample_len = 0;

		short * fillBuff;

		player = new MP3Player(FileNavi::get_current_full_name());
		
		//ESP_LOGI("main", "starting decode.");
		f_change_file = false;

		//for( ;; )
		while(!f_change_file)
		{



			//Only pause after we have started the output (decoding has actually happened)
			if (player->is_output_started)
			{
				vTaskSuspend( mp3TaskHandle );
			}

			

			fillBuff = buff1;

			if (buff_num) 
			{
				fillBuff = buff0;
			}


			//ESP_LOGI("main", "decoding samples.");
			player->decodeSample(fillBuff, &sample_len);
			total_samples = sample_len;

			if (sample_len > 0 && !has_started) //make sure this only happens once
			{


				has_started = true;
				ESP_LOGI("main", "i2s output starting. Sample rate: %d, Channels: %d", player->info.hz, player->info.channels);
				output->start(player->info.hz);
				current_sample_rate = player->info.hz;
				ESP_LOGI("main", "i2s output started");

				if (sample_rate != current_sample_rate)
				{
					sample_rate = current_sample_rate;

					if (bt_control->get_media_state() == APP_AV_MEDIA_STATE_STARTED)
					{
						esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
						vTaskDelay(pdMS_TO_TICKS(100));
						// printf("Media state: %d\n", bt_control->get_media_state());
						// printf("a2d state: %d\n", bt_control->get_a2d_state());
						set_freq(current_sample_rate);
						esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
					}
					else
					{
						set_freq(current_sample_rate);
					}
					
				}
			}

			player->decodeSample(fillBuff + MINIMP3_MAX_SAMPLES_PER_FRAME, &sample_len); 
			total_samples += sample_len;
			if (sample_len > 0 && !has_started) //make sure this only happens once
			{

				
				has_started = true;
				ESP_LOGI("main", "i2s output starting. Sample rate: %d, Channels: %d", player->info.hz, player->info.channels);
				output->start(player->info.hz);
				current_sample_rate = player->info.hz;
				ESP_LOGI("main", "i2s output started");

				if (sample_rate != current_sample_rate)
				{
					sample_rate = current_sample_rate;

					if (bt_control->get_media_state() == APP_AV_MEDIA_STATE_STARTED)
					{
						esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
						vTaskDelay(pdMS_TO_TICKS(100));
						set_freq(current_sample_rate);
						// printf("Media state: %d\n", bt_control->get_media_state());
						// printf("a2d state: %d\n", bt_control->get_a2d_state());
						esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
					}
					else
					{
						set_freq(current_sample_rate);
					}
					
				}
				
			}

			//sample_len of 0 means we reached the end of the file so we can go to the next one
			if (!player->is_output_started && has_started)
			{

				
				f_change_file = true;
				FileNavi::goto_next_mp3();
				has_started = false;

			}

		}


		output->stop();


		player->dispose();
		
	}
}

void vDisplayUpdate(void * pvParameters)
{
	int scroll_pos = 0;
	int totalSeconds = mp3_run_time / 1000000;
	int minutes = totalSeconds / 60;
	int seconds = totalSeconds % 60;
	char disp_buff[24];	
	char file_name_buff[MAX_CHARS];

	init_display(); //eventually move to display task

	vTaskDelay(pdMS_TO_TICKS(3000));


	while (1)
	{

		totalSeconds = mp3_run_time / 1000000;
		minutes = totalSeconds / 60;
		seconds = totalSeconds % 60;

		scroll_pos++;

		if (scroll_pos > full_path_len - MAX_CHARS)	
			scroll_pos = 0;

		scroll_text(full_path, full_path_len, MAX_CHARS, scroll_pos, file_name_buff);

		disp_buff[0] = '\0';

		if (playing)
		{
			sprintf(disp_buff, "%02d:%02d Playing", minutes, seconds);	
		}
		else
		{
			sprintf(disp_buff, "%02d:%02d Paused ", minutes, seconds);	
		}



		ssd1306_display_text(&oled_display, 0, file_name_buff, strlen(file_name_buff), false);
		ssd1306_display_text(&oled_display, 1, disp_buff, strlen(disp_buff), false);

		disp_buff[0] = '\0';

		if (i2s_output)
		{
			sprintf(disp_buff, "Vol:%02d%% Out:i2s ", (int)((output_volume / MAX_VOL) * 100));	
		}
		else
		{
			sprintf(disp_buff, "Vol:%02d%% Out:BT  ", (int)((output_volume / MAX_VOL) * 100));	
		}
		
		ssd1306_display_text(&oled_display, 2, disp_buff, strlen(disp_buff), false);

		disp_buff[0] = '\0';

		if (bt_control->get_a2d_state() != 5)
		{
			sprintf(disp_buff, "BT Connecting...");	
		}
		else
		{
			sprintf(disp_buff, "BT Connected    ");	
		}
		
		ssd1306_display_text(&oled_display, 3, disp_buff, strlen(disp_buff), false);
		ssd1306_display_text(&oled_display, 4, "<BT SINK NAME>", strlen("<BT SINK NAME>"), false);

		ssd1306_display_text(&oled_display, 5, "<DISPLAY MODE>", strlen("<DISPLAY MODE>"), false);

		ssd1306_display_text(&oled_display, 6, "<BATTERY?>", strlen("<BATTERY?>"), false);


		vTaskDelay(pdMS_TO_TICKS(OLED_DISP_INT));
	}
}


extern "C" void app_main(void)
{
	//FOR TESTING: address 42:fa:bf:75:ca:26, name Q50

	// static uint8_t ucParameterToPass;

    
    // xTaskCreatePinnedToCore(vTaskCode, "BT_CORE", 32768, NULL, 1, NULL, 0);

	//i2s_output = true;

	SDCard* sdc = new SDCard();
	sdc->init();





	xTaskCreatePinnedToCore(vDisplayUpdate, "OLED_DISPLAY", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(vI2SOutput, "I2S_OUTPUT", 2048, NULL, configMAX_PRIORITIES - 5, NULL, 1);
	xTaskCreatePinnedToCore(vButtonInput, "BUTTON_INPUT", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(vMp3Decode, "MP3_CORE", 1024*32, NULL, 10, &mp3TaskHandle, 1);


	esp_bd_addr_t addr = { 0x42, 0xfa, 0xbf, 0x75, 0xca, 0x26 };
	
	BT_a2db bt(bt_app_a2d_data_cb);
	bt_control = &bt;
	bt.connect_bluetooth(addr);

	//bt.discover_bluetooth(CONFIG_SPEAKER_NAME);

	
}
