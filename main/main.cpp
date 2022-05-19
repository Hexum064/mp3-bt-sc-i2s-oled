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

#define FRONT_DISPLAY_PIN GPIO_NUM_11

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

uint64_t max_v =0;
#define VAL_OFFSET 1024

uint16_t get_bar_mag(int64_t real, int64_t imaginary)
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
		return 0x01;
	}
	else if (val <= VAL_OFFSET * 2)
	{
		return 0x03;
	}
	else if (val <= VAL_OFFSET * 3)
	{
		return 0x07;
	}
	else if (val <= VAL_OFFSET * 4)
	{
		return 0x0F;
	}
	else if (val <= VAL_OFFSET * 5)
	{
		return 0x1F;
	}
	else if (val <= VAL_OFFSET * 6)
	{
		return 0x3F;	
	}
	else if (val <= VAL_OFFSET * 7)
	{
		return 0x7F;
	}
	else if (val <= VAL_OFFSET * 8)
	{
		return 0xFF;
	}
	else if (val <= VAL_OFFSET * 9)
	{
		return 0x1FF;
	}
	else if (val <= VAL_OFFSET * 10)
	{
		return 0x3FF;
	}
	else if (val <= VAL_OFFSET * 1)
	{
		return 0x7FF;
	}
	else if (val <= VAL_OFFSET * 12)
	{
		return 0xFFF;
	}
	else if (val <= VAL_OFFSET * 13)
	{
		return 0x1FFF;
	}
	else if (val <= VAL_OFFSET * 14)
	{
		return 0x3FFF;
	}
	else if (val <= VAL_OFFSET * 15)
	{
		return 0x7FFF;
	}
	else
	{
		return 0xFFFF;
	}											
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


int i, k, l = FFT_SAMPLE_SIZE >> 1, m = l / FFT_BINS;
int16_t l_channel[FFT_SAMPLE_SIZE];
int16_t r_channel[FFT_SAMPLE_SIZE];
uint32_t l_bins[FFT_BINS];
uint32_t r_bins[FFT_BINS];
short l_max;
short r_max;
int l_max_i = 0;
int r_max_i = 0;
uint8_t img_pg0[32];
uint8_t img_pg1[32];

int32_t l_bins_r[FFT_BINS];
int32_t r_bins_r[FFT_BINS];
int32_t l_bins_i[FFT_BINS];
int32_t r_bins_i[FFT_BINS];

int nfft = FFT_SAMPLE_SIZE ;
// the various buffers  
//int16_t* samples = (int16_t*) calloc(nfft, sizeof(int16_t));
kiss_fft_cpx* spectrum = (kiss_fft_cpx*) calloc(nfft + 1, sizeof(kiss_fft_cpx));
// int16_t* resampled = (int16_t*) calloc(nfft, sizeof(int16_t));
kiss_fftr_cfg st = kiss_fftr_alloc(nfft, 0, NULL, NULL);

	uint16_t sample_rate = 0;
	uint32_t sample_count = 0;
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
			}

			player->decodeSample(fillBuff + MINIMP3_MAX_SAMPLES_PER_FRAME, &sample_len); 
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


k = total_samples;

for (i = 0; i < nfft; i++)
{
	l_channel[i] = fillBuff[i * 2];
	r_channel[i] = fillBuff[(i * 2) + 1];
}

kiss_fftr(st, (kiss_fft_scalar*)l_channel, spectrum);

// fix_fftr(l_channel, FFT_SAMPLE_SIZE_POWER, 0);
// fix_fftr(r_channel, FFT_SAMPLE_SIZE_POWER, 0);

//Binning
k = 0;
l = nfft >> 2;
l_max = 0;
r_max = 0;
l_max_i = 0;
r_max_i = 0;

for (i = 0 ; i < FFT_BINS ; i++)			
{
	l_bins_r[i] = 0;
	l_bins_i[i] = 0;
}

