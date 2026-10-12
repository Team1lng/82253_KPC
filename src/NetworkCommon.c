#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "AudioOutput.h"
#include "NetworkCommon.h"
#include "NetMsgComm.h"
#include "GeneralInterface.h"
#include "SmartVisionPlatform.h"
#include "InfraredDetect.h"
#include "LightControl.h"
#include "Fingerprint.h"
#include "DeviceUpgrade.h"
#include "VoiceRingPlay.h"
#include "VoiceDecode.h"
#include "UserConfig.h"
#include "AdcDetect.h"
#include "Timer.h"
#include "Unlock.h"
#include "GpioControl.h"

#define TALKING_LIGHT_CONTROL_GPIO 34    //hare
/**********************************************************网络消息处理************************************************************8*/

#define EventList(EVENT)       \
    EVENT(IdRepeatEvent)       \
    EVENT(UnlockEvent)         \
    EVENT(StreamStatusEvent)   \
    EVENT(OutdoorTalkEvent)    \
    EVENT(OutdoorHangEvent)    \
    EVENT(OutdoorResetEvent)   \
    EVENT(UpgraedOutdoorEvent) \
    EVENT(MotionSensitivityEvent)

#define DefineEventFunc(_EVENT) [_EVENT].proc = _EVENT##Func, [_EVENT].str = #_EVENT,

static void IdRepeatEventFunc(NetworkMsgPacket Packet)
{
    static int HeratCount = 0;
    struct timespec time;
    // struct tm t = FetchCompileTime();
    if (DiffClockTimeMs(&time) > 1000)
    {
        GetClockTimeMs(&time);
        NetworkMsgData Data;
        HeratCount = (HeratCount + 1) % 2;
        switch (HeratCount)
        {
        case 0:
            Data.Device = DEVICE_ALL;
            Data.Cmd = StreamStatusEvent;
            Data.Arg1 = (0 << 4) | (FINGER_ENABLE_STATUE << 2) | TimerEnablestatus(SVPTimer) | (TimerEnablestatus(CommunicateTimer) << 1); // 通知移动侦测结果
            Data.Arg2 = DOOR_CAMERA_MODEL;
            break;
        case 1:
            Data.Device = DEVICE_ALL;
            Data.Cmd = CompileTimeEvent;
            unsigned int ver = MAJOR_VER * 10000 + MINOR_VER * 100 + PATCH_VER;
            Data.Arg1 = (char)(ver & 0xFF);
            Data.Arg2 = (char)((ver >> 8) & 0xFF);
            break;

        case 2:
            /* code */
            break;
        default:
            break;
        }
        NetworkMsgSned(Data);
    }
}

static void UnlockEventFunc(NetworkMsgPacket Packet)
{
#define UNLOCK_VOICE_INDEX (UnlockEng + ((Packet.Data.Arg[1] & 0x3C) >> 2))
#define UNLOCK_TIME (Packet.Data.Arg[0])
#define UNLOCK_TYPE ((Packet.Data.Arg[1] & 0x03))
    // VoiceRingPlay(UNLOCK_VOICE_INDEX, 100);
    int TmpLanguage = UserConfigGet()->Language;
    UserConfigGet()->Language = ((Packet.Data.Arg[1] & 0x3C) >> 2);
    Unlock(UNLOCK_TIME, UNLOCK_TYPE);
    UserConfigGet()->Language = TmpLanguage;
    UserConfigSave();
}

static void CommunicateOuttime(void *us)
{
    KeyLightControl(-1);
    // TalkLightControl(0);   // hare
    GpioLevelSet(TALKING_LIGHT_CONTROL_GPIO, GPIO_LEVEL_LOW); //hare
    InfraredLightControl(0);
}

extern int network_stream_count;
static void StreamStatusEventFunc(NetworkMsgPacket Packet)
{
    extern void VideoKeyFrameRequest(void);
#define KEY_FRAME_REQUEST (!(Packet.Data.Arg[0] & 0x01))
#define LEAVE_MESSAGE_ENABLE (Packet.Data.Arg[0] & 0x02)
#define TUYA_MONIOTR_ENABLE (Packet.Data.Arg[0] & 0x08)
#define AUDIO_TALK_VOLUME ((Packet.Data.Arg[1]) * 3 + 66)

    network_stream_count = 0;
    if (is_network_audio_send_package_open() == 0)   //hare set
    {
        network_audio_send_package_start();
    }

    if (is_network_video_send_package_open() == 0)
	{
        network_video_send_package_start();
    }

    if (KEY_FRAME_REQUEST)
    {
        VideoKeyFrameRequest();
    }

    if (LEAVE_MESSAGE_ENABLE)
    {
#define LEAVE_MSG_VOICE_INDEX (LeaveMsgEng + (Packet.Data.Arg[0] >> 2))
        static struct timespec time;
        if (DiffClockTimeMs(&time) > 3 * 1000)
        {
            VoiceRingPlay(LEAVE_MSG_VOICE_INDEX, 100);
        }
        GetClockTimeMs(&time);
    }
    else if (TUYA_MONIOTR_ENABLE)
    {
        static char CommDev;
        if (!TimerEnablestatus(CommunicateTimer))
        {
            // InfraredLightControl(DarkModeStatus());
            CommDev = Packet.SendDev;
            SetTimer(5000, CommunicateTimer, CommunicateOuttime, &CommDev);
            TimerDestroy(MonitorTimer);

            // TalkLightControl(1);    //hare
            GpioLevelSet(TALKING_LIGHT_CONTROL_GPIO, GPIO_LEVEL_HIGH);
            InfraredLightControl(DarkModeStatus());
        }
    }

    if (TimerEnablestatus(CommunicateTimer))
    {
        char *ConmmDevice = TimerGet(CommunicateTimer)->Data;
        if (ConmmDevice && *ConmmDevice == Packet.SendDev)
        {
            RefreshTimer(5000, CommunicateTimer);
        }
        AudioOutputVolumeSet(AUDIO_TALK_VOLUME);
    }
    else if (!TimerEnablestatus(MonitorTimer))
    {
        if (TimerEnablestatus(CallBusyTimer))
        {
            TimerDestroy(CallBusyTimer);
        }

        // InfraredLightControl(DarkModeStatus());
        VideoKeyFrameRequest();
        SetTimer(5000, MonitorTimer, CommunicateOuttime, NULL);
        InfraredLightControl(DarkModeStatus());
    }
    else
    {
        RefreshTimer(5000, MonitorTimer);
    }
}

