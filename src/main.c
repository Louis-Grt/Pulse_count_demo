#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "Demo_Pulse_Counter";

#define PCNT_HIGH_LIMIT 1
#define PCNT_LOW_LIMIT -1

#define EC11_GPIO_A 2
#define EC11_GPIO_B -1

QueueHandle_t queue;

static bool example_pcnt_on_reach(pcnt_unit_handle_t unit,
                                  const pcnt_watch_event_data_t *edata,
                                  void *user_ctx)
{
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);

    return true;
}

static void pcnt_init()
{
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = EC11_GPIO_A,
        .level_gpio_num = EC11_GPIO_B,
        .flags = {
            .virt_level_io_level = 0,
        }, // positive edge
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
        pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_HOLD));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, PCNT_HIGH_LIMIT));

    pcnt_event_callbacks_t cbs = {
        .on_reach = example_pcnt_on_reach,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, queue));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

void app_main(void)
{
    QueueHandle_t queue = xQueueCreate(10, sizeof(int));
    pcnt_init();

    int event_count = 0;
    int pulse_count = 0;
    float time_passed = 0;
    while (1)
    {
        if (xQueueReceive(queue, &event_count, pdMS_TO_TICKS(1000)))
        {
            pulse_count += 1;
            ESP_LOGI(TAG, "Watch point event, count: %d", event_count);
        }
        else
        {
            ESP_LOGI(TAG, "Pulse count: %d", pulse_count);
            float frequency = pulse_count / time_passed;
            ESP_LOGI(TAG, "Frequency: %f Hz", frequency);
        }
        time_passed += 1;
    }
}