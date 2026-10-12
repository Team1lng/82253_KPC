/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-06-06 15:06:15
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-10-23 14:21:37
 * @FilePath: /Doorbell/src/LightControl.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "PeripheralControl.h"
#include "LightControl.h"
#include "GpioControl.h"
#include "Timer.h"
#include <stdio.h>
#include "../common/VideoInput/VideoInput.h"

#define CONTROL_FUNC_DEFINE(LightType, Index) \
    int LightType##LightControl(int en)     \
    {                                       \
        static int EnState = -1;            \
        if (en != EnState)                  \
        {                                   \
            EnState = en;                   \
            char Data = en ? (_74hc595dDevStatus() | (1 << Index)) : (_74hc595dDevStatus() & ~(1 << Index)); \
            _74hc595dDevWrite(Data); \
            return EnState;                 \
        }   \
        return EnState;                     \
    }

#define LIGHT_INIT(LightType, Index)           \
    LightType##LightControl(0);
    
/**
 * @description: 定义灯光控制
 * @return {*}
 */
LIGHT(CONTROL_FUNC_DEFINE)

static void CardLightFlashesHandle(void *us)
{
    static int Light = 1;
    if (TimerEnablestatus(AddCardTimer) || TimerEnablestatus(MasterCardAddTimer))
    {
        CardLightControl((Light = !Light));
        SetTimer(200, CardLightFlashesTimer, CardLightFlashesHandle, NULL);
    }
    else
    {
        CardLightControl(0);
    }
}

/**
 * @description: 刷卡灯光闪烁
 * @return {*}
 */
void CardLightFlashes(void)
{
    if (!TimerEnablestatus(CardLightFlashesTimer))
    {
        CardLightControl(1);
        SetTimer(200, CardLightFlashesTimer, CardLightFlashesHandle, NULL);
    }
}

/**
 * @description: 按键灯光控制
 * @param {int} index 按键索引 [-1 - 全关] [0~3 - 开启]
 * @return {*}
 */
void KeyLightControl(int index)
{   
    char Clear = 0xf0 & _74hc595dDevStatus();   //0xfc
    char Data = index < 0 ? Clear : (Clear | (1 << index));
    _74hc595dDevWrite(Data);
    return;
}

/**
 * @description: 灯光引脚初始化
 * @return {*}
 */
void LightGpioInit(void)
{
    PeripheralControlInit();
    LIGHT(LIGHT_INIT)
}

/****************************************hare set (视频传输时，AI采集标志位设置)*******************************************/
static int network_audio_send_ready = 0;
int is_network_audio_send_package_open(void)
{
	return network_audio_send_ready;
}

void network_audio_send_package_start(void)
{
	printf("==========>>> audio send package start <<<==========\n");
	network_audio_send_ready = 1;
}

void network_audio_send_package_stop(void)
{
	network_audio_send_ready = 0;
	printf("==========>>> audio send package stop <<<==========\n");
}
/****************************************hare set (视频信息传输标志位)********************************************************************/
static int network_video_send_ready = 0;
int is_network_video_send_package_open(void)
{
	return network_video_send_ready;
}

void network_video_send_package_start(void)
{
	printf("==========>>> video send package start <<<==========\n");
	network_video_send_ready = true;
	video_input_open();
	// set_network_i_frame_request_param(true);
}

void network_video_send_package_stop(void)
{
	printf("==========>>> video send package stop <<<==========\n");
	network_video_send_ready = false;
	video_input_close();
}