#include <esp_gap_bt_api.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

char *bdaToStr(const esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

bool getNameFromEir(uint8_t *eir, uint8_t *nameBuffer, uint8_t *nameLen) {
    assert(eir);

    uint8_t *remoteName = NULL;
    uint8_t remoteNameLen = 0;

    if (nameBuffer) {
        nameBuffer[0] = '\0';
    }

    if (nameLen) {
        *nameLen = 0;
    }

    remoteName = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &remoteNameLen);

    if (!remoteName) {
        remoteName = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &remoteNameLen);
    }

    if (!remoteName) {
        return false;
    }

    if (remoteNameLen > ESP_BT_GAP_MAX_BDNAME_LEN) {
        remoteNameLen = ESP_BT_GAP_MAX_BDNAME_LEN;
    }

    if (nameBuffer) {
        memcpy(nameBuffer, remoteName, remoteNameLen);
        nameBuffer[remoteNameLen] = '\0';
    }

    if (nameLen) {
        *nameLen = remoteNameLen;
    }

    return true;
}
