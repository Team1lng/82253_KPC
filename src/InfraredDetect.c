/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-23 20:37:33
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-12-12 09:14:31
 * @FilePath: /82225-EPC/src/InfraredDetect.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "GeneralInterface.h"
#include "EpollGpioEvent.h"
#include "InfraredDetect.h"
#include "LightControl.h"
#include "GpioControl.h"
#include "VideoInput.h"
#include "Timer.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <unistd.h>
#include <pthread.h>

#define IR_FEED_GPIO 70
#define IRCUT_INA_GPIO 65
#define IRCUT_INB_GPIO 66

/**
 * @description: 获取是否处于夜间模式
 * @return {*}
 */
int DarkModeStatus(void)
{
    GPIO_LEVEL level;
    GpioLevelGet(IR_FEED_GPIO, &level);
    return level;
}

static void IrCurClose(void *u)
{
    GpioLevelSet(IRCUT_INA_GPIO, GPIO_LEVEL_LOW);
    GpioLevelSet(IRCUT_INB_GPIO, GPIO_LEVEL_LOW);
}

static void InfraredDebounce(void *u)
{
    static int StableLevel = -1;
    if(StableLevel != DarkModeStatus())
    {
        StableLevel = DarkModeStatus();
        VideoSwitchMode(StableLevel);
        // printf("_74hc595dDevStatus()==0======%02x\n",_74hc595dDevStatus());
        InfraredLightControl((TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer)) ? StableLevel : false);
        // printf("_74hc595dDevStatus()==1======%02x    %d    %d    %d   \n",_74hc595dDevStatus(), TimerEnablestatus(MonitorTimer),TimerEnablestatus(CommunicateTimer),StableLevel);
        /* 夜视 */
        if (StableLevel)
        {
            printf("====================================================>>>>>夜视\n\n\n");
            GpioLevelSet(IRCUT_INA_GPIO, GPIO_LEVEL_LOW);
            GpioLevelSet(IRCUT_INB_GPIO, GPIO_LEVEL_HIGH);

#ifdef KEYPAD_ENABLE
            printf("====================================================>>>>>\n\n\n");
            void KeypadLightEnable(void);
            KeypadLightEnable();
#endif
        }
        /* 白天 */
        else
        {
            printf("====================================================>>>>>白天\n\n\n");
#ifdef KEYPAD_ENABLE
            RefreshTimer(10 * 1000, KeypadTimer);
#endif
            GpioLevelSet(IRCUT_INA_GPIO, GPIO_LEVEL_HIGH);
            GpioLevelSet(IRCUT_INB_GPIO, GPIO_LEVEL_LOW);
        }
        
        SetTimer(100, IrCurCloseTimer, IrCurClose, NULL);
    }
}

static int InfraredDetectHandle(int Level)
{
    if(TimerEnablestatus(IrFeedTimer))
    {
        RefreshTimer(1000,IrFeedTimer);
    }
    else
    {
        SetTimer(1000, IrFeedTimer, InfraredDebounce, NULL);
    }
    return 0;
}

/**
 * @description: 夜视检测初始化
 * @return {*}
 */
static bool InfraredDetectInit(void)
{
    if (GpioOpen(IRCUT_INA_GPIO, GPIO_DIR_LOW, false) == false)
    {
        return false;
    }

    if (GpioOpen(IRCUT_INB_GPIO, GPIO_DIR_LOW, false) == false)
    {
        return false;
    }

    if (GpioOpen(70, GPIO_DIR_IN, true) == false)
    {
        return false;
    }

    return true;
}

int InfraredDetectEpollEventInit(struct EpollEvent *Event)
{
    InfraredDetectInit();

    if (GpioOpen(IR_FEED_GPIO, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioEdge(IR_FEED_GPIO, BOTH_EDGE);
    char Path[64] = {0};
    memset(Path, 0, sizeof(Path));
    sprintf(Path, "/sys/class/gpio/gpio%d/value", IR_FEED_GPIO);
    Event->Fd = open(Path, O_RDONLY);
    Event->TriggerLevel = -1;
    Event->EpollEventHandle = InfraredDetectHandle;
    InfraredDetectHandle(DarkModeStatus());
    return 0;
}

static void *InfraredDetectThread(void *arg)
{
    GPIO_LEVEL level = GPIO_LEVEL_LOW;
    GpioLevelGet(70, &level);
    while (1)
    {
        GPIO_LEVEL level_in = GPIO_LEVEL_LOW;
        if (GpioLevelGet(70, &level_in) && level_in != level)
        {
            level = level_in;
            InfraredDetectHandle(level_in);
        }
        usleep(1000 * 500);
    }
    return NULL;
}

/**
 * @description: 红外夜视检测初始化
 * @author hare
 * @return {*}
 */
int InfraredDetectInit_main(void)
{
    if (InfraredDetectInit() == false)
    {
        return 0;
    }
    InfraredDetectHandle(DarkModeStatus());
    pthread_t Thread;
    pthread_create(&Thread, NULL, InfraredDetectThread, NULL);
    pthread_detach(Thread);
    return 0;
}
