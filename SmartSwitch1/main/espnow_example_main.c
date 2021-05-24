
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
#include "espnow_example.h"
#include <sys/param.h>
#include "freertos/task.h"
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
#include "freertos/event_groups.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"

static const char *TAG = "espnow_example";
static xQueueHandle s_example_espnow_queue;
static uint8_t device_mac_addr[6] = {0};
static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t rx_cmd[] = {0x0, 0x00}; //{0};
static uint8_t *store_status = {0};
static int state = 0;
static bool save_status = false;
static void example_espnow_deinit(example_espnow_send_param_t *send_param);

#include "utils.h"
#include "wifi_init.h"
#include "tcp_server.h"
#include "storage.h"
#include "gpio_task.h"


#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

static const char *TAG_OTA = "advanced_https_ota_example";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG_OTA, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG_OTA, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

void advanced_ota_example_task(void *pvParameter)
{
    ESP_LOGI(TAG_OTA, "Starting Advanced OTA example");

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
    };

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG_OTA, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "ESP HTTPS OTA Begin failed");
        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "image header verification failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG_OTA, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG_OTA, "Complete data was not received.");
    }

ota_end:
    ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        ESP_LOGI(TAG_OTA, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG_OTA, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG_OTA, "ESP_HTTPS_OTA upgrade failed %d", ota_finish_err);
        vTaskDelete(NULL);
    }
}





