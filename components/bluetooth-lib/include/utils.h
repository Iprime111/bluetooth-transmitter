#ifndef BT_LIB_UTILS_H_
#define BT_LIB_UTILS_H_

#include <esp_bt_defs.h>
#include <stddef.h>

char *bdaToStr(const esp_bd_addr_t bda, char *str, size_t size);
bool getNameFromEir(uint8_t *eir, uint8_t *nameBuffer, uint8_t *nameLen);

#endif
