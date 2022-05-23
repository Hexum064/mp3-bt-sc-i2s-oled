#define FIXED_POINT 16

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
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include "ws2812.h"
#include <math.h>

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

#define FRONT_DISPLAY_PIN GPIO_NUM_27

#define OLED_DISP_INT 250

#define RGB_LED_COUNT 536

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
//#define DEBUG

#define FFT_SAMPLE_SIZE_POWER 11
#define FFT_SAMPLE_SIZE (1 << FFT_SAMPLE_SIZE_POWER) 
#define FFT_BINS 8

#define SCROLL_HOLD 4

TaskHandle_t mp3TaskHandle = NULL;


short buff0[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
short buff1[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
uint8_t buff_num = 0;
float output_volume = MAX_VOL / 2;
float prev_volume;
bool playing = false;
bool i2s_output = false;
bool bt_enabled = true;
bool bt_discovery_mode = false;
bool has_started = false;
bool f_change_file = false;
bool f_muted = false;
Output *output = NULL;
int buff_pos = 0;
i2s_pin_config_t i2s_speaker_pins;
int total_samples = 0;
uint16_t current_sample_rate = 0;
uint64_t mp3_run_time = 0;
char * full_path;
uint16_t full_path_len = 0;
rgbVal pixel_colors[8];
BT_a2db *bt_control;
SSD1306_t oled_display;

	 kiss_fft_cpx l_spectrum[FFT_SAMPLE_SIZE + 1];
	 kiss_fft_cpx r_spectrum[FFT_SAMPLE_SIZE + 1];
	 kiss_fftr_cfg st = kiss_fftr_alloc(FFT_SAMPLE_SIZE, 0, NULL, NULL);

	 int16_t l_channel[FFT_SAMPLE_SIZE];
	 int16_t r_channel[FFT_SAMPLE_SIZE];
	 rgbVal pixels[RGB_LED_COUNT];

uint64_t max_v =0;
#define VAL_OFFSET 1024

inline uint16_t get_bar_mag(int64_t real, int64_t imaginary)
{
	// uint32_t val = (uint32_t)sqrt((real * real) + (imaginary * imaginary));
	uint32_t val = abs(real) + abs(imaginary);

	// if (val > max_v)
	// {
	// 	printf("real: %lld, img: %lld, %llu\n", real, imaginary, val);
	// 	max_v = val;
	// }

	// if (val <= 1 + VAL_OFFSET)
	// {
	// 	return 0x00;
	// }
	// else if (val <= 2 + VAL_OFFSET)
	// {
	// 	return 0x01;
	// }
	// else if (val <= 4 + VAL_OFFSET)
	// {
	// 	return 0x03;
	// }
	// else if (val <= 8 + VAL_OFFSET)
	// {
	// 	return 0x07;
	// }
	// else if (val <= 16 + VAL_OFFSET)
	// {
	// 	return 0x0F;
	// }
	// else if (val <= 32 + VAL_OFFSET)
	// {
	// 	return 0x1F;
	// }
	// else if (val <= 64 + VAL_OFFSET)
	// {
	// 	return 0x3F;	
	// }
	// else if (val <= 128 + VAL_OFFSET)
	// {
	// 	return 0x7F;
	// }
	// else if (val <= 256 + VAL_OFFSET)
	// {
	// 	return 0xFF;
	// }
	// else if (val <= 512 + VAL_OFFSET)
	// {
	// 	return 0x1FF;
	// }
	// else if (val <= 1024 + VAL_OFFSET)
	// {
	// 	return 0x3FF;
	// }
	// else if (val <= 2048 + VAL_OFFSET)
	// {
	// 	return 0x7FF;
	// }
	// else if (val <= 4096 + VAL_OFFSET)
	// {
	// 	return 0xFFF;
	// }
	// else if (val <= 8192 + VAL_OFFSET)
	// {
	// 	return 0x1FFF;
	// }
	// else if (val <= 16384 + VAL_OFFSET)
	// {
	// 	return 0x3FFF;
	// }
	// else if (val <= 32768 + VAL_OFFSET)
	// {
	// 	return 0x7FFF;
	// }
	// else
	// {
	// 	return 0xFFFF;
	// }				

	if (val <= VAL_OFFSET)
	{
		return 8;
	}
	else if (val <= VAL_OFFSET * 2)
	{
		return 7;
	}
	else if (val <= VAL_OFFSET * 4)
	{
		return 6;
	}
	else if (val <= VAL_OFFSET * 6)
	{
		return 5;
	}
	else if (val <= VAL_OFFSET * 8)
	{
		return 4;
	}
	else if (val <= VAL_OFFSET * 10)
	{
		return 3;
	}
	else if (val <= VAL_OFFSET * 12)
	{
		return 2;
	}
	else if (val <= VAL_OFFSET * 14)
	{
		return 1;
	} 
	else
	{
		return 0;
	}

	// if (val <= VAL_OFFSET)
	// {
	// 	//return 0x01;
	// 	return 1;
	// }
	// else if (val <= VAL_OFFSET * 2)
	// {
	// 	//return 0x03;
	// 	return 2;
	// }
	// else if (val <= VAL_OFFSET * 3)
	// {
	// 	// return 0x07;
	// 	return 3;
	// }
	// else if (val <= VAL_OFFSET * 4)
	// {
	// 	// return 0x0F;
	// 	return 4;
	// }
	// else if (val <= VAL_OFFSET * 5)
	// {
	// 	// return 0x1F;
	// 	return 5;
	// }
	// else if (val <= VAL_OFFSET * 6)
	// {
	// 	// return 0x3F;	
	// 	return 6;
	// }
	// else if (val <= VAL_OFFSET * 7)
	// {
	// 	// return 0x7F;
	// 	return 7;
	// }
	// else if (val <= VAL_OFFSET * 8)
	// {
	// 	// return 0xFF;
	// 	return 8;
	// }
	// else if (val <= VAL_OFFSET * 9)
	// {
	// 	return 0x1FF;
	// }
	// else if (val <= VAL_OFFSET * 10)
	// {
	// 	return 0x3FF;
	// }
	// else if (val <= VAL_OFFSET * 1)
	// {
	// 	return 0x7FF;
	// }
	// else if (val <= VAL_OFFSET * 12)
	// {
	// 	return 0xFFF;
	// }
	// else if (val <= VAL_OFFSET * 13)
	// {
	// 	return 0x1FFF;
	// }
	// else if (val <= VAL_OFFSET * 14)
	// {
	// 	return 0x3FFF;
	// }
	// else if (val <= VAL_OFFSET * 15)
	// {
	// 	return 0x7FFF;
	// }
	// else
	// {
	// 	return 0xFFFF;
	// }											
}


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
	//For display
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);
	//end for display	
	f_change_file = true;

}

