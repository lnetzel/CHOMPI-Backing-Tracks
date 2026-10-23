#pragma once
#ifndef DSY_SD_DISKIO_H
#define DSY_SD_DISKIO_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "ff_gen_drv.h"
#include "util/bsp_sd_diskio.h"

    extern const Diskio_drvTypeDef SD_Driver; /**< & */
    void SD_SetOperationDeadline(uint32_t deadline);
    void SD_ClearOperationDeadline(void);
    uint8_t SD_IsQuarantined(void);

#ifdef __cplusplus
}
#endif

#endif
