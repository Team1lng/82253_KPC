/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-22 20:56:52
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-13 16:05:02
 * @FilePath: /82225-EPC/src/AdcDetectEvent.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "NetworkCommon.h"
#include "UserNetManage.h"
#include "NetMsgComm.h"
#include "VoiceRingPlay.h"
#include "LightControl.h"
#include "AdcControl.h"
#include "AdcDetect.h"
#include "UserConfig.h"
#include "Unlock.h"
#include "Timer.h"
#include <math.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "GpioControl.h"

/* 2472 */
/* 670 */
/* 1580 */
/* 1960 */
#define KeyInterval 100
#define DEFINE_ADC_TAG(_TAG, ADC, FUNC) _TAG,
#define DEFINE_ADC_VOLTAGE(_TAG, ADC, FUNC) {#_TAG, ADC, FUNC},
#define ADC_TAG_LIST(TAG)           \
    TAG(Call1, 1960, CallKeyHandle) \
    TAG(Call2, 1580, CallKeyHandle) \
    TAG(Call3, 670, CallKeyHandle) \
    TAG(Call4, 2472, CallKeyHandle) \
    TAG(Exit2, 90, ExitBtnHandle) \
    TAG(Exit1, 2830, ExitBtnHandle) 

typedef enum
{
    ADC_TAG_LIST(DEFINE_ADC_TAG)
        AdcTagMax
} AdcTagList;

typedef struct
{
    char *Str;
    int Voltage;
    void (*Func)(void *Arg);
} AdcVoltage;

/**
 * @description: 呼叫按键灯光控制
 * @param {int} Floor 户型
 * @return {*}
 */
void CallKeyLightCtrl(int Floor)
{
    KeyLightControl(abs((Floor)-Call2));
}

static void CallBusyHandle(void *u)
{
    VoiceRingPlay(CallBusy, VoiceDefVol);
    printf("\n\n===================\n\n");
    KeyLightControl(-1);
}

static void CallKeyHandle(void *Arg)
{
    if (IsNetManageEntryState())
        return;


    #define OPEN1_SW0_GPIO 27   
    #define OPEN1_SW1_GPIO 25   
    static GPIO_LEVEL level0 = GPIO_LEVEL_UNKNOWN;
    static GPIO_LEVEL level1 = GPIO_LEVEL_UNKNOWN;

    static int level = 0;
    if (level0 == GPIO_LEVEL_UNKNOWN)
    {
        GpioOpen(OPEN1_SW0_GPIO, GPIO_DIR_IN, false);
    }
    if (level1 == GPIO_LEVEL_UNKNOWN)
    {
        GpioOpen(OPEN1_SW1_GPIO, GPIO_DIR_IN, false);
    }

    if (!GpioLevelGet(OPEN1_SW0_GPIO, &level0))
    {
        level0 = GPIO_LEVEL_LOW;
    }
    if (!GpioLevelGet(OPEN1_SW1_GPIO, &level1))
    {
        level1 = GPIO_LEVEL_LOW;
    }

    if (level0 == GPIO_LEVEL_LOW && level1 == GPIO_LEVEL_LOW)
    {
        level = 0;
    }else if (level0 == GPIO_LEVEL_HIGH && level1 == GPIO_LEVEL_LOW){
        level = 1;
    }else if (level0 == GPIO_LEVEL_LOW && level1 == GPIO_LEVEL_HIGH){
        level = 2;
    }else if (level0 == GPIO_LEVEL_HIGH && level1 == GPIO_LEVEL_HIGH){
        level = 3;
    }
    
    int Key = *(int *)Arg;
    NetworkMsgData Data;
    Data.Device = DEVICE_ALL;
    Data.Cmd = DoorbellEvent;
    Data.Arg1 = ((!TimerEnablestatus(SecirityTriggerTimer)) << (TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer)));
    Data.Arg2 = Key + 1 - level;
    NetworkMsgSned(Data);

    VoiceRingPlay(Bi1, VoiceDefVol);

    KeyLightControl(3 - Key); //hare Key

    if (!TimerEnablestatus(CallBusyTimer) && !TimerEnablestatus(MonitorTimer) && !TimerEnablestatus(CommunicateTimer))
    {
        SetTimer(5000, CallBusyTimer, CallBusyHandle, NULL);
    }

    printf("[%s][%d]Key:%d\n", __func__, __LINE__, Key);
}

static void ExitBtnHandle(void *Arg)
{
    int Exit = *(int *)Arg;
    printf("[%s][%d]Exit:%d\n", __func__, __LINE__, Exit);
    Unlock(Exit == Exit1 ? UserConfigGet()->UnlockTime : UserConfigGet()->UngateTime, Exit == Exit1 ? LOCK_TYPE : GATE_TYPE);
}

void DoorbellCallDetect(int Voltage)
{
#define VoltageDefault 3299
#define VoltageCacheMax 7

    static AdcVoltage AdcVoltageGroup[] = {ADC_TAG_LIST(DEFINE_ADC_VOLTAGE)};
    static int AdcVoltageCache[VoltageCacheMax];
    static int CacheIndex = 0;
    static int VaildIndex = -1;

    /* 值未达到有效变动范围 */
    if (Voltage > (VoltageDefault - KeyInterval)) /* 去掉误差 */
    {
        VaildIndex = -1;
        CacheIndex = 0;
        return;
    }

    /* 滤波 */
    if (CacheIndex >= VoltageCacheMax)
    {
#if 1
        /* 取中位数做最终值，若没有中位数，则取前一位数据 */
        int Index = (VoltageCacheMax % 2) ? (VoltageCacheMax >> 1) + 1 : (VoltageCacheMax >> 1);
        Voltage = AdcVoltageCache[Index];
#else
        /* 去掉最大最小值，取平均值做最终值 */
        int Num = 0, Sum = 0;
        for (int i = 1; i < VoltageCacheMax - 1; i++, Num++)
        {
            Sum += AdcVoltageCache[i];
        }
        Voltage = Sum / Num;
#endif
        CacheIndex = 0;
        // printf("Filter Key Voltage:%d\n", Voltage);
    }
    else
    {
        AdcVoltageCache[CacheIndex++] = Voltage;
        return;
    }

    /* 事件驱动 */
    for (int i = 0; i < sizeof(AdcVoltageGroup) / sizeof(AdcVoltage); i++)
    {
        if (abs(Voltage - AdcVoltageGroup[i].Voltage) < KeyInterval)
        {
            if (VaildIndex != i)
            {
                VaildIndex = i;
                if (AdcVoltageGroup[i].Func)
                {
                    AdcVoltageGroup[i].Func(&i);
                }
            }
            return;
        }
    }
    VaildIndex = -1;
}

int AdcDetectHandle(int Voltage)
{
    DoorbellCallDetect(Voltage);
    return 0;
}