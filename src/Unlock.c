#include "PeripheralControl.h"
#include "GeneralInterface.h"
#include "EpollGpioEvent.h"
#include "VoiceRingPlay.h"
#include "LightControl.h"
#include "GpioControl.h"
#include "UserConfig.h"
#include "Unlock.h"
#include "Timer.h"
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

// #define LOCK_GPIO 34
// #define GATE_GPIO 32
// static char LockGpio[] = {0, LOCK_GPIO, GATE_GPIO};
static void GateColse(void *u)
{
    printf("[%s]\n", __func__);
    // GpioLevelSet(GATE_GPIO, GPIO_LEVEL_LOW);
    GateopenLightControl(0);
    if (!TimerEnablestatus(LockTimer))
    {
        // KeyLightControl(TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer));
    }
}

static void LockColse(void *u)
{
    printf("[%s]\n", __func__);
    // GpioLevelSet(LOCK_GPIO, GPIO_LEVEL_LOW);
    DooropenLightControl(0);
    if (!TimerEnablestatus(GateTimer))
    {
        // KeyLightControl(TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer));
    }
}

static int LockExitHandle(int Level)
{
    static struct timespec time;
    if (DiffClockTimeMs(&time) > 1 * 1000)
    {
        printf("[%s]Trigger:%d\n\n", __func__, Level);
        Unlock(UserConfigGet()->UnlockTime, LOCK_TYPE);
    }
    GetClockTimeMs(&time);
    return 0;
}
int LockExitEpollEventInit(struct EpollEvent *Event)
{
#define LOCK_EXIT_GPIO 25
    if (GpioOpen(LOCK_EXIT_GPIO, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioEdge(LOCK_EXIT_GPIO, FALLING_EDGE);
    char Path[64] = {0};
    memset(Path, 0, sizeof(Path));
    sprintf(Path, "/sys/class/gpio/gpio%d/value", LOCK_EXIT_GPIO);
    Event->Fd = open(Path, O_RDONLY);
    Event->TriggerLevel = 0;
    Event->EpollEventHandle = LockExitHandle;
    return 0;
}

static int GateExitHandle(int Level)
{
    static struct timespec time;
    if (DiffClockTimeMs(&time) > 1 * 1000)
    {
        // printf("[%s]Trigger:%d\n\n", __func__, Level);
        Unlock(UserConfigGet()->UngateTime, GATE_TYPE);
    }
    GetClockTimeMs(&time);
    return 0;
}
int GateExitEpollEventInit(struct EpollEvent *Event)
{
#define GATE_EXIT_GPIO 27
    if (GpioOpen(GATE_EXIT_GPIO, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioEdge(GATE_EXIT_GPIO, FALLING_EDGE);
    char Path[64] = {0};
    memset(Path, 0, sizeof(Path));
    sprintf(Path, "/sys/class/gpio/gpio%d/value", GATE_EXIT_GPIO);
    Event->Fd = open(Path, O_RDONLY);
    Event->TriggerLevel = 0;
    Event->EpollEventHandle = GateExitHandle;
    return 0;
}

/**
 * @description: 开锁
 * @param {int} time    开锁时间
 * @param {LockType} type   开锁类型
 * @return {*}0-失败，1-成功
 */
int Unlock(int time, LockType type)
{
    assert(type <= GATE_TYPE);
    if (SetTimer(time * 1000, type == LOCK_TYPE ? LockTimer : GateTimer, type == LOCK_TYPE ? LockColse : GateColse, NULL))
    {
        // printf("================%d    ===================%d\n",TimerEnablestatus(type == LOCK_TYPE ? GateTimer : LockTimer),UserConfigGet()->UnlockVoiceEn);
        if (!TimerEnablestatus(type == LOCK_TYPE ? GateTimer : LockTimer) /* && UserConfigGet()->UnlockVoiceEn */)  //hare
        {
            VoiceRingPlay(26, VoiceDefVol); //30
        }
        printf("[%s] %ds\n", type == LOCK_TYPE ? "Unlock" : "Ungate", time);
        // GpioLevelSet(LockGpio[type], GPIO_LEVEL_HIGH);
        if (type == 1)
        {
            DooropenLightControl(1);
        }
        else if (type == 2)
        {
            GateopenLightControl(1);
        }
        char Data[] = {0x7e, 0xa5, 0x97};
        WiegandDevWrite(Data, sizeof(Data));
        // KeyLightControl(!(TimerEnablestatus(MonitorTimer) || TimerEnablestatus(CommunicateTimer)));
        return 1;
    }
    return 0;
}

/**
 * @description: 锁引脚初始化
 * @return {*}
 */
// void LockGpioInit(void)
// {
//     for (int i = LOCK_TYPE; i < sizeof(LockGpio); i++)
//     {
//         if (GpioOpen(LockGpio[i], GPIO_DIR_LOW, false))
//         {
//         }
//     }
// }