//First have are real numbers we will use for max
for(i = 0; i < l; i++)
{

	//bins
	if (i <= 3 * 2)
	{		
		l_bins_r[0] += spectrum[i].r;
		l_bins_i[0] += spectrum[i].i;
	}
	else if (i <= 6 * 2)
	{
		l_bins_r[1] += spectrum[i].r;
		l_bins_i[1] += spectrum[i].i;
	}
	else if (i <= 13 * 2)
	{
		l_bins_r[2] += spectrum[i].r;
		l_bins_i[2] += spectrum[i].i;		
	}
	else if (i <= 27 * 2)
	{
		l_bins_r[3] += spectrum[i].r;
		l_bins_i[3] += spectrum[i].i;		
	}
	else if (i <= 55 * 2)
	{
		l_bins_r[4] += spectrum[i].r;
		l_bins_i[4] += spectrum[i].i;		
	}
	else if (i <= 112 * 2)
	{
		l_bins_r[5] += spectrum[i].r;
		l_bins_i[5] += spectrum[i].i;		
	}
	else if (i <= 229 * 2)
	{
		l_bins_r[6] += spectrum[i].r;
		l_bins_i[6] += spectrum[i].i;		
	}
	else 
	{
		l_bins_r[7] += spectrum[i].r;
		l_bins_i[7] += spectrum[i].i;		
	}
	// if (i<=3 )           {l_bins[0]  += l_channel[i]; r_bins[0]  += r_channel[i];}
	// if (i>3   && i<=6  ) {l_bins[1]  += l_channel[i]; r_bins[1]  += r_channel[i];}
	// if (i>6   && i<=13 ) {l_bins[2]  += l_channel[i]; r_bins[2]  += r_channel[i];}
	// if (i>13  && i<=27 ) {l_bins[3]  += l_channel[i]; r_bins[3]  += r_channel[i];}
	// if (i>27  && i<=55 ) {l_bins[4]  += l_channel[i]; r_bins[4]  += r_channel[i];}
	// if (i>55  && i<=112) {l_bins[5]  += l_channel[i]; r_bins[5]  += r_channel[i];}
	// if (i>112 && i<=229) {l_bins[6]  += l_channel[i]; r_bins[6]  += r_channel[i];}
	// if (i>229          ) {l_bins[7]  += l_channel[i]; r_bins[7]  += r_channel[i];}	

	// if ((i % m) == (m - 1))
	// {
		
	// 	l_bins[k] = sqrt((uint32_t)(l_channel[l_max_i] * l_channel[l_max_i]) + (uint32_t)(l_channel[l_max_i + l] * l_channel[l_max_i + l]));
	// 	r_bins[k] = sqrt((uint32_t)(r_channel[r_max_i] * r_channel[r_max_i]) + (uint32_t)(r_channel[r_max_i + l] * r_channel[r_max_i + l]));
	// 	//printf("setting max at %d, l=%d, r=%d, l=%d, r=%d\n", i, l_max_i, r_max_i, l_bins[k], r_bins[k]);
	// 	l_max = 0;
	// 	r_max = 0;

	// 	k++;
	// }

	// if (l_channel[i] > l_max)
	// {
	// 	l_max = l_channel[i];
	// 	l_max_i = i;
	// }

	// if (r_channel[i] > r_max)
	// {
	// 	r_max = r_channel[i];
	// 	r_max_i = i;
	// }	

	// l_max = ((l_channel[i] > l_max) ? l_channel[i] : l_max);
	// r_max = ((r_channel[i] > r_max) ? r_channel[i] : r_max);
	
}

uint16_t l_val = 0;
uint16_t r_val = 0;

for (i = 0 ; i < FFT_BINS ; i++)			
{
	l_val = get_bar_mag(l_bins_r[i], l_bins_i[i]);
	printf("Bin %d, val %d\n", i, l_val);
	img_pg0[(i * 2) + 0] = l_val & 0xff;
	img_pg0[(i * 2) + 1] = img_pg0[(i * 2) + 0];
	
// 	img_pg0[(i * 4) + 2] = r_max & 0xff;
// 	img_pg0[(i * 4) + 3] = img_pg0[(i * 4) + 2];

	img_pg1[(i * 2) + 0] = l_val >> 8 & 0xff;
	img_pg1[(i * 2) + 1] = img_pg1[(i * 2) + 0];
	
// 	img_pg1[(i * 4) + 2] = r_max >> 8 & 0xff;
// 	img_pg1[(i * 4) + 3] = img_pg1[(i * 4) + 2];	
}
printf("\n");

