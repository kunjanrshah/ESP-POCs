#ifndef STORAGE_INIT_H
#define STORAGE_INIT_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_crc.h"
#include <sys/param.h>
#include "freertos/task.h"
#include "esp_system.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_spiffs.h"


static void storeMS51StatusToMemory(uint8_t *data)
{
    int a1=sizeof(data);
    int a2=sizeof(data[0]);
    int n = (int)(a1 / a2);
    int len=n*2;
    char str[len];
        
    unsigned char *pin = data;
    const char *hex = "0123456789ABCDEF";
    char *pout = str;
    int i = 0;
    for (; i < n-1; ++i)
    {
        *pout++ = hex[(*pin >> 4) & 0xF];
        *pout++ = hex[(*pin++) & 0xF];
    }
    *pout++ = hex[(*pin >> 4) & 0xF];
    *pout++ = hex[(*pin) & 0xF];
    *pout = 0;
    
    FILE *f = fopen("/spiffs/ms51_data.txt", "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, str);
    fclose(f);
}

static uint8_t *getMS51StatusFromMemory()
{
    int n=10;
    int len=n*2;
    uint8_t *tx_buffer = malloc(sizeof(uint8_t) * len);
    memset(tx_buffer, 0, sizeof(uint8_t));
    for (int i = 0; i < n; i++)
    {
        tx_buffer[i] = 0x00;
    }
    
    // Open renamed file for reading
   // ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen("/spiffs/ms51_data.txt", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        storeMS51StatusToMemory(tx_buffer);
        return tx_buffer;
    }
    char line[len+1];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "getMS51StatusFromMemory: Read from file: '%s'", line);
    
    // Convert String to Integer array
    int i = 0;
    char tmp[3];
    tmp[2] = '\0';
    uint8_t len_buffer = 0;

    for (i = 0; i < strlen(line); i += 2)
    {
        tmp[0] = line[i];
        tmp[1] = line[i + 1];
        tx_buffer[len_buffer] = strtol(tmp, NULL, 16);
        len_buffer++;
    }

    ESP_LOGI(TAG, "Read line lenght: '%d'", len_buffer);

    for (i = 0; i < len_buffer; i++)
    {
        ESP_LOGI(TAG, "'%d' ", tx_buffer[i]);
    }
    return tx_buffer;
}

static void storeStatusToMemory()
{
    /* target buffer should be large enough */
    int len=n_status*2;
    char str[len];
    ESP_LOGI(TAG, "storeStatusToMemory: store_status array value before store");
    for (int i = 0; i < n_status; i++)
    {
        printf("%d ", store_status[i]);
    }

    unsigned char *pin = store_status;
    const char *hex = "0123456789ABCDEF";
    char *pout = str;
    int i = 0;
    for (; i < n_status-1; ++i)
    {
        *pout++ = hex[(*pin >> 4) & 0xF];
        *pout++ = hex[(*pin++) & 0xF];
    }
    *pout++ = hex[(*pin >> 4) & 0xF];
    *pout++ = hex[(*pin) & 0xF];
    *pout = 0;

    FILE *f = fopen("/spiffs/broadcast_data.txt", "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, str);
    fclose(f);
}

static uint8_t *getStatusFromMemory()
{
    int len=n_status*2;
    uint8_t *tx_buffer = malloc(sizeof(uint8_t) * len);
    memset(tx_buffer, 0, sizeof(uint8_t));
    for (int i = 0; i < n_status; i++)
    {
        tx_buffer[i] = 0x00;
    }
    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen("/spiffs/broadcast_data.txt", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return tx_buffer;
    }
    char line[len+1];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "getStatusFromMemory: Read from file: '%s'", line);

    // Convert String to Integer array
    int i = 0;
    char tmp[3];
    tmp[2] = '\0';
    uint8_t len_buffer = 0;

    for (i = 0; i < strlen(line); i += 2)
    {
        tmp[0] = line[i];
        tmp[1] = line[i + 1];
        tx_buffer[len_buffer] = strtol(tmp, NULL, 16);
        len_buffer++;
    }

    ESP_LOGI(TAG, "Read line lenght: '%d'", len_buffer);

    for (i = 0; i < len_buffer; i++)
    {
        ESP_LOGI(TAG, "'%d' ", tx_buffer[i]);
    }
    return tx_buffer;
}


void SPIFFS_Init(){

    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

#endif