void play_previous_song()
{
	//Don't want to change songs in this mode 
	if (bt_discovery_mode)
	{
		return;
	}

	ESP_LOGI("main", "Going to previous file.");
	//TODO: if runtime < n number of seconds, go to start of song (just don't call goto_prev_mp3), else go to previous song
	FileNavi::goto_prev_mp3();
	//For display
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);
	//end for display	
	f_change_file = true;

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
			val *= f_muted ? 0 : (output_volume / MAX_VOL) / 2;
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

			//ESP_LOGI("main", "Outputting I2S");
			output->set_volume(f_muted ? 0 : output_volume / MAX_VOL);
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

void init_colors(rgbVal * pixel_colors)
{
	pixel_colors[0].r = 37;
	pixel_colors[0].g = 0;
	pixel_colors[0].b = 32;

	pixel_colors[1].r = 18;
	pixel_colors[1].g = 0;
	pixel_colors[1].b = 48;

	pixel_colors[2].r = 0;
	pixel_colors[2].g = 0;
	pixel_colors[2].b = 96;

	pixel_colors[3].r = 0;
	pixel_colors[3].g = 24;
	pixel_colors[3].b = 48;

	pixel_colors[4].r = 0;
	pixel_colors[4].g = 64;
	pixel_colors[4].b = 0;

	pixel_colors[5].r = 32;
	pixel_colors[5].g = 32;
	pixel_colors[5].b = 0;

	pixel_colors[6].r = 64;
	pixel_colors[6].g = 32;
	pixel_colors[6].b = 0;

	pixel_colors[7].r = 64;
	pixel_colors[7].g = 0;
	pixel_colors[7].b = 0;
}

