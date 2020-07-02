//
// Created by shubin on 22.06.2020.
//

#include <stdlib.h>
#include "http_server.h"
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
#include "../gen/html.h"
#include "loop.h"

typedef enum {
    HTTP_STATE_METHOD,
    HTTP_STATE_HEADER,
    HTTP_DONE,
    HTTP_CONTENT,
    HTTP_FILE,
    HTTP_STATE_FAIL
} REQUEST_STATE;

static struct espconn _http_conn;
static esp_tcp _http_proto;

#define HTTP_POST_FLAG              BIT(0)
#define HTTP_POST_FROM_SUBMIT       BIT(1)
#define HTTP_POST_CONTENT_STARTED   BIT(2)
#define HTTP_FILE_STARTED           BIT(3)

#define TRANSFER_FINALIZE_TIMER_DELAY           2000
#define TRANSFER_FINALIZE_TIMER_DELAY_REPEAT    500

static uint32_t _http_keepalive_idle = 30;
static uint32_t _http_keepalive_int = 1;

typedef struct {
    REQUEST_STATE state;
    unsigned char request_id;
    unsigned char flags;
    char file_name[100];
    u32 content_size;
    u32 content_counter;
    int file_part_index;
    unsigned char buffer[1024];
    unsigned char buffer_index;
} HTTP_REQUEST;

static void http_on_discon(struct espconn *current);


static ICACHE_FLASH_ATTR void http_server_sent_responce_message(struct espconn *current, char * msg) {
    espconn_sent(current, msg, os_strlen(msg));
}

static os_timer_t _transfer_complete_timer;

static void ICACHE_FLASH_ATTR on_transfer_complete_timer_proc(void *arg) {
    os_timer_disarm(&_transfer_complete_timer);
    if (uart_tx_buffer_queue_len()==0)
        uart_sync_dma(0);
    else
        os_timer_arm(&_transfer_complete_timer, TRANSFER_FINALIZE_TIMER_DELAY_REPEAT, 0);
}

static ICACHE_FLASH_ATTR void http_server_sent_responce(struct espconn *current) {
    HTTP_REQUEST * request = (HTTP_REQUEST *)current->reverse;
    if (request->state == HTTP_STATE_FAIL) {
        http_server_sent_responce_message(current, "HTTP/1.1 400 OK\r\n\r\n");
    } else {
        switch (request->request_id) {
            case MESSAGE_HTTP_ROOT:
                espconn_sent(current, index_html, sizeof(index_html));
                break;
            case MESSAGE_HTTP_UPLOAD:
                http_server_sent_responce_message(current, "HTTP/1.1 200 OK\r\n\r\n");
                break;
        }
    }
}

static unsigned ICACHE_FLASH_ATTR char is_started(const unsigned char * str, const char * pattern) {
    return os_strncmp(str, pattern, os_strlen(pattern))==0;
}

