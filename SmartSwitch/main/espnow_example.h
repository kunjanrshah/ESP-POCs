/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef ESPNOW_EXAMPLE_H
#define ESPNOW_EXAMPLE_H

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif


#define CONFIG_ESP_WIFI_SSID "myssid1"
#define CONFIG_ESP_WIFI_PASSWORD "mypassword1"
#define CONFIG_ESP_WIFI_CHANNEL 1
#define CONFIG_ESP_MAX_STA_CONN 4
#define CONFIG_EXAMPLE_PORT 3333
#define ESPNOW_MAXDELAY 512
#define ESPNOW_STATUS_STR_SIZE     100
#define ESPNOW_QUEUE_SIZE          6
#define ESPNOW_DEVICE_ID           1
#define ESPNOW_SWITCH              1

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} example_espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} example_espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} example_espnow_event_recv_cb_t;

typedef union {
    example_espnow_event_send_cb_t send_cb;
    example_espnow_event_recv_cb_t recv_cb;
} example_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    example_espnow_event_id_t id;
    example_espnow_event_info_t info;
} example_espnow_event_t;

/* User defined field of ESPNOW data in this example. */
typedef struct {
    uint16_t crc;                         //CRC16 value of ESPNOW data.
    uint8_t seq_status[10]; 
    uint8_t seq_cmd[3];
} __attribute__((packed)) example_espnow_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
    uint8_t seq_status[10];
    uint8_t seq_cmd[3]; 
    uint16_t delay;                       //Delay between sending two ESPNOW data, unit: ms.
    int len;                                 //Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
    uint8_t broadcast_mac[ESP_NOW_ETH_ALEN];   //MAC address of end device which is in the range.
} example_espnow_send_param_t;

#endif
