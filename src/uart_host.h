//
// Created by shubin on 18.06.2020.
//

#ifndef WIFI_MODULE_UART_HOST_H
#define WIFI_MODULE_UART_HOST_H

#include "../sdk/gpio.h"

#define ESP_PROTOC_HEAD				(unsigned char)0xa5
#define ESP_PROTOC_TAIL				(unsigned char)0xfc

#define ESP_CONNECTION_STATE_CONNECTED			            0x0A
#define ESP_CONNECTION_STATE_ERROR			                0x0E
#define ESP_CONNECTION_STATE_UNCONFIGURED                   0x0F



#define ESP_MSG_ID_SET_CONNECTION_CONFIG    0
#define ESP_MSG_ID_CONFIG_STATE	            0
#define ESP_MSG_ID_GCODE                    1
#define ESP_MSG_ID_GCODE_RESP               2
#define ESP_TYPE_FILE_FIRST                 2
#define ESP_TYPE_FILE_FRAGMENT              3
#define ESP_MSG_ID_WIFI_SCAN_RESP		    4
#define ESP_MSG_ID_WIFI_SCAN_REQ		    7
#define ESP_TYPE_DEBUG                      0x10
#define ESP_TYPE_PING                       0x11
#define ESP_START_UPLOAD                    0x12
#define ESP_UPLOAD                          0x13

#define USED_UART                   0

#define LOOPBACK_PIN    4
#define uart_loopback_state() (GPIO_REG_READ(GPIO_IN_ADDRESS) & (1<<(LOOPBACK_PIN)))


typedef struct {
    unsigned char command;
    unsigned short size;
    unsigned char is_dynamic_memory;
    unsigned char * message;
} UartSendMessage;

extern volatile unsigned char tx_has_data;

void init_uart();

void uart_sync_dma(unsigned char v);
void uart_tx(unsigned char c);
void uart_message_async(UartSendMessage * uartSendMessage);
void uart_out(UartSendMessage * uartSendMessage);

void uart_start_message(unsigned char cmd, unsigned short len);
void uart_message(const unsigned char * msg, unsigned short len);
void uart_end_message();
void uart_wait_empty(unsigned char limit);

void uart_debug_message(const unsigned char * msg);
void uart_data_send(const unsigned char * msg, unsigned short len, unsigned char cmd);

#endif //WIFI_MODULE_UART_HOST_H
