#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <string.h>



static void receive_task(void *pvParameters)
{
    int sock = (int)pvParameters;

    int len;
    char rx_buffer[128];
    do
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            ESP_LOGE(TAG_H, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG_H, "Connection closed");
        }
        else
        {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG_H, "Received %d bytes: %s", len, rx_buffer);
        }
    } while (len > 0);

    shutdown(sock, 0);
    close(sock);

    vTaskDelete(NULL);
}

static void send_task(void *pvParameters)
{
    int sock = (int)pvParameters;
    int count = 100;
    char tx_buffer[10] = "counter ";

    while (count > 0)
    {
        vTaskDelay(3000 / portTICK_RATE_MS);
        count--;
        itoa(count, tx_buffer + 8, 10);
        int written = send(sock, tx_buffer, 10, 0);
        if (written < 0)
        {
            ESP_LOGE(TAG_H, "Error occurred during sending: errno %d", errno);
            break;
        }
    }
    
    shutdown(sock, 0);
    close(sock);
     
     vTaskDelete(NULL);

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
        ESP_LOGE(TAG_H, "Unable to create socket: errno %d", errno);
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

    ESP_LOGI(TAG_H, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG_H, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG_H, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG_H, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG_H, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {

        ESP_LOGI(TAG_H, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG_H, "Unable to accept connection: errno %d", errno);
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
        ESP_LOGI(TAG_H, "Socket accepted ip address: %s", addr_str);

        xTaskCreate(&send_task, "send_task", 2048, (void *)sock, 4, NULL);
       
        xTaskCreate(&receive_task, "receive_task", 2048, (void *)sock, 4, NULL);

    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

// static void do_retransmit(const int sock)
// {
//     int len;
//     char rx_buffer[10];

//     do
//     {
//         len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
//         if (len < 0)
//         {
//             ESP_LOGE(TAG_H, "Error occurred during receiving: errno %d", errno);
//         }
//         else if (len == 0)
//         {
//             ESP_LOGW(TAG_H, "Connection closed");
//         }
//         else
//         {
//             rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
//             ESP_LOGI(TAG_H, "Received %d bytes: %s", len, rx_buffer);
//             int *a = convert(rx_buffer);
//             rx_cmd[0] = a[0];
//             rx_cmd[1] = a[1];
//             rx_cmd[2] = a[2];

//             ESP_LOGI(TAG_H, "%d ", rx_cmd[0]);
//             ESP_LOGI(TAG_H, "%d ", rx_cmd[1]);
//             ESP_LOGI(TAG_H, "%d ", rx_cmd[2]);

//             free(a);
//             // send() can return less bytes than supplied length.
//             // Walk-around for robust implementation.
//             int to_write = len;
//             while (to_write > 0)
//             {
//                 int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
//                 if (written < 0)
//                 {
//                     ESP_LOGE(TAG_H, "Error occurred during sending: errno %d", errno);
//                 }
//                 to_write -= written;
//             }
//         }
//     } while (len > 0);
// }


#endif