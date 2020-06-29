//
// Created by shubin on 18.06.2020.
//

#include "loop.h"
#include "user_config.h"
#include "../sdk/c_types.h"
#include "../sdk/gpio.h"
#include "../sdk/user_interface.h"
#include "../sdk/os_type.h"
#include "../sdk/osapi.h"
#include "../sdk/ets_sys.h"
#include "../sdk/mem.h"
#include "uart_host.h"
#include "eeprom.h"
#include "wifi.h"
#include "bridge.h"
#include "http_server.h"

#define TASK_QUEUE_LEN    10
static os_event_t task_queue[TASK_QUEUE_LEN];

static unsigned char _return_to_softap_after_scan = 0;

static os_timer_t _ping_timer;

static void ICACHE_FLASH_ATTR on_ping_timer(void *arg) {
    system_os_post(0, MESSAGE_PING, 0);
}

void ICACHE_FLASH_ATTR _wifi_done_scan(void * arg, STATUS status) {
    if (status == OK) {
        UartSendMessage * uartSendMessage = (UartSendMessage *)os_zalloc(sizeof(UartSendMessage));
        uartSendMessage->command = ESP_MSG_ID_WIFI_SCAN_RESP;
        uartSendMessage->is_dynamic_memory = 1;
        struct bss_info *bss_link = (struct bss_info *)arg;
        unsigned char cnt = 0;
        uartSendMessage->size = 1;
        while (bss_link) {
            cnt++;
            uartSendMessage->size++;
            unsigned char i = 0;
            while ((i<32) && (bss_link->ssid[i]!=0))
                i++;
            uartSendMessage->size+=i;
            uartSendMessage->size++;
            bss_link = bss_link->next.stqe_next;
        }

        uartSendMessage->message = (unsigned char *)os_malloc(uartSendMessage->size);

        bss_link = (struct bss_info *)arg;
        unsigned short idx = 0;
        uartSendMessage->message[idx++] = cnt;
        while (bss_link) {
            unsigned short sz_pos = idx++;
            unsigned char i = 0;
            while ((i<32) && (bss_link->ssid[i]!=0))
                uartSendMessage->message[idx++] = bss_link->ssid[i++];
            uartSendMessage->message[sz_pos] = i;
            uartSendMessage->message[idx++] = bss_link->rssi;
            bss_link = bss_link->next.stqe_next;
        }
        if (_return_to_softap_after_scan) {
            wifi_set_opmode_current(SOFTAP_MODE);
            _return_to_softap_after_scan = 0;
        }
        uart_message_async(uartSendMessage);
    }
}

void ICACHE_FLASH_ATTR _wifi_state_response(unsigned char wifistate) {
    UartSendMessage * uartSendMessage = (UartSendMessage *)os_zalloc(sizeof(UartSendMessage));
    uartSendMessage->command = ESP_MSG_ID_CONFIG_STATE;
    uartSendMessage->is_dynamic_memory = 1;
    uartSendMessage->message = (unsigned char *)os_zalloc(256);
    unsigned short index = 0;
    struct softap_config softap_config;
    struct station_config station_config;
    unsigned char * sid = 0;
    unsigned char * key = 0;
    unsigned char mode = 0;
    unsigned char state = 0;
    switch(wifi_get_opmode()) {
        case SOFTAP_MODE: {
            wifi_softap_get_config(&softap_config);
            sid = softap_config.ssid;
            key = softap_config.password;
            uartSendMessage->message[index++] = 192;
            uartSendMessage->message[index++] = 168;
            uartSendMessage->message[index++] = 4;
            uartSendMessage->message[index++] = 1;
            mode = 1;
            state = ESP_CONNECTION_STATE_CONNECTED;
            break;
        }
        case STATION_MODE: {
            wifi_station_get_config(&station_config);
            sid = station_config.ssid;
            key = station_config.password;
            struct ip_info ip_info;
            wifi_get_ip_info(STATION_IF, &ip_info);
            *(struct ip_addr *)(uartSendMessage->message) = ip_info.ip;
            index+=4;
            mode = 2;
            switch (wifistate) {
                case EVENT_STAMODE_CONNECTED:
                case EVENT_STAMODE_GOT_IP:
                    state = ESP_CONNECTION_STATE_CONNECTED;
                    break;
                default:
                    state = 0x00;
                    break;
            }
            break;
        }
    }
    index += 2;
    uartSendMessage->message[index++] = state;
    uartSendMessage->message[index++] = mode;
    unsigned long stored_pos;
    unsigned char counter;

    if (sid) {
        stored_pos = index++;
        for (counter = 0; counter < 32; counter++) {
            if (sid[counter])
                uartSendMessage->message[index++] = sid[counter];
            else
                break;
        }
        uartSendMessage->message[stored_pos] = counter;
    } else
        index++;
    if (key) {
        stored_pos = index++;
        for (counter = 0; counter < 64; counter++) {
            if (key[counter])
                uartSendMessage->message[index++] = key[counter];
            else
                break;
        }
        uartSendMessage->message[stored_pos] = counter;
    } else
        index++;

    index++;//cloud_state;
    index++;//cloud_name_len = 0;
    index+=2;//cloud-port
    uartSendMessage->message[index++] = 20;
    for (counter = 0; counter<20;counter++)
        uartSendMessage->message[index++] = 'F';
    const char * version = VERSION;
    counter = os_strlen(version);
    uartSendMessage->message[index++] = counter;
    os_strncpy(&uartSendMessage->message[index], version, counter);
    index+=counter; //version_len;
    uartSendMessage->size = index;
    uart_message_async(uartSendMessage);
}

