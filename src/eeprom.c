//
// Created by shubin on 18.06.2020.
//

#include "eeprom.h"
#include "../sdk/mem.h"
#include "../sdk/osapi.h"

bool ICACHE_FLASH_ATTR _eeprom_erase_and_save() {
    eeprom->signature = EEPROM_DATA_SIGNATURE;
    eeprom->size=EEPROM_DATA_SIZE;
    if (spi_flash_erase_sector(EEPROM_SECTOR)==SPI_FLASH_RESULT_OK) {
        return (spi_flash_write(EEPROM_SECTOR*SPI_FLASH_SEC_SIZE, (u32 *)eeprom_data_memory, sizeof(eeprom_data_memory))==SPI_FLASH_RESULT_OK);
    }
    return FALSE;
}

bool ICACHE_FLASH_ATTR eeprom_save() {
    u32 *mem = (u32 *)os_malloc(sizeof(eeprom_data_memory));
    bool res=FALSE;
    if (spi_flash_read(EEPROM_SECTOR * SPI_FLASH_SEC_SIZE, mem, sizeof(eeprom_data_memory))==SPI_FLASH_RESULT_OK) {
        if (os_memcmp(mem, eeprom_data_memory, sizeof(eeprom_data_memory))!=0) {
            res = _eeprom_erase_and_save();
        } else
            res=TRUE;
    }
    os_free(mem);
    return res;
}

bool ICACHE_FLASH_ATTR eeprom_load() {
    bool res=FALSE;
    if (spi_flash_read(EEPROM_SECTOR * SPI_FLASH_SEC_SIZE, (u32 *)eeprom_data_memory, sizeof(eeprom_data_memory))==SPI_FLASH_RESULT_OK) {
        if (eeprom->signature!=EEPROM_DATA_SIGNATURE) {
            os_printf("EEPROM FIX SIGNATURE\n");
            os_memset(eeprom_data_memory, 0, sizeof(eeprom_data_memory));
            res=_eeprom_erase_and_save();
        } else if (EEPROM_DATA_SIZE != eeprom->size) {
            os_printf("EEPROM FIX SIZE\n");
            os_memset(&eeprom_data_memory[eeprom->size], 0, sizeof(eeprom_data_memory)-eeprom->size);
            res=_eeprom_erase_and_save();
        } else
            res=TRUE;
        return TRUE;
    }
    return res;
}