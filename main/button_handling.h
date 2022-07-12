#ifndef BUTTON_HANDLING_H
#define BUTTON_HANDLING_H

#include "rgb_led_displays.h"
#include "utilities.h"
#include "defines.h"


bool playing = false;
bool i2s_output = true;
bool bt_enabled = true;
bool bt_discovery_mode = false;
bool nyan_mode = false;
bool repeat_mode = false;

bt_device_param *bt_discovered_devices;
uint8_t bt_discovered_count = 0;
uint8_t bt_device_list_index = 0;
uint8_t display_index = 0;
BT_a2dp *bt_control;
char bt_discovery_mode_flag_key[] = "btd";
char bt_sink_name_key[] = "btname";
char bt_sink_addr_key[] = "btaddr_";

//+ 1 for term char
char bt_sink_name[MAX_BT_NAME_LEN + 1];
uint8_t bt_sink_addr[6];
float output_volume = MAX_VOL / 2;

char * full_path;
uint16_t full_path_len = 0;

bool f_change_file = false;
bool initializing = false;
bool sd_initialized = false;

TaskHandle_t mp3TaskHandle = NULL;


void toggle_play_pause()
{
	//Don't want to play or pause in this mode 
	if (bt_discovery_mode)
	{
		if (bt_discovered_count > 0)
		{
			
			ESP_LOGI("main", "Selecting BT device %s, addr %02x:%02x:%02x:%02x:%02x:%02x\n", bt_discovered_devices[bt_device_list_index].name,
				bt_discovered_devices[bt_device_list_index].address[0], bt_discovered_devices[bt_device_list_index].address[1], bt_discovered_devices[bt_device_list_index].address[2], 
				bt_discovered_devices[bt_device_list_index].address[3], bt_discovered_devices[bt_device_list_index].address[4], bt_discovered_devices[bt_device_list_index].address[5]);
			printf("Writing name: %d\n", nvs_set_str(bt_control->get_nvs_handle(), bt_sink_name_key, (char *)bt_discovered_devices[bt_device_list_index].name));
			nvs_commit(bt_control->get_nvs_handle());
			//printf("Writing addr: %d\n", nvs_set_blob(bt_control->get_nvs_handle(), bt_sink_addr_key, (uint8_t *)bt_discovered_devices[bt_device_list_index].address, 6));
			//Having an issue with blob where I could not read it. Using a more heavy-handed approach
			bt_sink_addr_key[6] = '0';
			printf("Writing addr 0: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[0]));
			nvs_commit(bt_control->get_nvs_handle());

			bt_sink_addr_key[6] = '1';
			printf("Writing addr 1: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[1]));
			nvs_commit(bt_control->get_nvs_handle());

			bt_sink_addr_key[6] = '2';
			printf("Writing addr 2: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[2]));
			nvs_commit(bt_control->get_nvs_handle());

			bt_sink_addr_key[6] = '3';
			printf("Writing addr 3: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[3]));
			nvs_commit(bt_control->get_nvs_handle());

			bt_sink_addr_key[6] = '4';
			printf("Writing addr 4: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[4]));
			nvs_commit(bt_control->get_nvs_handle());

			bt_sink_addr_key[6] = '5';
			printf("Writing addr 5: %d\n", nvs_set_u8(bt_control->get_nvs_handle(), bt_sink_addr_key, bt_discovered_devices[bt_device_list_index].address[5]));
			nvs_commit(bt_control->get_nvs_handle());

			printf("Restarting after bt device set.\n"); //testing
			vTaskDelay(pdMS_TO_TICKS(100)); //testing

			esp_restart();
		}
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
		if (bt_device_list_index < bt_discovered_count - 1)
		{
			ESP_LOGI("main", "Going to next device.");
			bt_device_list_index++;
		}
		return;
	}

	if (nyan_mode)
	{
		return;
	}

	ESP_LOGI("main", "Going to next file.");
	//For display
    FileNavi::goto_next_mp3();
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
		if (bt_device_list_index > 0)
		{
			ESP_LOGI("main", "Going to previous device.");
			bt_device_list_index--;
		}
		return;
	}


	if (nyan_mode)
	{
		return;
	}	

	ESP_LOGI("main", "Going to previous file.");

    
    if (get_runtime_seconds() < SECONDS_TILL_RESET_CURRENT_SONG)
    {
	    FileNavi::goto_prev_mp3();
    }
	
	//For display
	full_path = FileNavi::get_current_full_name();
	full_path_len = strlen(full_path);
	//end for display	
	f_change_file = true;

}

void cycle_display()
{
	if (nyan_mode)
	{
		return;
	}	


	if (display_index == MAX_DISPLAY_MODES - 1)
	{
		display_index = 0;
	}
	else
	{
		display_index++;
	}
}

void toggle_bt_discovery_mode()
{
	printf("Toggling bt discovery mode\n");
	nyan_mode = false;

	if (!bt_discovery_mode)
	{
		nvs_set_u8(bt_control->get_nvs_handle(), bt_discovery_mode_flag_key, 1);
		nvs_commit(bt_control->get_nvs_handle());
	}

	esp_restart();


}

void toggle_nyan_mode()
{
	printf("Toggling nyan display mode\n");
	nyan_mode = !nyan_mode;
	if (nyan_mode)
	{
		display_nyan();
		
	}
	f_change_file = true; //trigger the mp3 decoder to switch 
    vTaskResume(mp3TaskHandle);	
}

void toggle_normal_repeat_mode()
{
    repeat_mode = !repeat_mode;
}

void update_front_display(short * buff)
{


	if (nyan_mode)
	{
		display_nyan();
	}
	else
	{
		switch (display_index)
		{
			case 0:
				display_sins(buff);
				break;
			case 1:
				display_combo_option_0();
				break;
			case 2:
				display_combo_option_1();
				break;
			case 3:
				display_combo_option_2();
				break;
			case 4:
				display_combo_option_3();
				break;
			case 5:
				display_fft(buff);
				break;
			default:
				blank_display();
				break;

		}
	}	
}

void handle_button_input()
{
    uint8_t input = 0;
	static uint8_t last_input = 0;
	static bool held = false;
	static uint64_t start_time = 0;

    input = gpio_get_level(BUTTON_BIT_0) | (gpio_get_level(BUTTON_BIT_1) << 1) | (gpio_get_level(BUTTON_BIT_2) << 2);

#ifdef INVERSE_BUTTON_INPUT

	input = (~input) & 0x07;

#endif

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
                        break; //do nothing intentionally
                    case PAUSE_BUTTON:	
                        toggle_normal_repeat_mode();												
                        break; //do nothing intentionally
                    case NEXT_BUTTON:						
                        break; //do nothing intentionally
                    case OUTPUT_BUTTON:		
                        //This should reset the mcu so no need to do anything else
                        toggle_bt_discovery_mode();
                        
                        break;
                    case DISPLAY_BUTTON:					
                        toggle_nyan_mode();
                        break;
                    case VOL_DOWN_BUTTON:
                        volume_down();	
                        volume_down();
                        volume_down();
                        volume_down();		
                        held = false;
                        last_input = input;
                        start_time = esp_timer_get_time();
                        break;
                    case VOL_UP_BUTTON:
                        volume_up();
                        volume_up();
                        volume_up();
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
                    cycle_display();
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

    //If the Card Detect goes low, it means the card was removed so it get's uninitialized
    if (!gpio_get_level(PIN_NUM_CD))
    {
        sd_initialized = false;
    }

    //if the sd card was not initialized but the card detect signal goes high, it means the card was inserted
    //after startup so it's best to restart now.
    if (gpio_get_level(PIN_NUM_CD) && !sd_initialized && !initializing && !bt_discovery_mode)
    {
        esp_restart();
    }

}

#endif //BUTTON_HANDLING_H