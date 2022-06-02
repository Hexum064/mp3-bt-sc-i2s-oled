  

#ifndef MP3_PLAYER_H
#define MP3_PLAYER_H

#include "minimp3.h"





class MP3Player
{

    public:
        bool is_output_started = false;
        mp3d_sample_t *pcm;
        mp3dec_frame_info_t info = {};
        MP3Player(const char *file, long file_start_pos);
        void dispose();
        mp3d_sample_t * decodeSample(int * sample_len);
        void decodeSample(mp3d_sample_t * buffer, int * sample_len);
        long get_file_pos();

};


#endif //MP3_PLAYER_H
