
#include <stdbool.h>


#ifndef SDCARD_H
#define SCDARD_H

#include "defines.h"



class SDCard
{

    private :
        bool initialized = false;

    public :
        SDCard();
        bool init();
        bool isInitialized();


};


#endif //SDCARD_H