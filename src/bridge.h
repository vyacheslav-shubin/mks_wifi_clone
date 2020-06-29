//
// Created by shubin on 19.06.2020.
//

#ifndef WIFI_MODULE_BRIDGE_H
#define WIFI_MODULE_BRIDGE_H

#endif //WIFI_MODULE_BRIDGE_H

extern struct espconn * _bridge_client;
extern void bridge_start();
extern unsigned char bridge_out(char * data);
extern void bridge_stop();