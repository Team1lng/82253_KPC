/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-28 13:44:27
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-09-11 10:53:49
 * @FilePath: /project_3/src/SC92F836Keypad.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "DrvNumericKeypad.h"
#include "GeneralInterface.h"
#include "NumericKeypad.h"
#include "UserNetManage.h"
#include "VoiceRingPlay.h"
#include <sys/ioctl.h>
#include "Timer.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define SC92F836_DRIVE_PATH "/usr/modules/SC92F836.ko"
#define SIMULATED_I2C_DRIVE_PATH "/usr/modules/sim_idrv.ko"
#define SIMULAT_I2C_DEV_PATH "/dev/SIMULAT_I2C"
#define SC92F836_DEV_PATH "/dev/sc92f836"
#define CONTROL_INTERRUPT_CMD 0xABCD

static Keyboard Keypad;
static char KeyValue[] = {
    [0x11] = 1,
    [0x10] = 2,
    [0x09] = 3,
    [0x13] = 4,
    [0x12] = 5,
    [0x04] = 6,
    [0x15] = 7,
    [0x16] = 8,
    [0x03] = 9,
    [0x17] = KEYSTAR,
    [0x18] = 0,
    [0x02] = KEYPOUND,
    [0xAA] = MAILBOX_PUT_IN,
    [0xAB] = MAILBOX_TAKE_OUT,
    [0xAC] = MAILBOX_ALARM,
    };

static int KeypadBuffClear(void)
{
    Keypad.Cursor = 0;
    return 0;
}

static int KeypadDataVerify(char Data)
{
    if (Keypad.Cursor && DiffClockTimeMs(&Keypad.Time) > 30000)
    {
        Keypad.Cursor = 0;
    }
    GetClockTimeMs(&Keypad.Time);
    if (Keypad.Cursor >= sizeof(Keypad.Buff))
    {
        Keypad.Cursor = 0;
        return -1;
    }
    if (Data == KEYPOUND)
    {
        Keypad.Buff[Keypad.Cursor++] = '#';
    }
    else if (Data == KEYSTAR)
    {
        if (Keypad.Cursor)
        {
            Keypad.Cursor--;
            return 0;
        }
        else
        {
            Keypad.Buff[Keypad.Cursor++] = '*';
        }
    }
    else
    {
        Keypad.Buff[Keypad.Cursor++] = Data + 48;
        return 0;
    }
    return 1;
}

/**********************************************************弱函数重定义*************************************************************/
int SC92F836ModuleInit(int *DrvFd, int *DrvLen)
{
    if (access(SC92F836_DRIVE_PATH, F_OK) != 0)
    {
        printf("%s no exist\n", SC92F836_DRIVE_PATH);
        return -1;
    }

    if (access(SIMULAT_I2C_DEV_PATH, F_OK) != 0)
        system("insmod " SIMULATED_I2C_DRIVE_PATH);

    usleep(1000 * 10);

    if (access(SC92F836_DEV_PATH, F_OK) != 0)
        system("insmod " SC92F836_DRIVE_PATH);

    *DrvFd = open(SC92F836_DEV_PATH, O_RDWR);
    if (*DrvFd < 0)
    {
        return -1;
    }

    // 设置中断配置
    if (ioctl(*DrvFd, CONTROL_INTERRUPT_CMD, 1) < 0)
    {
        perror("ioctl");
    }

    NumericKeypadInit();
    *DrvLen = 1;
    memset(&Keypad, 0, sizeof(Keypad));

    printf("%s  Succeed!!!\n", __func__);
    return 0;
}

int SC92F836ModuleHandle(char *Data)
{
    // printf("0x%02x\n", Data[0]);
    // return 1;
    if (Data[0] >= sizeof(KeyValue))
        return -1;
    if (TimerEnablestatus(SecirityTriggerTimer))
        return -1;
    if (VoiceDecodeStatus())
        return -1;

    if (IsNetManageEntryState())
        return -1;

    int ret = KeypadDataVerify(KeyValue[(int)Data[0]]);
    KeypadLightEnable();
    if (ret < 0)
    {
        printf("Verify Fail!!!!\n");
        VoiceRingPlay(Bi4, VoiceDefVol);
        return 0;
    }
    else
    {
        VoiceRingPlay(Dio, VoiceDefVol);
        if (!ret)
        {
            printf("[");
            for (int i = 0; i < KEYPAD_BUFFER_SIZE; i++)
            {
                if (i < Keypad.Cursor)
                    printf("%c", Keypad.Buff[i]);
                else
                    printf(" ");
            }
            printf("]\r");
            fflush(stdout);
        }
        else
        {
            KeypadProess(&Keypad);
            printf("\r\n");
            KeypadBuffClear();
        }
    }
    return 0;
}
/***********************************************************************************************************************/