/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL)
    {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
int example_espnow_data_parse(example_espnow_send_param_t *send_param, uint8_t *mac_addr, uint8_t *data, uint16_t data_len)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    ESP_LOGI(TAG, "Receive  data_len: %d, sizeof: %d", data_len, sizeof(example_espnow_data_t));
    if (data_len < sizeof(example_espnow_data_t))
    {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    int n_status = (int)(sizeof(send_param->seq_status) / sizeof(send_param->seq_status[0]));
    if (buf->seq_status != NULL)
    {
        for (int i = 0; i < n_status; i++)
        {
            send_param->seq_status[i] = buf->seq_status[i];
            ESP_LOGI(TAG, "Receiving:  seq_status: %d", buf->seq_status[i]);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Receiving:  seq_status:  NULL");
    }

    if (buf->seq_cmd != NULL)
    {
        for (int i = 0; i < 2; i++)
        {
            send_param->seq_cmd[i] = buf->seq_cmd[i];
            ESP_LOGI(TAG, "Receiving:  seq_cmd: %d", buf->seq_cmd[i]);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Receiving:  seq_cmd: NULL");
    }

    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc)
    {
        return 1;
    }

    return 0;
}

/* Prepare ESPNOW data to be sent. */
void example_espnow_data_prepare(example_espnow_send_param_t *send_param)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(example_espnow_data_t));

    buf->crc = 0;
    int dev_id = 1;
    int count = 0;
    int n_status = (int)(sizeof(send_param->seq_status) / sizeof(send_param->seq_status[0]));

    for (int i = 0; i < n_status; i++)
    {
        if (send_param->seq_status[i] == 0)
        {
            count++;
        }
    }

    if (count == n_status)
    {

        int connected = 0;
        if (IS_CONNECTED)
        {
            connected = setBit(connected, ESPNOW_DEVICE_ID);
        }
        else
        {
            connected = clearBit(connected, ESPNOW_DEVICE_ID);
        }
        buf->seq_status[0] = connected;

        for (int i = 1; i < n_status; i++)
        {
            if (dev_id == ESPNOW_DEVICE_ID)
            {
                buf->seq_status[i] = dev_id;
                buf->seq_status[++i] = ESPNOW_SWITCH;
                if (rx_cmd[0] == ESPNOW_DEVICE_ID)
                {
                    buf->seq_status[++i] = rx_cmd[1];
                    for (int j = 0; j < 2; j++)
                    {
                        rx_cmd[j] = 0;
                    }
                }
                else
                {
                    //TODO: Get Hardware Status from cache. Initially status would be 0.
                    buf->seq_status[++i] = 0;
                }
            }
            else
            {
                //TODO: Get Hardware Status from cache. Initially status would be 0.
                buf->seq_status[i] = dev_id;
                buf->seq_status[++i] = 0;
                buf->seq_status[++i] = 0;
                //i++;
                //i++;
            }
            dev_id++;
        }
        for (int j = 0; j < 2; j++)
        {
            buf->seq_cmd[j] = rx_cmd[j];
            send_param->seq_cmd[j] = rx_cmd[j];
        }
    }
    else if (count != n_status)
    {
        for (int i = 0; i < n_status; i++)
        {
            buf->seq_status[i] = send_param->seq_status[i];
        }

        int connected = buf->seq_status[0];
        if (IS_CONNECTED)
        {
            connected = setBit(connected, ESPNOW_DEVICE_ID);
        }
        else
        {
            connected = clearBit(connected, ESPNOW_DEVICE_ID);
        }
        buf->seq_status[0] = connected;
    
        for (int i = 1; i < n_status; i = i + 3)
        {
            if (buf->seq_status[i] == ESPNOW_DEVICE_ID)
            {
                buf->seq_status[i + 1] = ESPNOW_SWITCH;
                if (send_param->seq_cmd[0] == ESPNOW_DEVICE_ID || rx_cmd[0] == ESPNOW_DEVICE_ID)
                {
                    //TODO: Change Hardware Status
                    //TODO: Get Hardware Status
                    if (rx_cmd[0] == ESPNOW_DEVICE_ID)
                    {
                        buf->seq_status[i + 2] = rx_cmd[1];
                        for (int j = 0; j < 2; j++)
                        {
                            rx_cmd[j] = 0;
                            buf->seq_cmd[j] = 0;
                        }
                    }
                    else if (send_param->seq_cmd[0] == ESPNOW_DEVICE_ID)
                    {
                        buf->seq_status[i + 2] = send_param->seq_cmd[1];
                        for (int j = 0; j < 2; j++)
                        {
                            send_param->seq_cmd[j] = 0;
                            buf->seq_cmd[j] = 0;
                        }
                    }
                }
                else
                {
                    //TODO: Get Hardware Status
                    buf->seq_status[i + 2] = store_status[i + 2];
                    if(rx_cmd[0]!=0){
                        send_param->seq_cmd[0] = rx_cmd[0];
                        send_param->seq_cmd[1] = rx_cmd[1];
                    }
                    
                    buf->seq_cmd[0] = send_param->seq_cmd[0];
                    buf->seq_cmd[1] = send_param->seq_cmd[1];
                }

                break;
            }
            else
            {
                if (rx_cmd[0] == i)
                {
                    if (rx_cmd[1] == buf->seq_status[i + 2])
                    {
                        for (int j = 0; j < 2; j++)
                        {
                            rx_cmd[j] = 0;
                            buf->seq_cmd[j] = 0;
                        }
                    }
                }
            }
        }
    }
    
    save_status = false;
    for (int i = 0; i < 10; i++)
    {
        if (store_status[i] != buf->seq_status[i])
        {
            save_status = true;
            break;
        }
    }
   
    if (save_status)
    {
        for (int i = 0; i < 10; i++)
        {
            store_status[i] = buf->seq_status[i];
        }
    }

    for (int i = 0; i < n_status; i++)
    {
       // send_param->seq_status[i] = buf->seq_status[i];
        ESP_LOGI(TAG, "Sending Buffer Status: %d", buf->seq_status[i]);
    }
    for (int i = 0; i < 2; i++)
    {
        ESP_LOGI(TAG, "Sending Buffer Command: %d", buf->seq_cmd[i]);
    }

    //esp_fill_random(buf->payload, send_param->len - sizeof(example_espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}


static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;

    vTaskDelay(5000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;

    if (esp_now_send(send_param->broadcast_mac, send_param->buffer, send_param->len) != ESP_OK)
    {
        ESP_LOGE(TAG, "Send error");
        example_espnow_deinit(send_param);
        vTaskDelete(NULL);
    }

    while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case EXAMPLE_ESPNOW_SEND_CB:
        {

            example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

            /* Delay a while before sending the next data. */
            if (send_param->delay > 0)
            {
                vTaskDelay(send_param->delay / portTICK_RATE_MS);
            }

            //ESP_LOGI(TAG, "send data to " MACSTR "", MAC2STR(send_cb->mac_addr));

            memcpy(send_param->broadcast_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
            example_espnow_data_prepare(send_param);

            /* Send the next data after the previous data is sent. */
            if (esp_now_send(send_param->broadcast_mac, send_param->buffer, send_param->len) != ESP_OK)
            {
                ESP_LOGE(TAG, "Send error");
                example_espnow_deinit(send_param);
                vTaskDelete(NULL);
            }

            break;
        }
        case EXAMPLE_ESPNOW_RECV_CB:
        {
            example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

            example_espnow_data_parse(send_param, recv_cb->mac_addr, recv_cb->data, recv_cb->data_len);
            free(recv_cb->data);

            ESP_LOGI(TAG, "Receive data from: " MACSTR ", len: %d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

            break;
        }
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

static esp_err_t example_espnow_init(void)
{
    example_espnow_send_param_t *send_param;

    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_example_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(example_espnow_send_param_t));
    memset(send_param, 0, sizeof(example_espnow_send_param_t));
    if (send_param == NULL)
    {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }

    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);

    if (send_param->buffer == NULL)
    {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->broadcast_mac, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);

    for (int i = 0; i < 10; i++)
    {
        send_param->seq_status[i] = store_status[i];
    }

    send_param->seq_cmd[0] = rx_cmd[0];
    send_param->seq_cmd[1] = rx_cmd[1];

    example_espnow_data_prepare(send_param);

    xTaskCreate(example_espnow_task, "example_espnow_task", 2048, send_param, 4, NULL);

    do_operation();

    return ESP_OK;
}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}


void app_main(void)
{

    //Get MAC address for WiFi Station interface
    ESP_ERROR_CHECK(esp_read_mac(device_mac_addr, ESP_MAC_WIFI_STA));
    ESP_LOGI("WIFI_STA MAC", "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", device_mac_addr[0], device_mac_addr[1], device_mac_addr[2], device_mac_addr[3], device_mac_addr[4], device_mac_addr[5]);

    SPIFFS_Init();
    
    wifi_init_softap();

    gpio_init();
   
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
     
    store_status = (uint8_t *)malloc(sizeof(uint8_t) * 10);
    memset(store_status, 0, sizeof(uint8_t) * 10);

    store_status = getStatusFromMemory();

    for (int i = 0; i < 10; i++)
    {
        ESP_LOGE(TAG, "get from memory ==> store_status %d", store_status[i]);
    }

    example_espnow_init();
    
    xTaskCreate(&advanced_ota_example_task, "advanced_ota_example_task", 1024 * 8, NULL, 5, NULL);
}