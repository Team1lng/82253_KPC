/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-29 08:54:35
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-08-06 16:29:30
 * @FilePath: /82225-EPC/src/SecurityMode.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "VoiceRingPlay.h"
#include "NetMsgComm.h"
#include "SecurityMode.h"
#include "LightControl.h"
#include "GpioControl.h"
#include "UserConfig.h"
#include "Timer.h"
#include <stdio.h>

#define SecurityErrorMax 10
static int SecurityErrorCount = 0;

void SecurityErrorReset(void)
{
    SecurityErrorCount = 0;
    TimerDestroy(SecirityErrorTimer);
}

static void SecirityAlarmVoice(void *arg)
{
    // printf("[%s][%d]!!!!!!!!!!!!\n", __func__, TimerEnablestatus(SecirityTriggerTimer));
    if (TimerEnablestatus(SecirityTriggerTimer))
    {
        VoiceRingPlayL(Bi4, VoiceDefVol, NULL, SecirityAlarmVoice);
    }
}

static void LightFlashesHandle(void *us)
{
    // static int Light = 1;   hare
    if (TimerEnablestatus(SecirityTriggerTimer))
    {
        // printf("KeypadLightControl:%d\n", !Light);
        // KeypadLightControl((Light = !Light));   // hare
        SetTimer(500, AlarmLightFlashesTimer, LightFlashesHandle, NULL);
    }
    else
    {
        KeyLightControl(TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer));
    }
}

static void SecirityCloseHandle(void *us)
{
    printf("[%s]!!!!!!!!!!!!\n", __func__);
}

static void SecirityErrorHandle(void *us)
{
    SecurityErrorCount = 0;
}

/**
 * @description: 安全模式触发
 * @param {int} TriggerType 触发类型
 * @return {*}
 */
int SecurityModeTrigger(int TriggerType)
{
    if (TimerEnablestatus(SecirityTriggerTimer))
    {
        return 0;
    }

    int TriggerTime = (TriggerType == LockMode ? 120 : 60) * 1000;
    NetworkMsgData Data;
    Data.Device = DEVICE_ALL;
    Data.Cmd = DoorbellEvent;
    Data.Arg1 = 1;
    Data.Arg2 = 1;
    NetworkMsgSned(Data);
    RefreshTimer(1, SecirityErrorTimer);
    SetTimer(TriggerTime, SecirityTriggerTimer, SecirityCloseHandle, NULL);
    SetTimer(200, AlarmLightFlashesTimer, LightFlashesHandle, NULL);
    printf("[%s]!!!!!!!!!!!!\n", __func__);
    if (TriggerType == AlarmMode)
    {
        SecirityAlarmVoice(NULL);
    }
    return 1;
}

/**
 * @description: 更新安全模式试错次数
 * @return {*}
 */
int SecurityErrorUpdate(void)
{
    if (UserConfigGet()->SafeMode == CloseSafe)
    {
        return 0;
    }
    if (!SecurityErrorCount)
    {
        if (!TimerEnablestatus(SecirityErrorTimer))
            SetTimer((UserConfigGet()->SafeMode == LockMode ? 120 : 60) * 1000, SecirityErrorTimer, SecirityErrorHandle, NULL);
    }
    ++SecurityErrorCount;
    printf("[%s] - %d\n", __func__, SecurityErrorCount);
    if (SecurityErrorCount == SecurityErrorMax)
    {
        SecurityErrorCount = 0;
        SecurityModeTrigger(UserConfigGet()->SafeMode);
    }
    return 1;
}