// for (i = 0 ; i < FFT_BINS ; i++)			
// {
// 	//double wide
// 	//bottom half
// 	//printf("i: %d, l_bin: %d, rshifted: %d, val:%u\n", i, l_bins[i], (l_bins[i] >> 9), (1 << (l_bins[i] >> 9)));
// 	l_max = (1 << (l_bins[i] >> 16)) - 1;
// 	r_max = (1 << (r_bins[i] >> 16)) - 1;

// 	img_pg0[(i * 4) + 0] = l_max & 0xff;
// 	img_pg0[(i * 4) + 1] = img_pg0[(i * 4) + 0];
	
// 	img_pg0[(i * 4) + 2] = r_max & 0xff;
// 	img_pg0[(i * 4) + 3] = img_pg0[(i * 4) + 2];

// 	img_pg1[(i * 4) + 0] = l_max >> 8 & 0xff;
// 	img_pg1[(i * 4) + 1] = img_pg1[(i * 4) + 0];
	
// 	img_pg1[(i * 4) + 2] = r_max >> 8 & 0xff;
// 	img_pg1[(i * 4) + 3] = img_pg1[(i * 4) + 2];			

// }
 ssd1306_display_image(&oled_display, 4, 0, img_pg0, sizeof(img_pg0));		
 ssd1306_display_image(&oled_display, 5, 0, img_pg1, sizeof(img_pg1));	


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
		// ssd1306_display_text(&oled_display, 4, "<BT SINK NAME>", strlen("<BT SINK NAME>"), false);

		// ssd1306_display_text(&oled_display, 5, "<DISPLAY MODE>", strlen("<DISPLAY MODE>"), false);

		// ssd1306_display_text(&oled_display, 6, "<BATTERY?>", strlen("<BATTERY?>"), false);



		vTaskDelay(pdMS_TO_TICKS(OLED_DISP_INT));
	}
}



