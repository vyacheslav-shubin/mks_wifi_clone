//
// Created by shubin on 18.06.2020.
//

#ifndef WIFI_MODULE_EEPROM_H
#define WIFI_MODULE_EEPROM_H


#include "../sdk/c_types.h"
#include "../sdk/spi_flash.h"

#define EEPROM_DATA_SIGNATURE 	((u32)0xA5A5A5A7)
//0x7b000
//#define EEPROM_SECTOR			123
#define EEPROM_SECTOR			128

bool eeprom_load();
bool eeprom_save();

#define EEPROM_WIFI_SID_SIZE			32
#define EEPROM_WIFI_PASSWORD_SIZE		64

typedef struct {
    unsigned char       mode;
    unsigned char 		ssid[EEPROM_WIFI_SID_SIZE];
    unsigned char 		password[EEPROM_WIFI_PASSWORD_SIZE];
} eeprom_wifi;


typedef struct  {
    u16		size;
    u32		signature;
    u32		counter;
    eeprom_wifi wifi_client;
} eeprom_data;

#define EEPROM_DATA_SIZE				(sizeof(eeprom_data))
#define EEPROM_DATA_ALIGNED_SIZE	((u32)(EEPROM_DATA_SIZE + 3) & (u32)(~3))

u8 eeprom_data_memory[EEPROM_DATA_ALIGNED_SIZE];

#define eeprom ((eeprom_data *) &eeprom_data_memory[0])

#endif //WIFI_MODULE_EEPROM_H
