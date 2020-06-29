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
#include "../sdk/espconn.h"
#include "uart_host.h"
#include "eeprom.h"
#include "wifi.h"

#include "bridge.h"

static struct espconn _bridge_conn;
struct espconn * _bridge_client = 0;
static esp_tcp _bridge_proto;
static unsigned char _bridge_buffer[1024];
static unsigned short _bridge_buffer_index;
static uint32_t _bridge_keepalive;

void ICACHE_FLASH_ATTR  _bridge_on_recv(struct espconn *connection, char *pdata, unsigned short len) {
    unsigned short i = 0;
    while (i<len) {
        unsigned char b = pdata[i++];
        if ((_bridge_buffer_index == 0) && ((b == ' ') || (b == '\r') || (b == '\n')))
            continue;
        if ((b == '\n') || (b == '\r')) {
            uart_data_send(_bridge_buffer, _bridge_buffer_index, ESP_MSG_ID_GCODE);
            _bridge_buffer_index = 0;
        } else {
            _bridge_buffer[_bridge_buffer_index++] = b;
            if (_bridge_buffer_index>=sizeof(_bridge_buffer) - 1)
                espconn_disconnect(connection);
        }
    }
}

struct SEND_QUEUE;

typedef struct{
    unsigned char * message;
    void * next;
} SEND_QUEUE;

SEND_QUEUE * sendQueue = 0;

void  ICACHE_FLASH_ATTR bridge_release() {
    while (sendQueue) {
        SEND_QUEUE * next = (SEND_QUEUE *)sendQueue->next;
        os_free(sendQueue->message);
        os_free(sendQueue);
        sendQueue = next;
    }
}

unsigned ICACHE_FLASH_ATTR char _bridge_out() {
    if (sendQueue) {
        unsigned short sz = sendQueue->message[1] | (sendQueue->message[2] << 8);
        espconn_sent(_bridge_client, &sendQueue->message[3], sz);
    }
}

unsigned char bridge_out(char * data) {
    if (_bridge_client) {
        SEND_QUEUE * q = (SEND_QUEUE *)os_zalloc(sizeof(SEND_QUEUE));
        q->message = data;
        if (sendQueue) {
            sendQueue->next = q;
        } else {
            sendQueue = q;
            _bridge_out();
        }
        return 1;
    };
    return 0;
}

void ICACHE_FLASH_ATTR  _bridge_on_sent(struct espconn *current) {
    SEND_QUEUE * next = (SEND_QUEUE *)sendQueue->next;
    os_free(sendQueue->message);
    os_free(sendQueue);
    sendQueue = next;
    _bridge_out();
}

void ICACHE_FLASH_ATTR  _bridge_on_recon(struct espconn *current, sint8 err) {
}

void ICACHE_FLASH_ATTR  _bridge_on_discon(struct espconn *current) {
    bridge_release();
    uart_debug_message("bridge client disconnected");
    _bridge_client = 0;
}


void ICACHE_FLASH_ATTR  _bridge_on_connect(struct espconn *current) {
    _bridge_buffer_index = 0;
    _bridge_client = current;
    //espconn_set_keepalive(current)
    espconn_regist_recvcb(current, (espconn_recv_callback)_bridge_on_recv);
    espconn_regist_sentcb(current, (espconn_sent_callback)_bridge_on_sent);
    espconn_regist_reconcb(current, (espconn_reconnect_callback)_bridge_on_recon);
    espconn_regist_disconcb(current, (espconn_connect_callback)_bridge_on_discon);
    _bridge_keepalive = 300;
    espconn_set_keepalive(current, ESPCONN_KEEPIDLE, &_bridge_keepalive);
    uart_debug_message("bridge client connected");
}


void ICACHE_FLASH_ATTR bridge_start() {
    uart_debug_message("bridge started");
    _bridge_buffer_index = 0;
    _bridge_client = 0;
    sendQueue = 0;

    os_memset(_bridge_conn, 0, sizeof(_bridge_conn));
    os_memset(_bridge_proto, 0, sizeof(_bridge_proto));

    _bridge_conn.type=ESPCONN_TCP;
    _bridge_conn.state=ESPCONN_NONE;
    _bridge_conn.proto.tcp=&_bridge_proto;
    _bridge_conn.link_cnt = 1;
    _bridge_proto.local_port=8080;
    //_server_proto.connect_callback=_on_connect;
    espconn_regist_connectcb(&_bridge_conn, (espconn_connect_callback)_bridge_on_connect);
    espconn_accept(&_bridge_conn);
}

void ICACHE_FLASH_ATTR bridge_stop() {
    uart_debug_message("bridge stopped");
    espconn_delete(&_bridge_conn);
    if (_bridge_client!=0) {
        bridge_release();
        _bridge_client = 0;
    }
}