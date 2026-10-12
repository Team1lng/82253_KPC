/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-14 19:43:25
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-07-27 08:26:31
 * @FilePath: /project_3/common/AudioOutput/AudioOutput.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <unistd.h>
#include <pthread.h>
#include "AudioInput.h"
#include "ak_common_audio.h"
#include "ak_common.h"
#include "ak_ai.h"
#include <math.h>
#include <string.h>
#include <stdatomic.h>

#include "../../src/LightControl.h"
#include <stdbool.h>
#include <stdio.h>

#define PCM_SIZE_MAX 4096

struct
{
    int HandleId;
    struct ak_audio_in_param param;
    atomic_int AIThreadState;
} AI = {
    .HandleId = -1,
    .AIThreadState = ATOMIC_VAR_INIT(-1),
    .param.dev_id = DEV_ADC,
    .param.pcm_data_attr.sample_rate = AK_AUDIO_SAMPLE_RATE_16000,
    .param.pcm_data_attr.channel_num = AUDIO_CHANNEL_MONO,
    .param.pcm_data_attr.sample_bits = AK_AUDIO_SMPLE_BIT_16};

/**
 * @description: 获取输入音频数据 [需要函数重定义]
 * @param {AudioOutputFrame} frame   节点信息
 * @return {*}
 */
__attribute__((weak)) int AIDataIxport(AudioInputFrame *frame)
{
    printf("This is the weak AIDataIxport function.\n");
    return -1;
}

/**
 * @description: 设置音频输入参数
 * @param {int} HandleId 音频输入句柄
 * @return {*}
 */
__attribute__((weak)) int SetupAudioInputArgument(int HandleId)
{
    struct ak_audio_nr_attr default_ai_nr_attr = {-40, 0, 1};
    struct ak_audio_agc_attr default_ai_agc_attr = {24576, 4, 0, 80, 0, 1};
    struct ak_audio_aec_attr default_ai_aec_attr = {0, 1024, 1024, 0, 512, 1, 9829};
    struct ak_audio_aslc_attr default_ai_aslc_attr = {22937, 0, 0};
    struct ak_audio_eq_attr default_ai_eq_attr = {
        0,
        1,
        {5000, 5300, 125, 250, 500, 1000, 2000, 4000, 8000, 16000},
        {-6144, -12288, 0, 0, 0, 0, 0, 0, 0, 0},
        {716, 1024, 717, 717, 717, 717, 717, 717, 717, 717},
        {TYPE_HSF, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1, TYPE_PF1},
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

    ak_ai_set_nr_attr(HandleId, &default_ai_nr_attr);

    ak_ai_set_agc_attr(HandleId, &default_ai_agc_attr);

    ak_ai_set_aec_attr(HandleId, &default_ai_aec_attr);

    ak_ai_set_aslc_attr(HandleId, &default_ai_aslc_attr);

    ak_ai_set_eq_attr(HandleId, &default_ai_eq_attr);

    ak_ai_set_gain(HandleId, 3);
    ak_ai_set_volume(HandleId, 0);
    return 0;
}

static int AudioInputOpen(void)
{
    if (ak_ai_open(&AI.param, &AI.HandleId))
    {
        printf("ak_ao_open failed\n");
        return -1;
    }

    SetupAudioInputArgument(AI.HandleId);

    if (ak_ai_start_capture(AI.HandleId))
    {
        printf("ak_ai_start_capture failed\n");
        return -1;
    }
    printf("ak_ai_open succeed!!!\n");
    return 0;
}


/***
**	function: 关闭实际的音频输出设备
**	Author  : hare
**  time    : 2025.11.07.1712
***/
static bool audio_input_device_close(void)
{
	printf("%s $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n", __func__);
	atomic_store(&AI.AIThreadState, -1);
    ak_ai_stop_capture(AI.HandleId);
    ak_ai_close(AI.HandleId);
    AI.HandleId = -1;
	return true;
}

static void *AudioInputThread(void *arg)
{
    struct frame frame = {0};
    bool ai_capture = true;

    atomic_store(&AI.AIThreadState, 1);
    while (atomic_load(&AI.AIThreadState))
    {
        if (is_network_audio_send_package_open() == 1 && ai_capture == false)   
		{
			if (AudioInputOpen() == -1)
            {
                printf("audio_input_device_open_fail\n");
            }else{
                printf("audio_input_device_open_success\n");
                ai_capture = true;
            }
            
		}
		else if (is_network_audio_send_package_open() == 0 && ai_capture == true)
		{
			printf("audio_input_device_close\n");
			audio_input_device_close();
			ai_capture = false;
		}

        else if (ai_capture){
            // printf("--------------------\n");
            if (ak_ai_get_frame(AI.HandleId, &frame, 1) == 0)
            {
                if (!frame.data || frame.len <= 0)
                {
                    usleep(1000 * 10);
                    continue;
                }
                static AudioInputFrame Frame;
                Frame.Data = frame.data;
                Frame.Len = frame.len;
                Frame.SeqNo++;
                AIDataIxport(&Frame);
                ak_ai_release_frame(AI.HandleId, &frame);
            }
        }
       // printf("===========================\n");
        usleep(1000 * 1);
    }
    return NULL;
}
/**
 * @description: 初始化音频输出
 * @return {*}
 */
int AudioInputInit(void)
{
    if (AI.HandleId != -1)
    {
        return 0;
    }

    if (AudioInputOpen() == -1)
    {
        return -1;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, AudioInputThread, NULL) != 0)
    {
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

/**
 * @description: 反初始化音频输入
 * @return {*}
 */
int AudioInputDeinit(void)
{
    if (atomic_load(&AI.AIThreadState) == 1)
    {
        atomic_store(&AI.AIThreadState, 0);
    }
    return 0;
}

/**
 * @description: 音频输入线程状态
 * @return {*}
 */
int AudioInputThreadState(void)
{
    return (atomic_load(&AI.AIThreadState) == 1);
}