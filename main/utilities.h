#ifndef UTILITIES_H
#define UTILITIES_H

#define ADC_BATT_CHANNEL ADC1_CHANNEL_0
#define BATT_100 2000	//approx 6v with a 100K/22K divider
#define BATT_0	1500	//approx 4.1v with a 100K/22K divider
#define BATT_ADC_RANGE (BATT_100 - BATT_0)
#define BATT_ROLLING_AVG_CNT 40

int total_samples = 0;
uint16_t current_sample_rate = 0;

uint16_t batt_vals[BATT_ROLLING_AVG_CNT];
uint8_t batt_vals_index = 0;

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

int get_runtime_seconds()
{
    int totalSeconds = 0;
    if (current_sample_rate > 0)
        totalSeconds = (int)(total_samples / current_sample_rate);    
    return totalSeconds;
}

uint8_t get_batt_percent()
{
    int adc_batt_val = 0;

	batt_vals[batt_vals_index] = adc1_get_raw(ADC_BATT_CHANNEL);
	adc_batt_val = 0;

	//summing
	for(uint8_t i = 0; i < BATT_ROLLING_AVG_CNT; i++)
	{
		adc_batt_val += batt_vals[i];
	}

	//avging
	adc_batt_val /= BATT_ROLLING_AVG_CNT;

	batt_vals_index++;

	if (batt_vals_index >= BATT_ROLLING_AVG_CNT)
	{
		batt_vals_index = 0;
	}

	adc_batt_val = (int)(((float)(adc_batt_val - BATT_0) / (float)BATT_ADC_RANGE) * 100);

	if (adc_batt_val > 100)
	{
		adc_batt_val = 100;
	}

	if (adc_batt_val < 0)
	{
		adc_batt_val = 0;
	}    

    return adc_batt_val;
}
#endif //UTILITIES_H