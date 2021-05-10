/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
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
#include "esp_system.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "utils.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_spiffs.h"

/******************************Start Keyboard code****************************/
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
/*******************************End Keyboard code***************************/

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN
#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "espnow_example";
static bool IS_CONNECTED = false;
static xQueueHandle s_example_espnow_queue;
static uint8_t device_mac_addr[6] = {0};
static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t rx_cmd[3] = {0x2, 0x1, 0x03}; //{0};
static uint8_t *store_status = {0};
static int state = 0;
static bool save_status = false;
static void example_espnow_deinit(example_espnow_send_param_t *send_param);
static void storeStatusToMemory();

// Function to set the kth bit of n
int setBit(int n, int k)
{
    return (n | (1 << (k - 1)));
}

// Function to return kth bit on n
int getBit(int n, int k)
{
    return ((n >> k) & 1);
}

// Function to clear the kth bit of n
int clearBit(int n, int k)
{
    return (n & (~(1 << (k - 1))));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        IS_CONNECTED = true;
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        IS_CONNECTED = false;
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

int *convert(char *c)
{
    int len = strlen(c), i;
    int *a = (int *)malloc(len * sizeof(int));
    for (i = 0; i < len; i++)
        a[i] = c[i] - 48;
    return a;
}

static void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[10];

    do
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
            int *a = convert(rx_buffer);
            rx_cmd[0] = a[0];
            rx_cmd[1] = a[1];
            rx_cmd[2] = a[2];

            ESP_LOGI(TAG, "%d ", rx_cmd[0]);
            ESP_LOGI(TAG, "%d ", rx_cmd[1]);
            ESP_LOGI(TAG, "%d ", rx_cmd[2]);

            free(a);
            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0)
            {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0)
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
    else if (addr_family == AF_INET6)
    {
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.ss_family == PF_INET6)
        {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
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
            ESP_LOGI(TAG, "Receiving:  seq_status: %d\n", buf->seq_status[i]);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Receiving:  seq_status:  NULL");
    }

    if (buf->seq_cmd != NULL)
    {
        for (int i = 0; i < 3; i++)
        {
            send_param->seq_cmd[i] = buf->seq_cmd[i];
            ESP_LOGI(TAG, "Receiving:  seq_cmd: %d\n", buf->seq_cmd[i]);
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
                    buf->seq_status[++i] = rx_cmd[2];
                    for (int j = 0; j < 3; j++)
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
        for (int j = 0; j < 3; j++)
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
                        buf->seq_status[i + 2] = rx_cmd[2];
                        for (int j = 0; j < 3; j++)
                        {
                            rx_cmd[j] = 0;
                            buf->seq_cmd[j] = 0;
                        }
                    }
                    else if (send_param->seq_cmd[0] == ESPNOW_DEVICE_ID)
                    {
                        buf->seq_status[i + 2] = send_param->seq_cmd[2];
                        for (int j = 0; j < 3; j++)
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

                    buf->seq_cmd[0] = send_param->seq_cmd[0];
                    buf->seq_cmd[1] = send_param->seq_cmd[1];
                    buf->seq_cmd[2] = send_param->seq_cmd[2];
                }

                break;
            }
            else
            {
                if (rx_cmd[0] == i)
                {
                    if (rx_cmd[2] == buf->seq_status[i + 2])
                    {
                        for (int j = 0; j < 3; j++)
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
        // ESP_LOGI(TAG, "store_status & send_param %d = %d", store_status[i], send_param->seq_status[i]);
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
    for (int i = 0; i < 3; i++)
    {
        ESP_LOGI(TAG, "Sending Buffer Command: %d", buf->seq_cmd[i]);
    }

    //esp_fill_random(buf->payload, send_param->len - sizeof(example_espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
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

static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;

    vTaskDelay(10000 / portTICK_RATE_MS);
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

            ESP_LOGI(TAG, "send data to " MACSTR "", MAC2STR(send_cb->mac_addr));

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
    send_param->seq_cmd[2] = rx_cmd[2];

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

static void storeStatusToMemory()
{
    /* target buffer should be large enough */
    char str[20];
    ESP_LOGI(TAG, "store_status array value before store");
    for (int i = 0; i < 10; i++)
    {
        printf("%d\n", store_status[i]);
    }

    unsigned char *pin = store_status;
    const char *hex = "0123456789ABCDEF";
    char *pout = str;
    int i = 0;
    for (; i < 9; ++i)
    {
        *pout++ = hex[(*pin >> 4) & 0xF];
        *pout++ = hex[(*pin++) & 0xF];
    }
    *pout++ = hex[(*pin >> 4) & 0xF];
    *pout++ = hex[(*pin) & 0xF];
    *pout = 0;

    ESP_LOGI(TAG, "Opening file %s\n", str);
    FILE *f = fopen("/spiffs/smart_switch.txt", "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, str);
    fclose(f);
    ESP_LOGI(TAG, "File written");
}

static uint8_t *getStatusFromMemory()
{
    uint8_t *tx_buffer = malloc(sizeof(uint8_t) * 20);
    memset(tx_buffer, 0, sizeof(uint8_t));
    for (int i = 0; i < 10; i++)
    {
        tx_buffer[i] = 0x00;
    }
    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE *f = fopen("/spiffs/smart_switch.txt", "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return tx_buffer;
    }
    char line[21];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

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

void app_main(void)
{

    //Get MAC address for WiFi Station interface
    ESP_ERROR_CHECK(esp_read_mac(device_mac_addr, ESP_MAC_WIFI_STA));
    ESP_LOGI("WIFI_STA MAC", "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", device_mac_addr[0], device_mac_addr[1], device_mac_addr[2], device_mac_addr[3], device_mac_addr[4], device_mac_addr[5]);

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

    wifi_init_softap();

    /**************************Start Keyboard *************************/
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

   

    /***************************End Keyboard**************************/




    //xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
    store_status = (uint8_t *)malloc(sizeof(uint8_t) * 10);
    memset(store_status, 0, sizeof(uint8_t) * 10);

    store_status = getStatusFromMemory();

    for (int i = 0; i < 10; i++)
    {
        ESP_LOGE(TAG, "get from memory ==> store_status %d", store_status[i]);
    }

    example_espnow_init();


}