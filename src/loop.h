//
// Created by shubin on 18.06.2020.
//

#ifndef WIFI_MODULE_LOOP_H
#define WIFI_MODULE_LOOP_H

typedef enum {
    MESSAGE_INIT,
    MESSAGE_FROM_UART,
    MESSAGE_SEND_TO_UART,
    MESSAGE_WIFI_CONNECTED,
    MESSAGE_WIFI_DISCONNECTED,
    MESSAGE_WIFI_STATUS,
    MESSAGE_PING,
    MESSAGE_HTTP_ROOT,
    MESSAGE_HTTP_UPLOAD,
    MESSAGE_UART_TEST,
    MESSAGE_NTP_LOADED,
    MESSAGE_NTP_ERROR,
} MESSAGE_TYPE;

int uart_tx_buffer_queue_len();
void init_loop();

#endif //WIFI_MODULE_LOOP_H
