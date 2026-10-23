/**
  ******************************************************************************
  * @file    sd_diskio_dma_template.c
  * @author  MCD Application Team
  * @brief   SD DMA Disk I/O driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "util/sd_diskio.h"
#include "stm32h7xx_hal.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/*
 * the following Timeout is useful to give the control back to the applications
 * in case of errors in either BSP_SD_ReadCpltCallback() or BSP_SD_WriteCpltCallback()
 * the value by default is as defined in the BSP platform driver otherwise 30 secs
 */

#define SD_TIMEOUT (30U * 1000U)
#define SD_ABORT_TIMEOUT 100U
#define SD_ABORT_RESERVE 150U
#define SD_EDITOR_MAX_OPERATION 1500U

#define SD_DEFAULT_BLOCK_SIZE 512

/*
 * Depending on the usecase, the SD card initialization could be done at the
 * application level, if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize().
 */

/* #define DISABLE_SD_INIT */

/*
 * when using cachable memory region, it may be needed to maintain the cache
 * validity. Enable the define below to activate a cache maintenance at each
 * read and write operation.
 * Notice: This is applicable only for cortex M7 based platform.
 */

#define ENABLE_SD_DMA_CACHE_MAINTENANCE 1


/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
//static volatile  UINT  WriteStatus = 0, ReadStatus = 0;
static volatile uint32_t WriteStatus = 0;
static volatile uint32_t ReadStatus  = 0;
static volatile uint32_t OperationDeadline = 0;
static volatile uint8_t Quarantined = 0;
extern SD_HandleTypeDef hsd1;
/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS        SD_initialize(BYTE);
DSTATUS        SD_status(BYTE);
DRESULT        SD_read(BYTE, BYTE *, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write(BYTE, const BYTE *, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE, BYTE, void *);
#endif /* _USE_IOCTL == 1 */

void SD_SetOperationDeadline(uint32_t deadline)
{
    OperationDeadline = deadline;
}

void SD_ClearOperationDeadline(void)
{
    OperationDeadline = 0;
}

uint8_t SD_IsQuarantined(void)
{
    return Quarantined;
}

static uint32_t SD_OperationTimeout(void)
{
    const uint32_t deadline = OperationDeadline;
    if(deadline == 0)
        return SD_TIMEOUT;

    const int32_t remaining = (int32_t)(deadline - HAL_GetTick());
    if(remaining <= (int32_t)SD_ABORT_RESERVE)
        return 0;

    const uint32_t bounded = (uint32_t)remaining - SD_ABORT_RESERVE;
    return bounded < SD_EDITOR_MAX_OPERATION ? bounded
                                              : SD_EDITOR_MAX_OPERATION;
}

static uint8_t SD_OperationTimedOut(uint32_t started_at, uint32_t timeout)
{
    if((HAL_GetTick() - started_at) >= timeout)
        return 1;

    const uint32_t deadline = OperationDeadline;
    return deadline != 0
           && (int32_t)(deadline - HAL_GetTick())
                  <= (int32_t)SD_ABORT_RESERVE;
}

static uint8_t SD_TransferReady(void)
{
    const uint32_t deadline = OperationDeadline;
    if(deadline == 0)
        return BSP_SD_GetCardState() == SD_TRANSFER_OK;

    uint32_t card_status = 0;
    const uint32_t error = SDMMC_CmdSendStatusWithTimeout(
        hsd1.Instance, (uint32_t)(hsd1.SdCard.RelCardAdd << 16u), 25U,
        &card_status);
    if(error != SDMMC_ERROR_NONE)
        return 0;
    return ((card_status >> 9u) & 0x0fu) == HAL_SD_CARD_TRANSFER;
}

static void SD_QuiesceTimedOutTransfer(void)
{
    NVIC_DisableIRQ(SDMMC1_IRQn);
    __HAL_SD_DISABLE_IT(&hsd1, SDMMC_IT_DATAEND | SDMMC_IT_DCRCFAIL
                                   | SDMMC_IT_DTIMEOUT | SDMMC_IT_TXUNDERR
                                   | SDMMC_IT_RXOVERR);
    hsd1.Instance->IDMACTRL = SDMMC_DISABLE_IDMA;
    __HAL_SD_CLEAR_FLAG(&hsd1, SDMMC_STATIC_CMD_FLAGS);
    (void)SDMMC_CmdStopTransferWithTimeout(hsd1.Instance,
                                            SD_ABORT_TIMEOUT);
    hsd1.Instance->DCTRL = 0;
    __HAL_SD_CLEAR_FLAG(&hsd1, SDMMC_STATIC_FLAGS);
    __HAL_RCC_SDMMC1_FORCE_RESET();
    __HAL_RCC_SDMMC1_RELEASE_RESET();
    hsd1.State = HAL_SD_STATE_READY;
    hsd1.Context = SD_CONTEXT_NONE;
    NVIC_ClearPendingIRQ(SDMMC1_IRQn);
    NVIC_EnableIRQ(SDMMC1_IRQn);
    Quarantined = 1;
    Stat = STA_NOINIT;
}

const Diskio_drvTypeDef SD_Driver = {
    SD_initialize,
    SD_status,
    SD_read,
#if _USE_WRITE == 1
    SD_write,
#endif /* _USE_WRITE == 1 */

#if _USE_IOCTL == 1
    SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun)
{
    Stat = STA_NOINIT;

    if(Quarantined != 0)
        return Stat;

    if(BSP_SD_GetCardState() == MSD_OK)
    {
        Stat &= ~STA_NOINIT;
    }

    return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
#if !defined(DISABLE_SD_INIT)

    if(BSP_SD_Init() == MSD_OK)
    {
        Quarantined = 0;
        Stat = SD_CheckStatus(lun);
    }

#else
    Stat = SD_CheckStatus(lun);
#endif
    return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
    if(Quarantined != 0)
        return STA_NOINIT;
    return SD_CheckStatus(lun);
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    ReadStatus  = 0;
    const uint32_t timeout = SD_OperationTimeout();
    const uint32_t started_at = HAL_GetTick();
    if(Quarantined != 0 || timeout == 0)
        return RES_NOTRDY;
#if(ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    uint32_t alignedAddr;
    alignedAddr = (uint32_t)buff & ~0x1F;
    SCB_CleanDCache_by_Addr((uint32_t *)alignedAddr,
                            count * BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif
    if(BSP_SD_ReadBlocks_DMA((uint32_t *)buff, (uint32_t)(sector), count)
       == MSD_OK)
    {
        /* Wait that the reading process is completed or a timeout occurs */
          while(ReadStatus == 0 && !SD_OperationTimedOut(started_at, timeout)) {}
        /* incase of a timeout return error */
        if(ReadStatus == 0)
        {
            SD_QuiesceTimedOutTransfer();
            res = RES_ERROR;
        }
        else
        {
            ReadStatus = 0;
            while(!SD_OperationTimedOut(started_at, timeout))
            {
                if(SD_TransferReady())
                {
                    res = RES_OK;
#if(ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
                    /* the SCB_InvalidateDCache_by_Addr() requires a 32-Byte aligned address,
                     * adjust the address and the D-Cache size to invalidate accordingly. */
                    SCB_InvalidateDCache_by_Addr(
                        (uint32_t *)alignedAddr,
                        count * BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif
                    break;
                }
            }
            if(res != RES_OK)
                SD_QuiesceTimedOutTransfer();
        }
    }

    return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    WriteStatus = 0;
    const uint32_t timeout = SD_OperationTimeout();
    const uint32_t started_at = HAL_GetTick();
    if(Quarantined != 0 || timeout == 0)
        return RES_NOTRDY;
#if(ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    uint32_t alignedAddr;
#endif
    /*
  * since the MPU is configured as write-through, see main.c file, there isn't any need
  * to maintain the cache as its content is always coherent with the memory.
  * If needed, check the file "Middlewares/Third_Party/FatFs/src/drivers/sd_diskio_dma_template.c"
  * to see how the cache is maintained during the write operations.
  */
#if(ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)

    /*
    the SCB_CleanDCache_by_Addr() requires a 32-Byte aligned address
    adjust the address and the D-Cache size to clean accordingly.
    */
    alignedAddr = (uint32_t)buff & ~0x1F;
    SCB_CleanDCache_by_Addr((uint32_t *)alignedAddr,
                            count * BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif
    if(BSP_SD_WriteBlocks_DMA((uint32_t *)buff, (uint32_t)(sector), count)
       == MSD_OK)
    {
        /* Wait that writing process is completed or a timeout occurs */
          while(WriteStatus == 0
              && !SD_OperationTimedOut(started_at, timeout)) {}
        /* incase of a timeout return error */
        if(WriteStatus == 0)
        {
            SD_QuiesceTimedOutTransfer();
            res = RES_ERROR;
        }
        else
        {
            WriteStatus = 0;
            while(!SD_OperationTimedOut(started_at, timeout))
            {
                if(SD_TransferReady())
                {
                    res = RES_OK;
                    break;
                }
            }
            if(res != RES_OK)
                SD_QuiesceTimedOutTransfer();
        }
    }

    return res;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    DRESULT         res = RES_ERROR;
    BSP_SD_CardInfo CardInfo;

    if(Stat & STA_NOINIT)
        return RES_NOTRDY;

    switch(cmd)
    {
        /* Make sure that no pending write process */
        case CTRL_SYNC: res = RES_OK; break;

        /* Get number of sectors on the disk (DWORD) */
        case GET_SECTOR_COUNT:
            BSP_SD_GetCardInfo(&CardInfo);
            *(DWORD *)buff = CardInfo.LogBlockNbr;
            res            = RES_OK;
            break;

        /* Get R/W sector size (WORD) */
        case GET_SECTOR_SIZE:
            BSP_SD_GetCardInfo(&CardInfo);
            *(WORD *)buff = CardInfo.LogBlockSize;
            res           = RES_OK;
            break;

        /* Get erase block size in unit of sector (DWORD) */
        case GET_BLOCK_SIZE:
            BSP_SD_GetCardInfo(&CardInfo);
            *(DWORD *)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
            res            = RES_OK;
            break;

        default: res = RES_PARERR;
    }

    return res;
}
#endif /* _USE_IOCTL == 1 */


/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */

void BSP_SD_WriteCpltCallback(void)
{
    WriteStatus = 1;
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */

void BSP_SD_ReadCpltCallback(void)
{
    ReadStatus = 1;
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, 1);
}

// Interrupts -- Not sure these belong here or elsewhere yet.

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
