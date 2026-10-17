/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-20 08:53:40
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-10-23 14:21:54
 * @FilePath: /project_3/src/SpeakAmp.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "PeripheralControl.h"
#include "SpeakAmp.h"
#include <stdio.h>
#define GPIO_CONTRLL_AMP  // 功放使能用GPIO控制
#define APEAK_AMP_GPIO 69  // SP_EN 功放使能（原理图 JIAO 12 → GPIO69）；原来是32(锁脚)，改错了所以完全无声
static int AmpStatus = -1; 
/**
 * @description: 初始化功放控制脚
 * @return {*}
 */
void SpeakAmpGpioInit(void)
{
    if (GpioOpen(APEAK_AMP_GPIO, GPIO_DIR_LOW, true) == false)
    {
        return;
    }
    AmpStatus = 0;
}

int AmpControl(int en)     
{                                       
    static int EnState = -1;            
    if (en != EnState)                  
    {                                   
        EnState = en;                   

        #ifdef GPIO_CONTRLL_AMP
        GpioLevelSet(APEAK_AMP_GPIO,EnState ? GPIO_DIR_HIGH : GPIO_DIR_LOW);
        #else
        char Data = en ? (_74hc595dDevStatus() | (1 << 6)) : (_74hc595dDevStatus() & ~(1 << 6)); 
        _74hc595dDevWrite(Data); 
        #endif

        return EnState;                 
    }                                   
    return EnState;                     
}

/**
 * @description: 获取功放控制脚状态
 * @return {*}
 */
GPIO_LEVEL SpeakAmpStatus(void)
{
    GPIO_LEVEL Level;
    if (GpioLevelGet(APEAK_AMP_GPIO, &Level) == false)
    {
        return GPIO_LEVEL_UNKNOWN;
    }
    AmpStatus = Level;
    return AmpStatus;
}

/**
 * @description: 使能功放
 * @return {*}
 */
void SpeakAmpEnable(void)
{
    if (AmpStatus)
        return;
    AmpStatus = 1;
    // printf("[%s]!!!\n", __func__);
    #ifdef GPIO_CONTRLL_AMP
    GpioLevelSet(APEAK_AMP_GPIO, GPIO_LEVEL_HIGH);
    #else
    AmpControl(AmpStatus);
    #endif
}

/**
 * @description: 失能功放
 * @return {*}
 */
void SpeakAmpDisable(void)
{
    if (!AmpStatus)
        return;
    AmpStatus = 0;
    // printf("[%s]!!!\n", __func__);
    #ifdef GPIO_CONTRLL_AMP
    GpioLevelSet(APEAK_AMP_GPIO, GPIO_LEVEL_LOW);
    #else
    AmpControl(AmpStatus);
    #endif
}
