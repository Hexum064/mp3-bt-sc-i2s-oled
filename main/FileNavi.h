
#ifndef FILENAVI_H
#define FILENAVI_H

#define MAX_PATH_LEN 1024

class FileNavi
{
    public:
        static char *get_current_path();
        static struct dirent *get_current_file();
        static char* get_current_full_name();
        static void navigate_to_pos(long int pos);
        static void goto_next_file();
        static void goto_prev_file();
        static void goto_first_mp3();
        static void goto_next_mp3();
        static void goto_prev_mp3();
};

#endif //FILENAVI_H