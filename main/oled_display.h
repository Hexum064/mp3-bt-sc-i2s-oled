#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "defines.h"
#include "utilities.h"

void oled_display_normal_mode(SSD1306_t *oled_display)
{

	int totalSeconds = get_runtime_seconds();
	int minutes = totalSeconds / 60;
	int seconds = totalSeconds % 60;

	char disp_buff[24];	
	
	disp_buff[0] = '\0';

	if (playing)
	{
		sprintf(disp_buff, "%02d:%02d Playing  ", minutes, seconds);	
	}
	else
	{
		sprintf(disp_buff, "%02d:%02d Paused   ", minutes, seconds);	
	}



	
	ssd1306_display_text(oled_display, 1, disp_buff, 16, false);


	if (repeat_mode)
	{
		sprintf(disp_buff, "Repeat Song Mode");
	}
	else
	{
		sprintf(disp_buff, "Normal Play Mode");		
	}

	ssd1306_display_text(oled_display, 2, disp_buff, 16, false);

	disp_buff[0] = '\0';

	if (i2s_output)
	{
		sprintf(disp_buff, "Vol:%02d%% Out:i2s ", (int)((output_volume / MAX_VOL) * 100));	
	}
	else
	{
		sprintf(disp_buff, "Vol:%02d%% Out:BT  ", (int)((output_volume / MAX_VOL) * 100));	
	}
	
	ssd1306_display_text(oled_display, 3, disp_buff, 16, false);

	disp_buff[0] = '\0';

	if (bt_control->get_a2d_state() != 5)
	{
		sprintf(disp_buff, "BT Attaching to:");	
								
	}
	else
	{
		sprintf(disp_buff, "BT Connected to:");	
	}
	
	ssd1306_display_text(oled_display, 4, disp_buff, 16, false);
	ssd1306_display_text(oled_display, 5, bt_sink_name, 16, false);
	
	if (nyan_mode)
	{
		sprintf(disp_buff, "   Nyan  Mode   ");	
	}
	else
	{
		sprintf(disp_buff, "Display mode: %d ", display_index);	
		
	}
	
	ssd1306_display_text(oled_display, 6, disp_buff, 16, false);

	sprintf(disp_buff, "Battery %d%%    ", get_batt_percent());
	ssd1306_display_text(oled_display, 7, disp_buff, 16, false);
	








}	



void oled_display_discovery_mode(SSD1306_t *oled_display, char * current)
{
	char str_buff[MAX_CHARS + 1];
	uint8_t start_index = 0;
	bool selected = false;
	//Then "xx of yy"
	//Then show up to 6. highlight the current index.
	if (bt_discovered_count == 0)
	{
		sprintf(str_buff, "No Devices Found");
	}
	else
	{
		memset(str_buff, ' ', 16);
		sprintf(str_buff, "%02d of %02d", bt_device_list_index + 1, bt_discovered_count );
	}

	ssd1306_display_text(oled_display, 1, str_buff, 16, false);

	if (bt_device_list_index >= bt_discovered_count)
	{
		bt_device_list_index = 0;
	}

	if (bt_device_list_index > 3)
	{
		start_index = bt_device_list_index - 3;
	}
	if (bt_device_list_index + 4 > bt_discovered_count && bt_discovered_count > 6)
	{
		start_index = bt_discovered_count - 6;
	}

//printf("start: %u, selected: %u, count: %u\n", start_index, bt_device_list_index, bt_discovered_count);

	for (uint8_t i = 0; i < 6; i++)
	{
		memset(str_buff, ' ', 16);
		selected = false;

		if (i + start_index < bt_discovered_count)
		{
			selected = bt_device_list_index == i + start_index || (current && !(strcmp(current, (char *)bt_discovered_devices[i + start_index].name)));
			memcpy(str_buff, bt_discovered_devices[i + start_index].name, strlen((char *)bt_discovered_devices[i + start_index].name));	
			//printf("Adding device %s, start: %u, selected: %u, count: %u\n", bt_discovered_devices[i + start_index].name, start_index, bt_device_list_index, bt_discovered_count);
		}
		ssd1306_display_text(oled_display, i + 2, str_buff, 16,  selected);
	}

}



#endif //OLED_DISPLAY_H