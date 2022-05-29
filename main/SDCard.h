
#include <stdbool.h>


#ifndef SDCARD_H
#define SCDARD_H



#define MOUNT_POINT "/sdcard"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  GPIO_NUM_12
#define PIN_NUM_MOSI  GPIO_NUM_13
#define PIN_NUM_CLK   GPIO_NUM_14
#define PIN_NUM_CS    GPIO_NUM_15
#define PIN_NUM_CD    GPIO_NUM_27
#define SPI_DMA_CHAN    2

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