static void ICACHE_FLASH_ATTR http_row(struct espconn *connection) {
    HTTP_REQUEST * request = (HTTP_REQUEST *)connection->reverse;
    switch (request->state) {
        case HTTP_STATE_METHOD: {
            //'GET / HTTP/1.1'
            uart_debug_message(request->buffer);
            if (request->buffer_index < 14) {
                request->state = HTTP_STATE_FAIL;
                return;
            }
            int from;
            if (is_started(request->buffer, "GET")) {
                from = 4;
            } else if (is_started(request->buffer, "POST")) {
                request->flags |=  HTTP_POST_FLAG;
                from = 5;
            } else {
                request->state = HTTP_STATE_FAIL;
                return;
            }
            int i = from;
            while ((i < request->buffer_index) && (request->buffer[i] != ' '))
                i++;
            if (i == request->buffer_index) {
                request->state = HTTP_STATE_FAIL;
                return;
            }
            if ((i - from == 1) && (request->buffer[from] == '/') && (!(request->flags & HTTP_POST_FLAG))) {
                request->request_id = MESSAGE_HTTP_ROOT;
            } else if ((request->flags & HTTP_POST_FLAG) && is_started(&request->buffer[from], "/upload?X-Filename")) {
                request->request_id = MESSAGE_HTTP_UPLOAD;
                uart_debug_message("http upload file request");
            } else {
                request->state = HTTP_STATE_FAIL;
                break;
            }
            request->state = HTTP_STATE_HEADER;
            break;
        }
        case HTTP_STATE_HEADER: {
            if (request->buffer_index==0) {
                if (request->flags & HTTP_POST_FLAG) {
                    request->state = HTTP_CONTENT;
                } else {
                    request->state = HTTP_DONE;
                }
                break;
            }
            if (request->flags & HTTP_POST_FLAG) {
                if (is_started(request->buffer, "Content-Length:")) {
                    uart_debug_message(request->buffer);
                    request->content_size = atol(&request->buffer[16]);
                } else if (is_started(request->buffer, "Content-Type:")) {
                    if (
                            is_started(&request->buffer[14], "multipart/form-data")
                            ||
                                    is_started(&request->buffer[14], "application/octet-stream")
                            ) {
                        request->flags |= HTTP_POST_FROM_SUBMIT;
                    } else
                        request->state = HTTP_STATE_FAIL;
                }
            }
            break;
        }
        case HTTP_CONTENT: {
            if (request->buffer_index==0) {
                unsigned char len = os_strlen(request->file_name);
                request->state = ((request->content_size > 0) && (len != 0)) ? HTTP_FILE : HTTP_STATE_FAIL;
                break;
            }
            if (is_started(request->buffer, "Content-Disposition:")) {
                char * pos = (char *)os_strstr(request->buffer, "filename=\"");
                if (pos) {
                    pos = &pos[10];
                    unsigned char i;
                    while (1) {
                        unsigned char b = pos[i];
                        if (b && (b!='"'))
                            request->file_name[i++] = b;
                        else
                            break;
                    }
                    os_strcpy(request->buffer, "http file: ");
                    os_strcat(request->buffer, request->file_name);
                    uart_debug_message(request->buffer);
                }
            }
            break;
        }
        default:
            break;
    }
}

static void ICACHE_FLASH_ATTR http_file(struct espconn *connection, char *pdata, unsigned short len) {
    HTTP_REQUEST * request = (HTTP_REQUEST *)connection->reverse;
    if (!(request->flags & HTTP_FILE_STARTED)) {
        request->flags |= HTTP_FILE_STARTED;
        unsigned char l = os_strlen(request->file_name);
        uart_start_message(ESP_TYPE_FILE_FIRST, l + 5);
        uart_tx(l);
        uart_message((unsigned char *)&request->content_size, 4);
        uart_message((unsigned char *)request->file_name, l);
        uart_end_message();
        os_delay_us(1500000);
        uart_sync_dma(1);
    }

    unsigned short writed = 0;
    unsigned short package_size;
    unsigned char _sl = 0;
    while (writed<len) {
        package_size = len - writed;
        if (package_size > 500)
            package_size = 500;

        request->content_counter+=package_size;
        u32 part = request->file_part_index++;
        if (request->content_counter==request->content_size)
            part |= (u32) (1 << 31);

        //char buf[100];
        //os_sprintf(buf, "%d %d/%d", part, request->content_counter, request->content_size);
        //uart_debug_message(buf);

        uart_start_message(ESP_TYPE_FILE_FRAGMENT ,  package_size + sizeof(part));
        uart_message((unsigned char *)&part, sizeof(part));
        uart_message((unsigned char *)(&pdata[writed]), package_size);
        uart_end_message();

        writed += package_size;
    }
}

