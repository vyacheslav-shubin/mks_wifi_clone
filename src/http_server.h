//
// Created by shubin on 22.06.2020.
//

#ifndef WIFI_MODULE_HTTP_SERVER_H
#define WIFI_MODULE_HTTP_SERVER_H
#include "../sdk/c_types.h"
#include "../sdk/os_type.h"
#include "../sdk/gpio.h"
#include "../sdk/user_interface.h"
#include "../sdk/osapi.h"
#include "../sdk/ets_sys.h"
#include "../sdk/mem.h"
#include "../sdk/espconn.h"

extern void http_server_start();
extern void http_server_stop();

#endif //WIFI_MODULE_HTTP_SERVER_H
