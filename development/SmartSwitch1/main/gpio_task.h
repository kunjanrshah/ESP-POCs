#ifndef GPIO_TASK_H
#define GPIO_TASK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define ECHO_TEST_TXD  (GPIO_NUM_17)
#define ECHO_TEST_RXD  (GPIO_NUM_16)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)
#define BUF_SIZE (1024)

// REALY OUTPUT
#define GPIO_OUTPUT_IO_0 18
#define GPIO_OUTPUT_IO_1 19

#define GPIO_OUTPUT_IO_2 22
#define GPIO_OUTPUT_IO_3 23

#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1) | (1ULL << GPIO_OUTPUT_IO_2) | (1ULL << GPIO_OUTPUT_IO_3)) // 4 pin selected

// // STATUS
// #define GPIO_INPUT_IO_0 4
// #define GPIO_INPUT_IO_1 5
// #define GPIO_INPUT_IO_2 21
// #define GPIO_INPUT_IO_3 15

// #define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1) | (1ULL << GPIO_INPUT_IO_2) | (1ULL << GPIO_INPUT_IO_3))
// #define ESP_INTR_FLAG_DEFAULT 0

// static xQueueHandle gpio_evt_queue = NULL;
// static bool flag = true;
// esp_timer_handle_t oneshot_timer;
// static void oneshot_timer_callback(void *arg);
// static void IRAM_ATTR gpio_isr_handler(void *arg)
// {
//     uint32_t gpio_num = (uint32_t)arg;

//     xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
// }

static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    char current_value[5] =  {'0','0','0','0','\0'}; 
    char switch_value[3] = {'0','0','\0'};

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    int pos=0;
    for (int i = 1; i < 10; i = i + 3)
        {
            if (store_status[i] == ESPNOW_DEVICE_ID)
            {
                pos=i+2;
                break;
            }
        }
    
    while (1) {

            // Read data from the UART
            uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 20 / portTICK_RATE_MS);
            printf("MS51 data: %s \n", data);
            current_value[0]= (char) data[0];
            current_value[1]= (char) data[1];
            current_value[2]= (char) data[2];
            current_value[3]= (char) data[3];
            // (char) data[4];
            switch_value[0]= (char) data[5];
            switch_value[1]= (char) data[6];
            
            int switch_val=0;
            sscanf(switch_value, "%d", &switch_val);
            
            // if (store_status[pos] != switch_val){
            //         store_status[pos]=switch_val;
            //         storeStatusToMemory();
            // }
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        vTaskDelete(NULL);
        free(data);
}

// static void gpio_task_example(void *arg)
// {
//     uint32_t io_num;
//     static int cstate = 0;

//     for (;;)
//     {

//         if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
//         {

//             if (flag == true)
//             {
//                 flag = false;

//                 for (int i = 1; i < 10; i = i + 3)
//                 {
//                     if (store_status[i] == ESPNOW_DEVICE_ID)
//                     {
//                         cstate = store_status[i + 2];
//                         printf("store_status cstate: %d \n", cstate);
//                         break;
//                     }
//                 }

//                 if (io_num == GPIO_INPUT_IO_0)
//                 {
//                     int bit = getBit(cstate, 0);

//                     if (bit == 1)
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = clearBit(cstate, 1);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                     else
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = setBit(cstate, 1);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }

//                 }
//                 else if (io_num == GPIO_INPUT_IO_1)
//                 {
//                     int bit = getBit(cstate, 1);
//                     if (bit == 1)
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = clearBit(cstate, 2);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                     else
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = setBit(cstate, 2);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                 }
//                 else if (io_num == GPIO_INPUT_IO_2)
//                 {
//                     int bit = getBit(cstate, 2);
//                     if (bit == 1)
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = clearBit(cstate, 3);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                     else
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = setBit(cstate, 3);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                 }
//                 else if (io_num == GPIO_INPUT_IO_3)
//                 {
//                     int bit = getBit(cstate, 3);
//                     if (bit == 1)
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = clearBit(cstate, 4);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                     else
//                     {
//                         printf("setBit1: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                         cstate = setBit(cstate, 4);
//                         printf("setBit2: %d , bit: %d, io_num: %d \n", cstate, bit, io_num);
//                     }
//                 }

//                 for (int i = 1; i < 10; i = i + 3)
//                 {
//                     if (store_status[i] == ESPNOW_DEVICE_ID)
//                     {
//                         store_status[i + 2] = cstate;
//                         storeStatusToMemory();
                        
//                         break;
//                     }
//                 }
        
//                 printf("GPIO[%d] intr, val: %d cstate:%d \n", io_num, gpio_get_level(io_num), cstate);
//                 ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, 2500000));
//             }
//         }
//     }
//     vTaskDelete(NULL);
// }

static void do_operation()
{
    static int state = 0;
    while (1)
    { 
        ESP_LOGI(TAG, "write....... %d ", save_status);
        if (save_status)
        {
            save_status = false;
            storeStatusToMemory();
        }

        for (int i = 1; i < 10; i = i + 3)
        {
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
        
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

// static void oneshot_timer_callback(void *arg)
// {
//     int64_t time_since_boot = esp_timer_get_time();
//     ESP_LOGI(TAG, "One-shot timer called, time since boot: %lld us", time_since_boot);
//     flag = true;
// }

void gpio_init()
{

    gpio_config_t io_conf = {};
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

    // //interrupt of rising edge
    // io_conf.intr_type = GPIO_INTR_POSEDGE;
    // //bit mask of the pins, use GPIO4/5 here
    // io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // //set as input mode
    // io_conf.mode = GPIO_MODE_INPUT;
    // //enable pull-up mode
    // io_conf.pull_up_en = 1;
    // gpio_config(&io_conf);

    xTaskCreate(echo_task, "uart_echo_task", 4096, NULL, 10, NULL);

    //change gpio intrrupt type for one pin
   // gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);
    // gpio_set_intr_type(GPIO_INPUT_IO_1, GPIO_INTR_ANYEDGE);
    // gpio_set_intr_type(GPIO_INPUT_IO_2, GPIO_INTR_ANYEDGE);
    // gpio_set_intr_type(GPIO_INPUT_IO_3, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
   // gpio_evt_queue = xQueueCreate(1, sizeof(uint32_t));
    //start gpio task
    //xTaskCreate(gpio_task_example, "gpio_task_example", 4096, NULL, 10, NULL);
    

    // //install gpio isr service
    // gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // //hook isr handler for specific gpio pin
    // gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
    // //hook isr handler for specific gpio pin
    // gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *)GPIO_INPUT_IO_1);

    // gpio_isr_handler_add(GPIO_INPUT_IO_2, gpio_isr_handler, (void *)GPIO_INPUT_IO_2);
    // gpio_isr_handler_add(GPIO_INPUT_IO_3, gpio_isr_handler, (void *)GPIO_INPUT_IO_3); 
    
    // printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    // const esp_timer_create_args_t oneshot_timer_args = {
    //     .callback = &oneshot_timer_callback,
    //     .name = "one-shot"};

    // ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
}

#endif