void display_fft(short * buff)
{
	static int i;
	static uint8_t bin_i = 0;
	static uint8_t row = 0;	



	static uint32_t l_bins[FFT_BINS];
	static uint32_t r_bins[FFT_BINS];
	static uint16_t l_val = 0;
	static uint16_t r_val = 0;


	static int32_t l_bins_r[FFT_BINS];
	static int32_t r_bins_r[FFT_BINS];
	static int32_t l_bins_i[FFT_BINS];
	static int32_t r_bins_i[FFT_BINS];



	

//printf("Generatting FFT\n");
	for (i = 0; i < FFT_SAMPLE_SIZE; i++)
	{
		l_channel[i] = buff[i * 2];
		r_channel[i] = buff[(i * 2) + 1];
	}

	kiss_fftr(st, l_channel, l_spectrum);
	kiss_fftr(st, r_channel, r_spectrum);

	//Binning


	for (i = 0 ; i < FFT_BINS ; i++)			
	{
		l_bins_r[i] = 0;
		l_bins_i[i] = 0;
		r_bins_r[i] = 0;
		r_bins_i[i] = 0;
	}

	//First have are real numbers we will use for max
	for(i = 0; i < FFT_SAMPLE_SIZE >> 2; i++)
	{

		//bins
		if (i <= 3 * 2)
		{		
			l_bins_r[0] += l_spectrum[i].r;
			l_bins_i[0] += l_spectrum[i].i;
			r_bins_r[0] += r_spectrum[i].r;
			r_bins_i[0] += r_spectrum[i].i;
		}
		else if (i <= 6 * 2)
		{
			l_bins_r[1] += l_spectrum[i].r;
			l_bins_i[1] += l_spectrum[i].i;
			r_bins_r[1] += r_spectrum[i].r;
			r_bins_i[1] += r_spectrum[i].i;					
		}
		else if (i <= 13 * 2)
		{
			l_bins_r[2] += l_spectrum[i].r;
			l_bins_i[2] += l_spectrum[i].i;
			r_bins_r[2] += r_spectrum[i].r;
			r_bins_i[2] += r_spectrum[i].i;							
		}
		else if (i <= 27 * 2)
		{
			l_bins_r[3] += l_spectrum[i].r;
			l_bins_i[3] += l_spectrum[i].i;
			r_bins_r[3] += r_spectrum[i].r;
			r_bins_i[3] += r_spectrum[i].i;							
		}
		else if (i <= 55 * 2)
		{
			l_bins_r[4] += l_spectrum[i].r;
			l_bins_i[4] += l_spectrum[i].i;
			r_bins_r[4] += r_spectrum[i].r;
			r_bins_i[4] += r_spectrum[i].i;							
		}
		else if (i <= 112 * 2)
		{
			l_bins_r[5] += l_spectrum[i].r;
			l_bins_i[5] += l_spectrum[i].i;
			r_bins_r[5] += r_spectrum[i].r;
			r_bins_i[5] += r_spectrum[i].i;							
		}
		else if (i <= 229 * 2)
		{
			l_bins_r[6] += l_spectrum[i].r;
			l_bins_i[6] += l_spectrum[i].i;		
			r_bins_r[6] += r_spectrum[i].r;
			r_bins_i[6] += r_spectrum[i].i;					
		}
		else 
		{
			l_bins_r[7] += l_spectrum[i].r;
			l_bins_i[7] += l_spectrum[i].i;
			r_bins_r[7] += r_spectrum[i].r;
			r_bins_i[7] += r_spectrum[i].i;							
		}


	}

	for (i = 0; i < FFT_BINS; i++)
	{
		l_bins_r[i] = get_bar_mag(l_bins_r[i], l_bins_i[i]);
		r_bins_r[i] = get_bar_mag(r_bins_r[i], r_bins_i[i]);
	}



	//printf("Drawing FFT\n");
	//Draw the display
	//this will normally be RGB_LED_COUNT but for now, it's just 81 for now and both halves are done at the same time
	for (i = 0 ; i < 81 ; i++)			
	{
		bin_i = i % 9;
		row = i / 9;
		
		pixels[i].r = 0;
		pixels[i].g = 0;
		pixels[i].b = 0;	

		pixels[i + 81].r = 0;
		pixels[i + 81].g = 0;
		pixels[i + 81].b = 0;	

		if (bin_i) 
		{
			if (row >= l_bins_r[bin_i - 1])
			{
				pixels[i].r = pixel_colors[bin_i - 1].r;
				pixels[i].g = pixel_colors[bin_i - 1].g;
				pixels[i].b = pixel_colors[bin_i - 1].b;	
				
			}				

			if (row >= r_bins_r[bin_i - 1])
			{
				pixels[i + 81].r = pixel_colors[bin_i - 1].r;
				pixels[i + 81].g = pixel_colors[bin_i - 1].g;
				pixels[i + 81].b = pixel_colors[bin_i - 1].b;					
				
			}					

		}
		
	}

	ws2812_setColors(RGB_LED_COUNT, pixels);
}