void vFFT_FrontDisplay(void * pvParameters)
{
	int i, k, m = FFT_SAMPLE_SIZE / FFT_BINS;
	short * buff;
	short l_channel[FFT_SAMPLE_SIZE];
	short r_channel[FFT_SAMPLE_SIZE];
	short l_bins[FFT_BINS];
	short r_bins[FFT_BINS];
	short l_max;
	short r_max;
	uint8_t img_pg0[32];
	uint8_t img_pg1[32];
	// Create the FFT config structure
	// fft_config_t *real_fft_plan = fft_init(FFT_SAMPLE_SIZE, FFT_REAL, FFT_FORWARD, NULL, NULL);

	while(1)
	{


		if (playing)
		{
			k = total_samples;
			buff = buff0;
			if (buff_num)
			{
				buff = buff1;
			} 

			for (i = 0; i < FFT_SAMPLE_SIZE; i++)
			{
				l_channel[i] = buff[i * 2];
				r_channel[i] = buff[(i * 2) + 1];
			}

			// fix_fftr(l_channel, FFT_SAMPLE_SIZE_POWER, 0);
			// fix_fftr(r_channel, FFT_SAMPLE_SIZE_POWER, 0);

//Binning
			k = 0;
			l_max = 0;
			r_max = 0;

			for(i = 0; i < FFT_SAMPLE_SIZE; i++)
			{
				if ((i % m) == (m - 1))
				{
					//printf("setting max at %d\n", i);
					l_bins[k] = l_max;
					r_bins[k] = r_max;

					l_max = 0;
					r_max = 0;

					k++;
				}

				l_max = ((l_channel[i] > l_max) ? l_channel[i] : l_max);
				r_max = ((r_channel[i] > r_max) ? r_channel[i] : r_max);
				
			}

			

			for (i = 0 ; i < FFT_BINS ; i++)			
			{
				//double wide
				//bottom half
				//printf("i: %d, l_bin: %d, rshifted: %d, val:%u\n", i, l_bins[i], (l_bins[i] >> 9), (1 << (l_bins[i] >> 9)));
				l_max = (1 << (l_bins[i] >> 8)) - 1;
				r_max = (1 << (r_bins[i] >> 8)) - 1;

				img_pg0[(i * 4) + 0] = l_max & 0xff;
				img_pg0[(i * 4) + 1] = img_pg0[(i * 4) + 0];
				
				img_pg0[(i * 4) + 2] = r_max & 0xff;
				img_pg0[(i * 4) + 3] = img_pg0[(i * 4) + 2];

				img_pg1[(i * 4) + 0] = l_max >> 8 & 0xff;
				img_pg1[(i * 4) + 1] = img_pg1[(i * 4) + 0];
				
				img_pg1[(i * 4) + 2] = r_max >> 8 & 0xff;
				img_pg1[(i * 4) + 3] = img_pg1[(i * 4) + 2];

				//top half
				// img_pg1[(i * 4) + 0] = l_bins[i] >> 8 & 0xff;
				// img_pg1[(i * 4) + 1] = l_bins[i] >> 8 & 0xff;
				
				// img_pg1[(i * 4) + 2] = r_bins[i] >> 8 & 0xff;
				// img_pg1[(i * 4) + 3] = r_bins[i] >> 8 & 0xff;				
		
			}
			ssd1306_display_image(&oled_display, 4, 0, img_pg0, sizeof(img_pg0));		
			ssd1306_display_image(&oled_display, 5, 0, img_pg1, sizeof(img_pg1));		
//				printf("bin %d : l=%d, r=%d\n", i, l_bins[i], r_bins[i]);



		}
		
			vTaskDelay(pdMS_TO_TICKS(50));
	}
}


void vFrontSideDisplay(void * pvParameters)
{
	//Using this as a test for now
	WS2812* ws2812 = new WS2812(FRONT_DISPLAY_PIN, RGB_LED_COUNT);
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	while (1)
	{
		red += 16;
		if (red >= 128)
		{
			red = 0;
			green += 16;

			if (green >= 128)
			{
				green = 0;
				blue += 16;

				if (blue >= 128)
				{
					blue = 0;
				}
			}
		}

		for (uint16_t i = 0; i < RGB_LED_COUNT; i++)
		{
			ws2812->setPixel(i, red, green, blue);	
		}
		ws2812->show();

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

extern "C" void app_main(void)
{
	//FOR TESTING: address 42:fa:bf:75:ca:26, name Q50

	// static uint8_t ucParameterToPass;

    
    // xTaskCreatePinnedToCore(vTaskCode, "BT_CORE", 32768, NULL, 1, NULL, 0);

	//i2s_output = true;

	xTaskCreatePinnedToCore(vDisplayUpdate, "OLED_DISPLAY", 2048, NULL, 1, NULL, 1);

	SDCard* sdc = new SDCard();
	sdc->init();






	xTaskCreatePinnedToCore(vI2SOutput, "I2S_OUTPUT", 2048, NULL, configMAX_PRIORITIES - 5, NULL, 1);
	xTaskCreatePinnedToCore(vButtonInput, "BUTTON_INPUT", 2048, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(vMp3Decode, "MP3_CORE", 1024*32, NULL, 10, &mp3TaskHandle, 1);
	//xTaskCreate(vFFT_FrontDisplay, "FFT_and_FRONT", 1024*32, NULL, 2, NULL);
	xTaskCreate(vFrontSideDisplay, "FRONT_SIDES", 1024*32, NULL, 2, NULL);

	esp_bd_addr_t addr = { 0x42, 0xfa, 0xbf, 0x75, 0xca, 0x26 };
	
	BT_a2db bt(bt_app_a2d_data_cb);
	bt_control = &bt;
	bt.connect_bluetooth(addr);

	//bt.discover_bluetooth(CONFIG_SPEAKER_NAME);

	
}
