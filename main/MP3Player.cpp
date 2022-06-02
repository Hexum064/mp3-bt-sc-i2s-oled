
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#define MP3_BUFFER_SIZE 1024

#include "esp_log.h"

#include <string.h>
#include <stdio.h>
#include "MP3Player.h"



FILE *fp;


int to_read = MP3_BUFFER_SIZE;
uint8_t input_buf[MP3_BUFFER_SIZE];
int buffered = 0;
int decoded = 0;
int bytes_read = 0;
mp3dec_t mp3d = {};

// MP3Player::MP3Player(const char *file)
// {
//    mp3_player_init(file , 0);

// }


MP3Player::MP3Player(const char *file, long file_start_pos)
{
    // this assumes that you have uploaded the mp3 file to the SPIFFS
    ESP_LOGI("MP3Player", "Opening file %s", file);
    fp = fopen(file, "r");
    fseek(fp, file_start_pos, SEEK_SET);
    if (!fp)
    {
        ESP_LOGE("MP3Player", "Failed to open file");
        return;
    }

    is_output_started = false;
    buffered = 0;
    decoded = 0;
    to_read = MP3_BUFFER_SIZE;

}


void MP3Player::dispose()
{
    
 
    // free(input_buf);
    if (fp)
    {
        ESP_LOGI("MP3Player", "Closing file");
        fclose(fp);
    }
}

long MP3Player::get_file_pos()
{
    return ftell(fp);
}

mp3d_sample_t * MP3Player::decodeSample(int * sample_len)
{
    if (pcm == NULL)
    {
        pcm = (mp3d_sample_t *)malloc(sizeof(mp3d_sample_t) * MINIMP3_MAX_SAMPLES_PER_FRAME);
    }

    decodeSample(pcm, sample_len);
    return pcm;
}

void MP3Player::decodeSample(mp3d_sample_t * buffer, int * sample_len) {

    int samples;

    // #ifdef VOLUME_CONTROL
    //     auto adc_value = float(adc1_get_raw(VOLUME_CONTROL)) / 4096.0f;
    //     // make the actual volume match how people hear
    //     // https://ux.stackexchange.com/questions/79672/why-dont-commercial-products-use-logarithmic-volume-controls
    //     output->set_volume(adc_value * adc_value);
    // #endif
    // read in the data that is needed to top up the buffer
    size_t n = fread(input_buf + buffered, 1, to_read, fp);

    buffered += n;
    bytes_read += n;

    if (buffered == 0)
    {
        // we've reached the end of the file and processed all the buffered data

        is_output_started = false;
        return ;
    }

    // decode the next frame
    samples = mp3dec_decode_frame(&mp3d, input_buf, buffered, buffer, &info);
    //  we've processed this may bytes from teh buffered data
    buffered -= info.frame_bytes;

    // shift the remaining data to the front of the buffer
    memmove(input_buf, input_buf + info.frame_bytes, buffered);  
    to_read = info.frame_bytes;

    if (samples > 0 && !is_output_started)
    {
        is_output_started = true;
        ESP_LOGI("MP3Player", "decoding started\n");        
    }

    *sample_len = samples;

}