void vMp3Decode( void * pvParameters )
{





	uint16_t sample_rate = 0;
	uint32_t sample_count = 0;
	int sample_len = 0;
	short * fillBuff;

	FileNavi::goto_first_mp3();
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);


	ESP_LOGI("main", "starting mp3 decode task");
	
	MP3Player* player;

	while (1)
	{

		f_change_file = false;
		has_started = false;


		printf("loading file %s\n", FileNavi::get_current_full_name());
		vTaskDelay(pdMS_TO_TICKS(100)); //changing files does not work correctly without a little delay
		player = new MP3Player(FileNavi::get_current_full_name());



		while(1)
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

			if (f_change_file)
			{
				f_muted = true; //just to avoid the skipping sound when changing songs.				
				printf("Changing file\n");
				// vTaskDelay(pdMS_TO_TICKS(100));	
				break;
			}


			
			player->decodeSample(fillBuff, &sample_len);
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
				f_muted = false;
			}

			player->decodeSample(fillBuff + MINIMP3_MAX_SAMPLES_PER_FRAME, &sample_len); 
			total_samples += sample_len;

			if (sample_len > 0)
			{
				display_fft(fillBuff);
			}

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
				f_muted = false;
			}


			//sample_len of 0 means we reached the end of the file so we can go to the next one
			if (!player->is_output_started && has_started)
			{
		
				ESP_LOGI("main", "Done with current song. Going to next file.");
				FileNavi::goto_next_mp3();
				//For display
				full_path = FileNavi::get_current_full_name();
				full_path_len = strlen(full_path);
				//end for display				
				f_change_file = true;

			}

		}



		// printf("Disposing player\n");
		// vTaskDelay(pdMS_TO_TICKS(250));
		player->dispose();
		
	}

	printf("done with decode task\n");
}

void vOLEDDisplayUpdate(void * pvParameters)
{
	int scroll_hold = 0;
	int scroll_pos = 0;
	int totalSeconds = 0;
	int minutes = totalSeconds / 60;
	int seconds = totalSeconds % 60;
	char disp_buff[24];	
	char file_name_buff[MAX_CHARS];

	//uint8_t img[8*16];

	init_display(); //eventually move to display task

	vTaskDelay(pdMS_TO_TICKS(3000));

	ssd1306_clear_line(&oled_display, 4, false);
	ssd1306_clear_line(&oled_display, 5, false);
	ssd1306_clear_line(&oled_display, 6, false);
	ssd1306_clear_line(&oled_display, 7, false);



	while (1)
	{
		if (current_sample_rate > 0)
			totalSeconds = (int)(total_samples / current_sample_rate);
		//printf("samples: %d, rate: %d, seconds %d\n", total_samples, current_sample_rate, (int)(total_samples / current_sample_rate));
		minutes = totalSeconds / 60;
		seconds = totalSeconds % 60;

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
		// ssd1306_display_text(&oled_display, 4, "<BT SINK NAME>", strlen("<BT SINK NAME>"), false);

		// ssd1306_display_text(&oled_display, 5, "<DISPLAY MODE>", strlen("<DISPLAY MODE>"), false);

		// ssd1306_display_text(&oled_display, 6, "<BATTERY?>", strlen("<BATTERY?>"), false);



		vTaskDelay(pdMS_TO_TICKS(OLED_DISP_INT));

		//printf("Free heep: %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
	}
}