static void OutdoorTalkEventFunc(NetworkMsgPacket Packet)
{
    static char CommDev;
    // char CommFloor = Packet.Data.Arg[1]; hare
    char CommCh = Packet.Data.Arg[0];
    if (!TimerEnablestatus(CommunicateTimer) && (DEVICE_OUTDOOR_1+CommCh-1) == NetLocalDeviceIDGet())
    {
        CommDev = Packet.SendDev;
        SetTimer(5000, CommunicateTimer, CommunicateOuttime, &CommDev);
        TimerDestroy(MonitorTimer);
        InfraredLightControl(DarkModeStatus());

        // TalkLightControl(1);   // hare
        GpioLevelSet(TALKING_LIGHT_CONTROL_GPIO, GPIO_LEVEL_HIGH);
        // CallKeyLightCtrl(CommFloor);  hare
    }
}

static void OutdoorHangEventFunc(NetworkMsgPacket Packet)
{
}

static void OutdoorResetEventFunc(NetworkMsgPacket Packet)
{
    UserConfigReset();
}

static void UpgraedOutdoorEventFunc(NetworkMsgPacket Packet)
{
#define UpgradeLongPack (Packet.DataLen > 8)
#define CheckOnlineStatus (Packet.Data.Arg[0] & 0x01)
#define UpdateOver (Packet.Data.Arg[0] & 0x02)
#define UpdataFinish (Packet.Data.Arg[1] & 0x01)
#define UpdataFail (Packet.Data.Arg[1] & 0x02)
    if (UpgradeLongPack)
    {
        // printf("[%s]DataLen:%d\n", __func__, Packet.DataLen);
        int arg1 = Packet.Data.DP[0] << 24 | Packet.Data.DP[1] << 16 | Packet.Data.DP[2] << 8 | Packet.Data.DP[3];
        // buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];

        int arg2 = Packet.Data.DP[4] << 24 | Packet.Data.DP[5] << 16 | Packet.Data.DP[6] << 8 | Packet.Data.DP[7];
        // buf[8] << 24 | buf[9] << 16 | buf[10] << 8 | buf[11];
        if (ReceiveUpgradePack(arg1, arg2, &Packet.Data.DP[8]) == 0)
        {
            NetworkMsgData Data;
            Data.Device = Packet.SendDev;
            Data.Cmd = UpgraedOutdoorEvent;
            Data.Arg1 = 3; // 升级失败
            Data.Arg2 = 1;
            NetworkMsgSned(Data);
        }
    }
    else
    {
        // printf("[%s]Arg0:%d,Arg1:%d\n", __func__, Packet.Data.Arg[0], Packet.Data.Arg[1]);
        if (CheckOnlineStatus)
        {
            NetworkMsgData Data;
            Data.Device = Packet.SendDev;
            Data.Cmd = UpgraedOutdoorEvent;
            Data.Arg1 = 1;
            Data.Arg2 = 1;
            NetworkMsgSned(Data);
        }
        else if (UpdateOver)
        {
            if (UpdataFinish)
            {
                NetworkMsgData Data;
                Data.Device = Packet.SendDev;
                Data.Cmd = UpgraedOutdoorEvent;
                Data.Arg1 = 2;
                Data.Arg2 = 1;
                VoiceRingPlay(Unlock16k, VoiceDefVol);
                sleep(2);
                if (!UpgradeProcess(1))
                {
                    Data.Arg1 = 3;
                    UpgradeProcess(0);
                }
                NetworkMsgSned(Data);
            }
            else if (UpdataFail)
            {
                UpgradeProcess(0);
            }
        }
    }
}

static void MotionSensitivityEventFunc(NetworkMsgPacket Packet)
{
    int SvpSensit[][2] = {
        {0, 0},
        {18000, 8000},
        {10000, 5000},
        {7000, 3500},
    };
    int Sensitivity = Packet.Data.Arg[0];
    if (Sensitivity < sizeof(SvpSensit) / sizeof(SvpSensit[0]))
    {
        Debug("Sensitivity:%d\n", Sensitivity);
        SvpMdFiltersSet(SvpSensit[Sensitivity][0], SvpSensit[Sensitivity][1]);
    }
}

/**
 * @description: 通话灯引脚初始化
 * @return {*}
 */
void LockGpioInit(void)
{
        if (GpioOpen(TALKING_LIGHT_CONTROL_GPIO, GPIO_DIR_LOW, false))
        {
        }
}

NetworkEventHandle HandleFuncGroup[TotalEvent] = {EventList(DefineEventFunc)};