static int uart_div = 10;

static void ICACHE_FLASH_ATTR task_proc(os_event_t *events) {
    switch (events->sig) {
        case MESSAGE_FROM_UART:
            if (events->par) {
                unsigned char * buf = (unsigned char*)events->par;
                switch (buf[0]) {
                    case ESP_MSG_ID_SET_CONNECTION_CONFIG: {
                        unsigned char index = 1;
                        ShortLength sl;
                        sl.c[0] = buf[index++];
                        sl.c[1] = buf[index++];
                        os_memset(&eeprom->wifi_client, 0, sizeof(eeprom->wifi_client));
                        unsigned char mode = buf[index++];
                        switch (mode) {
                            case 1: mode = SOFTAP_MODE;
                                uart_debug_message("configured as SOFTAP");
                                break;
                            case 2: mode = STATION_MODE;
                                uart_debug_message("configured as STATION");
                                break;
                        }
                        eeprom->wifi_client.mode = mode;
                        unsigned char len = buf[index++];
                        os_strncpy(eeprom->wifi_client.ssid, &buf[index], len);
                        index = index+len;
                        len = buf[index++];
                        os_strncpy(eeprom->wifi_client.password, &buf[index], len);
                        eeprom_save();
                        wifi_set_opmode_current(NULL_MODE);
                        wifi_init();
                        break;
                    }
                    case ESP_MSG_ID_GCODE_RESP: {
                        if (bridge_out(buf))
                            events->par = 0;
                        break;
                    }
                    case ESP_MSG_ID_WIFI_SCAN_REQ: {
                        if (wifi_get_opmode() == SOFTAP_MODE) {
                            _return_to_softap_after_scan = 1;
                            wifi_set_opmode_current(STATION_MODE);
                        }
                        wifi_station_scan(0, _wifi_done_scan);
                        break;
                    }
                    case 9: {
                        uart_debug_message("Save config");
                        break;
                    }
                }
                if (events->par)
                    os_free(events->par);
            }
            break;
        case MESSAGE_SEND_TO_UART: {
            uart_debug_message("uart async now");
            uart_out((UartSendMessage *)events->par);
            break;

        }
        case MESSAGE_PING:
            if (_bridge_client)
                uart_data_send("M991\n", 5, ESP_MSG_ID_GCODE);
            else
                uart_data_send(0, 0, ESP_TYPE_PING);
            break;
        case MESSAGE_WIFI_STATUS: {
            switch (events->par) {
                case EVENT_STAMODE_CONNECTED:
                    uart_debug_message("event: connected");
                    break;
                case EVENT_STAMODE_GOT_IP:
                    uart_debug_message("event: got ip");
                    _wifi_state_response(events->par);
                    os_timer_disarm(&_ping_timer);
                    os_timer_setfn(&_ping_timer, (os_timer_func_t *)on_ping_timer, 0);
                    os_timer_arm(&_ping_timer, 2000, 1);
                    bridge_start();
                    http_server_start();
                    break;
                case EVENT_STAMODE_DISCONNECTED:
                    uart_debug_message("event: disconnected");
                    _wifi_state_response(events->par);
                    os_timer_disarm(&_ping_timer);
                    bridge_stop();
                    http_server_stop();
                    break;
                case EVENT_STAMODE_AUTHMODE_CHANGE:
                    uart_debug_message("event: auth mode change");
                    break;
                case EVENT_SOFTAPMODE_STACONNECTED:
                    uart_debug_message("event: station connected");
                    break;
                case EVENT_SOFTAPMODE_STADISCONNECTED:
                    uart_debug_message("event: station disconnected");
                    break;
                case EVENT_MAX:
                    uart_debug_message("event: max");
                    break;
            }
            break;
        }

        case MESSAGE_INIT: {
            uart_debug_message("module initialization");
            eeprom_load();
            wifi_init();
            break;
        }

    }
}


void init_loop() {
    os_memset(&_ping_timer, 0, sizeof(_ping_timer));
    system_os_task(task_proc, 0, task_queue, TASK_QUEUE_LEN);
    system_os_post(0, MESSAGE_INIT, 0);
}

//Start os task