void vFFT_FrontDisplay(void * pvParameters)
{
	ws2812_init(FRONT_DISPLAY_PIN);
	rgbVal pixels[RGB_LED_COUNT];
	rgbVal pixel_colors[8];

	init_colors(pixel_colors);



	int i;
	uint8_t bin_i = 0;
	uint8_t row = 0;	

	int16_t l_channel[FFT_SAMPLE_SIZE];
	int16_t r_channel[FFT_SAMPLE_SIZE];
	uint32_t l_bins[FFT_BINS];
	uint32_t r_bins[FFT_BINS];
	uint16_t l_val = 0;
	uint16_t r_val = 0;


	int32_t l_bins_r[FFT_BINS];
	int32_t r_bins_r[FFT_BINS];
	int32_t l_bins_i[FFT_BINS];
	int32_t r_bins_i[FFT_BINS];

	kiss_fft_cpx l_spectrum[FFT_SAMPLE_SIZE + 1];
	kiss_fft_cpx r_spectrum[FFT_SAMPLE_SIZE + 1];

	// int nfft = FFT_SAMPLE_SIZE ;

	short * buff = buff0;
	// the various buffers  

	// kiss_fft_cpx* l_spectrum = (kiss_fft_cpx*) calloc(nfft + 1, sizeof(kiss_fft_cpx));
	// kiss_fft_cpx* r_spectrum = (kiss_fft_cpx*) calloc(nfft + 1, sizeof(kiss_fft_cpx));

	kiss_fftr_cfg st = kiss_fftr_alloc(FFT_SAMPLE_SIZE, 0, NULL, NULL);
	// Create the FFT config structure


	while(1)
	{

	




		if (playing)
		{
			if (buff_num)
			{
				buff = buff1;
			} 

			//printf("Generatting FFT\n");
			for (i = 0; i < FFT_SAMPLE_SIZE; i++)
			{
				l_channel[i] = buff[i * 2];
				r_channel[i] = buff[(i * 2) + 1];
			}

			kiss_fftr(st, l_channel, l_spectrum);
			kiss_fftr(st, r_channel, r_spectrum);

			//Binning


			for (i = 0 ; i < FFT_BINS ; i++)			
			{
				l_bins_r[i] = 0;
				l_bins_i[i] = 0;
				r_bins_r[i] = 0;
				r_bins_i[i] = 0;
			}

			//First have are real numbers we will use for max
			for(i = 0; i < FFT_SAMPLE_SIZE >> 2; i++)
			{

				//bins
				if (i <= 3 * 2)
				{		
					l_bins_r[0] += l_spectrum[i].r;
					l_bins_i[0] += l_spectrum[i].i;
					r_bins_r[0] += r_spectrum[i].r;
					r_bins_i[0] += r_spectrum[i].i;
				}
				else if (i <= 6 * 2)
				{
					l_bins_r[1] += l_spectrum[i].r;
					l_bins_i[1] += l_spectrum[i].i;
					r_bins_r[1] += r_spectrum[i].r;
					r_bins_i[1] += r_spectrum[i].i;					
				}
				else if (i <= 13 * 2)
				{
					l_bins_r[2] += l_spectrum[i].r;
					l_bins_i[2] += l_spectrum[i].i;
					r_bins_r[2] += r_spectrum[i].r;
					r_bins_i[2] += r_spectrum[i].i;							
				}
				else if (i <= 27 * 2)
				{
					l_bins_r[3] += l_spectrum[i].r;
					l_bins_i[3] += l_spectrum[i].i;
					r_bins_r[3] += r_spectrum[i].r;
					r_bins_i[3] += r_spectrum[i].i;							
				}
				else if (i <= 55 * 2)
				{
					l_bins_r[4] += l_spectrum[i].r;
					l_bins_i[4] += l_spectrum[i].i;
					r_bins_r[4] += r_spectrum[i].r;
					r_bins_i[4] += r_spectrum[i].i;							
				}
				else if (i <= 112 * 2)
				{
					l_bins_r[5] += l_spectrum[i].r;
					l_bins_i[5] += l_spectrum[i].i;
					r_bins_r[5] += r_spectrum[i].r;
					r_bins_i[5] += r_spectrum[i].i;							
				}
				else if (i <= 229 * 2)
				{
					l_bins_r[6] += l_spectrum[i].r;
					l_bins_i[6] += l_spectrum[i].i;		
					r_bins_r[6] += r_spectrum[i].r;
					r_bins_i[6] += r_spectrum[i].i;					
				}
				else 
				{
					l_bins_r[7] += l_spectrum[i].r;
					l_bins_i[7] += l_spectrum[i].i;
					r_bins_r[7] += r_spectrum[i].r;
					r_bins_i[7] += r_spectrum[i].i;							
				}


			}

			for (i = 0; i < FFT_BINS; i++)
			{
				l_bins_r[i] = get_bar_mag(l_bins_r[i], l_bins_i[i]);
				r_bins_r[i] = get_bar_mag(r_bins_r[i], r_bins_i[i]);
			}



			//printf("Drawing FFT\n");
			//Draw the display
			//this will normally be RGB_LED_COUNT but for now, it's just 81 for now and both halves are done at the same time
			for (i = 0 ; i < 81 ; i++)			
			{
				bin_i = i % 9;
				row = i / 9;
				
				pixels[i].r = 0;
				pixels[i].g = 0;
				pixels[i].b = 0;	

				pixels[i + 81].r = 0;
				pixels[i + 81].g = 0;
				pixels[i + 81].b = 0;	

				if (bin_i) 
				{
					if (row >= l_bins_r[bin_i - 1])
					{
						pixels[i].r = pixel_colors[bin_i - 1].r;
						pixels[i].g = pixel_colors[bin_i - 1].g;
						pixels[i].b = pixel_colors[bin_i - 1].b;	
						
					}				

					if (row >= r_bins_r[bin_i - 1])
					{
						pixels[i + 81].r = pixel_colors[bin_i - 1].r;
						pixels[i + 81].g = pixel_colors[bin_i - 1].g;
						pixels[i + 81].b = pixel_colors[bin_i - 1].b;					
						
					}					



				}
				
			}




			ws2812_setColors(162, pixels);
		
		}
		
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}


void vFrontSideDisplay(void * pvParameters)
{
	//Using this as a test for now
	ws2812_init(FRONT_DISPLAY_PIN);
	rgbVal pixels[RGB_LED_COUNT];
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	while (1)
	{
		red += 4;
		if (red >= 64)
		{
			red = 0;
			green += 4;

			if (green >= 64)
			{
				green = 0;
				blue += 4;

				if (blue >= 64)
				{
					blue = 0;
				}
			}
		}

		for (uint16_t i = 0; i < RGB_LED_COUNT; i++)
		{
			
			pixels[i].r = red;
			pixels[i].g = green;
			pixels[i].b = blue;

		}
		ws2812_setColors(RGB_LED_COUNT, pixels);

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

void vBTSetup(void * pvParameters)
{
	esp_bd_addr_t addr = { 0x42, 0xfa, 0xbf, 0x75, 0xca, 0x26 };
	
	BT_a2db bt(bt_app_a2d_data_cb);
	bt_control = &bt;
	bt.connect_bluetooth(addr);
	
		//bt.discover_bluetooth(CONFIG_SPEAKER_NAME);

	while(1)
	{
		printf("Free heep: %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

extern "C" void app_main(void)
{
	//FOR TESTING: address 42:fa:bf:75:ca:26, name Q50

	// static uint8_t ucParameterToPass;

    
    // xTaskCreatePinnedToCore(vTaskCode, "BT_CORE", 32768, NULL, 1, NULL, 0);

	//i2s_output = true;

	ws2812_init(FRONT_DISPLAY_PIN);

	SDCard* sdc = new SDCard();
	sdc->init();


	init_colors(pixel_colors);


	//xTaskCreatePinnedToCore(vBTSetup, "BG_SETUP", 1024 * 20, NULL, configMAX_PRIORITIES - 5, NULL, 0);
	xTaskCreatePinnedToCore(vOLEDDisplayUpdate, "OLED_DISPLAY", 1024*2, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(vI2SOutput, "I2S_OUTPUT", 1024*2, NULL, configMAX_PRIORITIES - 5, NULL, 1);
	xTaskCreatePinnedToCore(vButtonInput, "BUTTON_INPUT", 1024*2, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(vMp3Decode, "MP3_CORE", 1024*17, NULL, 10, &mp3TaskHandle, 1);
	//xTaskCreate(vFFT_FrontDisplay, "FFT_and_FRONT", 1024*32, NULL, 2, NULL);
	//xTaskCreate(vFrontSideDisplay, "FRONT_SIDES", 1024*32, NULL, 2, NULL);


	esp_bd_addr_t addr = { 0x42, 0xfa, 0xbf, 0x75, 0xca, 0x26 };
	
	BT_a2db bt(bt_app_a2d_data_cb);
	bt_control = &bt;
	bt.connect_bluetooth(addr);
	
		//bt.discover_bluetooth(CONFIG_SPEAKER_NAME);


	
}