//2422560/2419220
//2422560/2416300
// 2422560/2391480
void ICACHE_FLASH_ATTR  http_on_recv(struct espconn *connection, char *pdata, unsigned short len) {
    HTTP_REQUEST * request = (HTTP_REQUEST *)connection->reverse;
    if (!request) {
        request = (HTTP_REQUEST *)os_zalloc(sizeof(HTTP_REQUEST));
        connection->reverse = request;
        uart_debug_message("http client memory allocated");
    }

    if (request->state == HTTP_FILE) {
        http_file(connection, pdata, len);
    } else {
        for (int i = 0; i < len; i++) {
            if (request->flags & HTTP_POST_CONTENT_STARTED)
                request->content_counter++;
            unsigned char b = pdata[i];
            if ((request->buffer_index == 0) && (b == '\n'))
                continue;
            switch (request->state) {
                case HTTP_CONTENT: {
                    if (!(request->flags & HTTP_POST_CONTENT_STARTED)) {
                        request->content_counter = 1;
                        request->flags |= HTTP_POST_CONTENT_STARTED;
                    }
                    break;
                }
                case HTTP_FILE: {
                    request->content_counter--; //Коррекция счетчика. Так как в http_file вычисляется content_counter
                    http_file(connection, &pdata[i], len - i);
                    goto done;
                }
            }

            if (b == '\r') {
                request->buffer[request->buffer_index] = 0;
                http_row(connection);
                request->buffer_index = 0;
            } else {
                request->buffer[request->buffer_index] = b;
                request->buffer_index++;
            }

            if (request->buffer_index == sizeof(request->buffer) - 1) {
                request->state = HTTP_STATE_FAIL;
                request->buffer_index = 0;
            }
        }
    }
    done:

    if((request->state == HTTP_FILE) && (request->content_counter == request->content_size)) {
        request->state = HTTP_DONE;
        uart_sync_dma(0);
        //os_timer_arm(&_transfer_complete_timer, TRANSFER_FINALIZE_TIMER_DELAY, 0);
    }

    if ((request->state == HTTP_STATE_FAIL) || (request->state == HTTP_DONE))
        http_server_sent_responce(connection);
}

void ICACHE_FLASH_ATTR  http_on_sent(struct espconn *current) {
    espconn_disconnect(current);
    espconn_delete(current);
}

void ICACHE_FLASH_ATTR  http_on_recon(struct espconn *current, sint8 err) {
    uart_debug_message("http client reconnected");
}

static void ICACHE_FLASH_ATTR http_on_discon(struct espconn *current) {
    HTTP_REQUEST * request = (HTTP_REQUEST *)current->reverse;
    if (current->reverse) {
        if (request->state==HTTP_FILE)
            os_timer_arm(&_transfer_complete_timer, TRANSFER_FINALIZE_TIMER_DELAY, 0);
        os_free(current->reverse);
        current->reverse = 0;
        uart_debug_message("http client memory release");
    }
    uart_debug_message("http client disconnected");
}

static void ICACHE_FLASH_ATTR http_on_connect(struct espconn *current) {
    uart_debug_message("http client connected");
    current->reverse = 0;
    espconn_set_keepalive(current, ESPCONN_KEEPIDLE, &_http_keepalive_idle);
    espconn_set_keepalive(current, ESPCONN_KEEPINTVL, &_http_keepalive_int);

    espconn_regist_recvcb(current, (espconn_recv_callback) http_on_recv);
    espconn_regist_sentcb(current, (espconn_sent_callback) http_on_sent);
    espconn_regist_reconcb(current, (espconn_reconnect_callback) http_on_recon);
    espconn_regist_disconcb(current, (espconn_connect_callback) http_on_discon);
}

void ICACHE_FLASH_ATTR http_server_start() {
    os_memset(&_transfer_complete_timer, 0, sizeof(_transfer_complete_timer));
    os_timer_disarm(&_transfer_complete_timer);
    os_timer_setfn(&_transfer_complete_timer, (os_timer_func_t *)on_transfer_complete_timer_proc, 0);
    uart_debug_message("http server started");
    os_memset(_http_conn, 0, sizeof(_http_conn));
    os_memset(_http_proto, 0, sizeof(_http_proto));

    _http_conn.type=ESPCONN_TCP;
    _http_conn.state=ESPCONN_NONE;
    _http_conn.proto.tcp=&_http_proto;
    _http_conn.link_cnt = 1;
    _http_proto.local_port=80;
    espconn_regist_connectcb(&_http_conn, (espconn_connect_callback) http_on_connect);
    espconn_accept(&_http_conn);
}

void ICACHE_FLASH_ATTR http_server_stop() {
    uart_debug_message("http server stoped");
    espconn_delete(&_http_conn);
}
