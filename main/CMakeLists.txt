idf_component_register(
    SRCS
        "ws2812.cpp"
        "kiss_fft.cpp"
        "kiss_fftr.cpp"
        "ssd1306_i2c.cpp"
        "ssd1306_spi.cpp" 
        "ssd1306.cpp"
        "Fifo.cpp" 
        "I2SOutput.cpp" 
        "Output.cpp" 
        "SDCard.cpp" 
        "bt_app_core.cpp" 
        "BT_a2dp.cpp" 
        "MP3Player.cpp" 
        "FileNavi.cpp" 
        "main.cpp"
    INCLUDE_DIRS "." 
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/btc/profile/std/include" 
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/common/include"
        "M:/esp-idf-extension/esp-idf/components/bt/common/include"
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/stack/include"
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/bta/include"
        "M:/esp-idf-extension/esp-idf/components/bt/common/btc/include"
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/btc/include"
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/btc/profile/std/a2dp/include"
        "M:/esp-idf-extension/esp-idf/components/bt/host/bluedroid/external/sbc/encoder/include")

# Create a SPIFFS image from the contents of the 'nyan_data' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_create_partition_image(storage ../nyan_data FLASH_IN_PROJECT)        
