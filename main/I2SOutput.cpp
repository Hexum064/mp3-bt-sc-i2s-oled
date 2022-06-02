
#include "I2SOutput.h"
#include "esp_log.h"
static bool installed = false;

I2SOutput::I2SOutput(i2s_port_t i2s_port, i2s_pin_config_t &i2s_pins) : Output(i2s_port), m_i2s_pins(i2s_pins)
{
}

void I2SOutput::start(int sample_rate)
{   

    // i2s config for writing both channels of I2S
    i2s_config_t i2s_config {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = (uint32_t)sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0 };
    //install and start i2s driver
    // if (installed)
    // {
    //     installed = false;
    //     ESP_LOGI("I2SOutput", "Uninstalling driver");
    //     i2s_stop(m_i2s_port);
    //     i2s_driver_uninstall(m_i2s_port);

    // }    
    ESP_LOGI("I2SOutput", "Installing driver");

    i2s_driver_install(m_i2s_port, &i2s_config, 0, NULL);
    installed = true;
    // set up the i2s pins
    ESP_LOGI("I2SOutput", "Setting pins");
    i2s_set_pin(m_i2s_port, &m_i2s_pins);
    // clear the DMA buffers
    ESP_LOGI("I2SOutput", "clearing dma");
    vTaskDelay(pdMS_TO_TICKS(100));
    i2s_zero_dma_buffer(m_i2s_port);

    ESP_LOGI("I2SOutput", "Starting I2S");
    i2s_start(m_i2s_port);
    started = true;
}
