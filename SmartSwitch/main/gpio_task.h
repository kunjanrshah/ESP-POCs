#ifndef GPIO_H
#define GPIO_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// REALY OUTPUT
#define GPIO_OUTPUT_IO_0 18
#define GPIO_OUTPUT_IO_1 19

#define GPIO_OUTPUT_IO_2 22
#define GPIO_OUTPUT_IO_3 23

#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1) | (1ULL << GPIO_OUTPUT_IO_2) | (1ULL << GPIO_OUTPUT_IO_3)) // 4 pin selected

// STATUS
#define GPIO_INPUT_IO_0 4
#define GPIO_INPUT_IO_1 5
#define GPIO_INPUT_IO_2 21
#define GPIO_INPUT_IO_3 15

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1) | (1ULL << GPIO_INPUT_IO_2) | (1ULL << GPIO_INPUT_IO_3))
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;

    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));

            printf("val: %d\n\n\n", gpio_get_level(io_num));
        }
    }
}


static void do_operation()
{
    while (1)
    {
        vTaskDelay(10000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "write....... %d ", save_status);
        if(save_status){
             save_status = false;
              storeStatusToMemory();
        }
       
        for (int i = 1; i < 10; i = i + 3)
        {
            // ESP_LOGE(TAG, "loop state ===> : %d ==> %d",send_param->seq_status[i], send_param->seq_status[i+2]);
            if (store_status[i] == ESPNOW_DEVICE_ID)
            {
                state = store_status[i + 2];
                break;
            }
        }
        gpio_set_level(GPIO_OUTPUT_IO_0, getBit(state, 0)); //led 1
        gpio_set_level(GPIO_OUTPUT_IO_1, getBit(state, 1)); //led 2

        gpio_set_level(GPIO_OUTPUT_IO_2, getBit(state, 2)); //led 3
        gpio_set_level(GPIO_OUTPUT_IO_3, getBit(state, 3)); //led 4

    }
    vTaskDelete(NULL);
}

void gpio_init(){

   
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *)GPIO_INPUT_IO_1);

    gpio_isr_handler_add(GPIO_INPUT_IO_2, gpio_isr_handler, (void *)GPIO_INPUT_IO_2);
    gpio_isr_handler_add(GPIO_INPUT_IO_3, gpio_isr_handler, (void *)GPIO_INPUT_IO_3);

    //remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

}

#endif