/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-04 15:08:22
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-23 08:10:30
 * @FilePath: /project_3/common/Fingerprint/Fingerprint.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "GeneralInterface.h"
#include "UserFingerprint.h"
#include "UserNetManage.h"
#include "EpollGpioEvent.h"
#include "VoiceRingPlay.h"
#include "GpioControl.h"
#include "UartControl.h"
#include "CircularList.h"
#include "UserConfig.h"
#include "Fingerprint.h"
#include <pthread.h>
#include "Unlock.h"
#include <unistd.h>
#include <string.h>
#include "assert.h"
#include "Timer.h"
#include <fcntl.h>
#include <sys/prctl.h>
#include <stddef.h>

#define FINGER_DEBUG
#ifdef FINGER_DEBUG
#define DebugLog(format, ...) printf("\033[0;33;40m" format "\033[0m", ##__VA_ARGS__)
#define DebugInfo(format, ...) \
    printf("[%s][%d]");        \
    DebugLog(format, ...)

#else
#define DebugLog(format, ...)
#endif

#define RETURN_TIMEOUT 2000

static void GetImageHandle(void *u);

/* ****************************************************************** 指纹事件注册接口 *********************************************************** */
DECLARE_LIST(FingerEventList)

/**
 * @description: 指纹事件注册
 * @return {*}
 */
int FingerEventRegister(FingerEvent *Event)
{
    assert(Event != NULL);
    ListNode *Node = CircularListRequest(&FingerEventList);
    if (Node != NULL)
    {
        Node->Data = Event;
        // printf("[%s] Event:%p\n", __func__, Event);
        atomic_store(&(Event->EvRelease), 0);
        if (CircularListWrite(&FingerEventList, Node) == -1)
        {
            printf("[%s] FingerEventList Write Fail\n", __func__);
            goto fail;
        }
        return 1;
    }

fail:
    atomic_store(&(Event->EvRelease), 1);
    return 0;
}

/* ****************************************************************** 指纹模块操作接口 *********************************************************** */
static int UartFd = -1;

static bool FingerReadData(FingerDataAck *Buffer)
{
    int ReadLen = 0;
    int timeout = 100;
    int FingerHeadLen = sizeof(FingerHead);
    memset(Buffer, 0, sizeof(FingerDataAck));
    if (UartBufferSize(UartFd) >= FingerHeadLen && (ReadLen = UartRead(UartFd, (char *)&(Buffer->Format), FingerHeadLen)) == FingerHeadLen) // 读取包头至包长度的数据
    {
        if (PACKAGE_HEADER == (uint16_t)((Buffer->Format.Header[0] << 8) | Buffer->Format.Header[1]))
        {
            if ((Buffer->Format.Addr[0] & Buffer->Format.Addr[1] & Buffer->Format.Addr[2] & Buffer->Format.Addr[3]) != 0xFF)
                return false;

            if (Buffer->Format.Type != DATA_PACKAGE && Buffer->Format.Type != ACK_PACKAGE && Buffer->Format.Type != END_PACKAGE)
                return false;

            uint16_t PackLen = (((uint16_t)(Buffer->Format.Len[0]) << 8) | (uint16_t)(Buffer->Format.Len[1] & 0xFF));

            /* 等待数据完整传输 */
            while (UartBufferSize(UartFd) < PackLen && timeout--)
            {
                usleep(1000);
            }

            { /* 包长度不可大于接收缓存，否则会内存溢出，其次按协议分析也代表包数据是错误的 */
                size_t CacheOffset;
                if (Buffer->Format.Type != ACK_PACKAGE)
                    CacheOffset = offsetof(FingerDataAck, Data);
                else
                    CacheOffset = offsetof(FingerDataAck, Affirm);

                int CacheLen = sizeof(FingerDataAck) - CacheOffset;
                DebugLog("Receive Format:");
                DebugLog("[ %2x ][ %2x ]", Buffer->Format.Header[0], Buffer->Format.Header[1]);
                DebugLog("[ %2x ][ %2x ][ %2x ][ %2x ]", Buffer->Format.Addr[0], Buffer->Format.Addr[1], Buffer->Format.Addr[2], Buffer->Format.Addr[3]);
                DebugLog("[ %2x ]", Buffer->Format.Type);
                DebugLog("[ %2x ][ %2x ]\n", Buffer->Format.Len[0], Buffer->Format.Len[1]);
                if (PackLen > CacheLen)
                {
                    DebugWarning("Data Error,Data out of cache size!!!!,PackLen:%d,CacheLen:%d,UartBufferSize:%d\n", PackLen, CacheLen, UartBufferSize(UartFd));
                    return false;
                }
            }

            if ((UartRead(UartFd, Buffer->Format.Type != ACK_PACKAGE ? (char *)Buffer->Data : (char *)&(Buffer->Affirm), PackLen)) != PackLen)
                return false;

            uint32_t CheckSum = Buffer->Format.Type + PackLen + Buffer->Affirm;

            uint16_t DataLen = Buffer->Format.Type == ACK_PACKAGE ? PackLen - AFFIRM_CODE_LEN : PackLen;

            if ((DataLen - CHECK_CODE_LEN) >= (sizeof(Buffer->Data) / sizeof(uint8_t)))
            {
                return false;
            }

            for (int i = 0; i < DataLen - CHECK_CODE_LEN; i++)
            {
                CheckSum += Buffer->Data[i];
            }

            DebugLog("Receive Data:");
            // DebugLog("[ %2x ]", Buffer->Format.Type);
            // DebugLog("[ %2x ][ %2x ]", Buffer->Format.Len[0], Buffer->Format.Len[1]);
            DebugLog("[ %2x ]", Buffer->Affirm);

            for (int i = 0; i < DataLen - CHECK_CODE_LEN; i++)
            {
                DebugLog("[ %2x ]", Buffer->Data[i]);
            }
            DebugLog("[ %2x ][ %2x ]", Buffer->Data[DataLen - 2], Buffer->Data[DataLen - 1]);
            DebugLog("\n");
            if (CheckSum == (((uint16_t)(Buffer->Data[DataLen - 2]) << 8) | (uint16_t)(Buffer->Data[DataLen - 1] & 0xFF)))
            {
                Buffer->CheckSum[0] = Buffer->Data[DataLen - 2];
                Buffer->CheckSum[1] = Buffer->Data[DataLen - 1];

                return true;
            }
        }
    }
    else if (ReadLen > 0)
    {
        DebugLog("RECEIVE DATA ERROR LEN: %d ", ReadLen);
        DebugLog("[ %2x ][ %2x ]", Buffer->Format.Header[0], Buffer->Format.Header[1]);
        DebugLog("[ %2x ][ %2x ][ %2x ][ %2x ]\n", Buffer->Format.Addr[0], Buffer->Format.Addr[1], Buffer->Format.Addr[2], Buffer->Format.Addr[3]);
        DebugLog("\n");
    }
    return false;
}

static void FingerWriteCmd(uint8_t Type, uint8_t Cmd, uint8_t *Data, uint16_t Size)
{
    UartClear(UartFd);

    uint16_t DataLen = FINGER_BASE_DATA_LEN + Size;
    uint8_t Buffer[DataLen];
    uint32_t CheckSum = 0;
    Buffer[0] = (PACKAGE_HEADER >> 8) & 0xFF;
    Buffer[1] = PACKAGE_HEADER & 0xFF;
    Buffer[2] = (DEVICE_ADDR >> 24) & 0xFF;
    Buffer[3] = (DEVICE_ADDR >> 16) & 0xFF;
    Buffer[4] = (DEVICE_ADDR >> 8) & 0xFF;
    Buffer[5] = DEVICE_ADDR & 0xFF;
    Buffer[6] = Type;
    Buffer[7] = ((Size + 3) >> 8) & 0xFF;
    Buffer[8] = (Size + 3) & 0xFF;
    Buffer[9] = Cmd;
    CheckSum = Type + (Size + 3) + Cmd;
    for (int i = 0; i < Size; i++)
    {
        Buffer[10 + i] = Data[i];
        CheckSum += Data[i];
    }
    Buffer[DataLen - 2] = (CheckSum >> 8) & 0xFF;
    Buffer[DataLen - 1] = CheckSum & 0xFF;
    UartWrite(UartFd, (char *)Buffer, DataLen);

    DebugLog("\n\rSend Data :");
    for (int i = 0; i < DataLen; i++)
    {
        DebugLog("[ %x ]", Buffer[i]);
    }
    DebugLog("\n");
}

#if 1
static bool Cancel(void)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_CANCEL, NULL, 0);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("指令中断退出%s\n", Buffer.Affirm == 0x00 ? "成功" : "失败");
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("指令中断退出失败\n");
    return false;
}
#endif

static uint16_t ValidTemplateNum(void)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_VALID_TEMPLATE_NUM, NULL, 0);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            uint16_t Nmber = (((uint16_t)Buffer.Data[0] << 8) | ((uint16_t)Buffer.Data[1]));
            Debug("有效模板数量获取%s 个数:%d\n", Buffer.Affirm == 0x00 ? "成功" : "失败", Nmber);
            return Buffer.Affirm == 0x00 ? Nmber : 0x00;
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("有效模板数量获取失败\n");
    return 0xff;
}

static uint16_t AutoVerifyFinger(uint16_t Id)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);

    uint8_t Data[5];
    Data[0] = 0x02;
    Data[1] = (Id >> 8) & 0xFF;
    Data[2] = Id & 0xFF;
    Data[3] = 0x00;
    Data[4] = 0b00000010;
    FingerWriteCmd(CMD_PACKAGE, CODE_AUTO_IDENTIFY, Data, 5);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            if (Buffer.Affirm == 0x00 && Buffer.Data[0] == 0x05)
            {
                Debug("比对成功\n");
                return (((uint16_t)Buffer.Data[1] << 8) | ((uint16_t)Buffer.Data[2]));
            }
            else if (Buffer.Affirm == 0x00 && Buffer.Data[0] == 0x00)
                Debug("指令合法性检测成功\n");
            else if (Buffer.Affirm == 0x00 && Buffer.Data[0] == 0x01)
                Debug("录入指纹获取图像成功\n");
            else{
                // printf("------------------time----%llu\n",DiffClockTimeMs(&Time));
                break;
            }
                
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            // printf("------------------time-in---%llu\n",DiffClockTimeMs(&Time));
            break;
        }
        // printf("------------------time-out---%llu\n",DiffClockTimeMs(&Time));
        usleep(1000);
    }
    Debug("比对失敗:0x%x\n", Buffer.Data[0]);
    return 0xFFFF;
}

static bool AutoEnrool(uint16_t Id)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);

    uint8_t Data[5];
    Data[0] = (Id >> 8) & 0xFF;
    Data[1] = Id & 0xFF;
    Data[2] = 0x02;
    Data[3] = 0x00;
    Data[4] = 0b00011000;
    FingerWriteCmd(CMD_PACKAGE, CODE_AUTO_ENROLL, Data, 5);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            if (Buffer.Data[0] == 0x00 && Buffer.Data[1] == 0x00)
            {
                if (Buffer.Affirm == 0x00)
                    Debug("指令合法性检测成功,并进入第一次指纹录入\n");
                else
                    break;
            }
            else if (Buffer.Affirm == 0x00 && Buffer.Data[0] == 0x01)
                Debug("等待第[%d]次彩图成功\n", Buffer.Data[1]);
            else if (Buffer.Data[0] == 0x02)
            {
                if (Buffer.Affirm == 0x00)
                    Debug("等待第[%d]次生成特征成功\n", Buffer.Data[1]);
                else
                    break;
            }
            else if (Buffer.Affirm == 0x00 && Buffer.Data[0] == 0x03)
                Debug("第[%d]次手指离开\n", Buffer.Data[1]);
            else if (Buffer.Data[0] == 0x04 && Buffer.Data[1] == 0xF0)
            {
                if (Buffer.Affirm == 0x00)
                    Debug("合成模板成功\n");
                else
                {
                    Debug("合成模板失败\n");
                    break;
                }
            }
            else if (Buffer.Data[0] == 0x05 && Buffer.Data[1] == 0xF1)
            {
                if (Buffer.Affirm == 0x00)
                    Debug("没有相同指纹\n");
                else if (Buffer.Affirm == 0x27)
                {
                    Debug("有相同指纹\n");
                    break;
                }
            }
            else if (Buffer.Data[0] == 0x06 && Buffer.Data[1] == 0xF2)
            {
                if (Buffer.Affirm == 0x00)
                {
                    Debug("模板数据存储成功：%d\n", Id);
                    return true;
                }
                else
                {
                    Debug("模板数据存储失败\n");
                    break;
                }
            }
            else if (Buffer.Affirm == 0x26)
            {
                Debug("超时\n");
                break;
            }
            else if (Buffer.Affirm == 0x22)
            {
                Debug("指纹模板非空\n");
                break;
            }
            else
            {
                Debug("\n");
                break;
            }
            GetClockTimeMs(&Time);
        }
        /* 指纹模块超时时间近8秒，因此八秒内无返回则退出 */
        else if (DiffClockTimeMs(&Time) > (RETURN_TIMEOUT * 4))
        {
            Debug("Exit after 8 seconds\n");
            break;
        }
        usleep(1000);
    }
    Cancel();
    Debug("指纹:%d 模板录入失败\n", Id);
    return false;
}

static bool DeleteTemplate(uint16_t Page, uint16_t Num)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);

    uint8_t Data[4];
    Data[0] = (Page >> 8) & 0xFF;
    Data[1] = Page & 0xFF;
    Data[2] = (Num >> 8) & 0xFF;
    Data[3] = Num & 0xFF;
    FingerWriteCmd(CMD_PACKAGE, CODE_DELETE_CHAR, Data, 4);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("刪除指紋 %d %s\n", Num, Buffer.Affirm == 0x00 ? "成功" : "失败");
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("刪除指紋 %d 失败\n", Num);
    return false;
}

static bool EmptyTemplate(void)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_EMPTY, NULL, 0);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("清空指紋%s\n", Buffer.Affirm == 0x00 ? "成功" : "失败");
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("清空指紋失败\n");
    return false;
}

static bool ReadIndexTable(uint8_t Page, FingerDataAck *DataAck)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_READ_INDEX_TABLE, &Page, 1);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {

            Debug("索引列表读取%s\n", Buffer.Affirm == 0x00 ? "成功" : "失败");
            *DataAck = Buffer.Affirm == 0x00 ? Buffer : *DataAck;
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("索引列表读取失败\n");
    return false;
}

#if (FINGER_MANUFACTURER == BLACK_FIRE)
static bool LightSetting(LightCode Code, uint8_t Speed, LightColor Color, uint8_t Times)
#elif (FINGER_MANUFACTURER == ML_FPM093A)
static bool LightSetting(LightCode Code, uint8_t StartColor, LightColor EndColor, uint8_t Times)
#endif
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);

    uint8_t Data[4];
    Data[0] = Code;
#if (FINGER_MANUFACTURER == BLACK_FIRE)
    Data[1] = Speed;
    Data[2] = Color;
#elif (FINGER_MANUFACTURER == ML_FPM093A)
    Data[1] = StartColor;
    Data[2] = EndColor;
#endif
    Data[3] = Times;
    FingerWriteCmd(CMD_PACKAGE, CODE_LIGHT_SETTING, Data, (sizeof(Data) / sizeof(uint8_t)));
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("灯光设置%s          while\n", Buffer.Affirm == 0x00 ? "成功" : "失败");
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            // printf("time-in---%llu\n",DiffClockTimeMs(&Time));
            break;
        }
        // printf("time-out---%llu\n",DiffClockTimeMs(&Time));
        usleep(1000);
    }
    Debug("灯光设置失败\n");
    return false;
}

static bool ModuleSleep(void)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_SLEEP, NULL, 0);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("休眠指令%s\n", Buffer.Affirm == 0x00 ? "成功" : "失败");
            return (Buffer.Affirm == 0x00);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("休眠指令失败\n");
    return false;
}

#if (FINGER_MANUFACTURER == ML_FPM093A)
static bool DetectFingerPress(void)
{
    FingerDataAck Buffer;
    struct timespec Time;
    GetClockTimeMs(&Time);
    FingerWriteCmd(CMD_PACKAGE, CODE_GET_IMAGE, NULL, 0);
    while (1)
    {
        if (FingerReadData(&Buffer))
        {
            Debug("获取图像指令:%s\n", Buffer.Affirm == 0x00 ? "成功" : (Buffer.Affirm == 0x02 ? "传感器无手指" : "失败"));
            return (Buffer.Affirm == 0x02);
        }
        else if (DiffClockTimeMs(&Time) > RETURN_TIMEOUT)
        {
            break;
        }
        usleep(1000);
    }
    Debug("获取图像指令失败\n");
    return false;
}
#endif

/* ****************************************************************** 指纹事件定义接口 *********************************************************** */

static int ReadFingerModuleData(void)
{
    uint8_t Num = ValidTemplateNum();
    if (Num == 0 && GetFingerInfo()->TotalNum)
    {
        FingerInfoFormat();
        return 0;
    }
    else if (Num == 0xFF)
    {
        if ((Num = ValidTemplateNum()) == 0xFF)
        {
            return -1;
        }
    }

    Debug("Vaild Num:%d\n", Num);

    GetFingerInfo()->NextEnptyIndex = GetFingerInfo()->TotalNum = Num;

    if (GetFingerInfo()->TotalNum)
    {

        FingerDataAck Buffer;

        if (ReadIndexTable(0, &Buffer))
        {
            for (int Byte = 0, Num = 0; Byte < 32; Byte++)
            {
                for (int Bit = 0; Bit < 8; Bit++)
                {
                    uint8_t Index = Byte * 8 + Bit;

                    if (Index >= FINGER_NUM_MAX)
                    {
                        GetFingerInfo()->TotalNum = Num;
                        goto Exit;
                    }

                    if (Buffer.Data[Byte] & (1 << Bit))
                    {
                        Num++;

                        GetFingerInfo()->Finger[Index].Perm = GetFingerInfo()->Finger[Index].Perm ? GetFingerInfo()->Finger[Index].Perm : LOCK_TYPE;
                        FingerDataInit(Index);
                    }
                    else
                    {
                        GetFingerInfo()->Finger[Index].Perm = NONE_TYPE;
                        FingerDataReset(Index);

                        if (GetFingerInfo()->NextEnptyIndex == GetFingerInfo()->TotalNum)
                            GetFingerInfo()->NextEnptyIndex = Index;
                    }

                    if (Num == GetFingerInfo()->TotalNum || Num == FINGER_NUM_MAX)
                    {
                        goto Exit;
                    }
                }
            }
        }
    Exit:
        FingerInfoSave();
        return 0;
    }
    return 0;
}

static int FingerLightControl(int En, LightColor Color, uint8_t Times)
{
    Debug("Addr En:%p,Color:%p,Times:%p\n", &En, &Color, &Times);
    Debug("Vol En:%d,Color:%d,Times:%d\n", En, Color, Times);
    if (En)
    {
#if (FINGER_MANUFACTURER == BLACK_FIRE)
        LightSetting(FLASHING, 0x0A, Color, Times);
#elif (FINGER_MANUFACTURER == ML_FPM093A)
        LightSetting(FLASHING, Color, Color, Times);
#endif
    }
    else
    {

#if (FINGER_MANUFACTURER == BLACK_FIRE)
        // LightSetting(CLOSE_POWER_ON_LIGHT, 0, COLORFUL, 0);
        // LightSetting(SUCCEE_DEFAULT_SET, 0, COLORFUL, 0);
        // LightSetting(FAILED_DEFAULT_SET, 0, COLORFUL, 0);
        // LightSetting(BREATHING, 0, BLUE, 0);
#elif (FINGER_MANUFACTURER == ML_FPM093A)
        // printf("\nuserdate_cmd_state===================%d\n",UserConfigGet() -> cmd_status);
        if (UserConfigGet() -> cmd_status)
        {
            LightSetting(CLOSE_POWER_ON_LIGHT, 0, COLORFUL, 0); //hare set
            UserConfigGet() -> cmd_status = false;
            UserConfigSave();
        }
        // LightSetting(BREATHING, 0, BLUE, 0);
#endif
    }
    Debug("\n");
    return 0;
}

static int VerifyFingerPrintf(const FingerEvent *Event)
{
    uint16_t Num = 0xFFFF;
    if ((Num = AutoVerifyFinger(0xFFFF)) == 0xFFFF)
    {
        Debug("*************该指纹不存在**************\n");
        FingerLightControl(1, RED, 3);
        VoiceRingPlay(Bi4, VoiceDefVol);
    }
    else
    {
        Debug("*************该指纹已存在 指纹ID:%d**************\n", Num);
        FingerLightControl(1, GREEN, 1);
        // VoiceRingPlay(Bi2, VoiceDefVol);    hare
        if (FingerPermGet(Num) & LOCK_TYPE)
            Unlock(UserConfigGet()->UnlockTime, LOCK_TYPE);
        if (FingerPermGet(Num) & GATE_TYPE)
            Unlock(UserConfigGet()->UngateTime, GATE_TYPE);
    }
    return Num;
}

static int AddFingerPrintf(const FingerEvent *Event)
{
    RefreshTimer(30 * 1000, AddFingerTimer);
    if (AutoEnrool(GetFingerInfo()->NextEnptyIndex))
    {
        GetFingerInfo()->Finger[GetFingerInfo()->NextEnptyIndex].Perm = LOCK_TYPE;
        FingerLightControl(1, GREEN, 1);
        ReadFingerModuleData();
        VoiceRingPlay(Bi2, VoiceDefVol);
        NetManageShortPack(1, ManageAddFinger, 1, 0);
    }
    else
    {
        FingerLightControl(1, RED, 3);
        VoiceRingPlay(Bi4, VoiceDefVol);
        NetManageShortPack(1, ManageAddFinger, 9, 0);
    }
    return 0;
}

static int EraseFingerprintf(const FingerEvent *Event)
{
    if (Event->Arg1 == 200)
    {
        EmptyTemplate();
    }
    else
    {
        DeleteTemplate(Event->Arg1, Event->Arg2);
    }
    ReadFingerModuleData();
    return 1;
}

static int FingerDetectHandle(int Level)
{
    #define FirIntervalMs 800  //hare set 500 -> 800
    static struct timespec FirIntervalTime = {0};
    unsigned long long IntervalMs = DiffClockTimeMs(&FirIntervalTime);

    GetClockTimeMs(&FirIntervalTime);
    printf("=================================%llu\n",IntervalMs);
    if (IntervalMs > FirIntervalMs) /* 每次事件需间隔500ms */
    {
        static FingerEvent Event = {.Func = NULL, .EvRelease = ATOMIC_VAR_INIT(1)};
        if (atomic_load(&Event.EvRelease))
        {
            Debug("Trigger:%d,EvRelease:%d\n\n", Level, atomic_load(&Event.EvRelease));
            Event.EvStr = (char *)__func__;
            Event.Func = TimerEnablestatus(AddFingerTimer) ? AddFingerPrintf : VerifyFingerPrintf;
            FingerEventRegister(&Event);
            return 0;
        }
    }
    return 0;
}

int FingerDetectEpollEventInit(struct EpollEvent *Event)
{
#define FINGER_INT_GPIO 55//58 hare
    if (GpioOpen(FINGER_INT_GPIO, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioEdge(FINGER_INT_GPIO, RISING_EDGE);
    char Path[64] = {0};
    memset(Path, 0, sizeof(Path));
    sprintf(Path, "/sys/class/gpio/gpio%d/value", FINGER_INT_GPIO);
    Event->Fd = open(Path, O_RDONLY);
    Event->TriggerLevel = 1;
    Event->ChatterTimeMs = 50; 
    Event->EpollEventHandle = FingerDetectHandle;
    return 0;
}

#if (FINGER_MANUFACTURER == ML_FPM093A)
static int DetectFingerReleaseHandle(const FingerEvent *Event)
{
    if (DetectFingerPress())
    {
        ModuleSleep();
        return 0;
    }

    return 0;
}

static void GetImageHandle(void *u)
{
    static FingerEvent Event = {.Func = DetectFingerReleaseHandle, .EvRelease = ATOMIC_VAR_INIT(1)};
    if (atomic_load(&Event.EvRelease))
    {
        Event.EvStr = (char *)__func__;
        Event.Func = DetectFingerReleaseHandle;
        FingerEventRegister(&Event);
    }
}
#endif

static void *DrvFingerprintThread(void *arg)
{
    prctl(PR_SET_NAME, __FUNCTION__);

    ModuleSleep();
    FingerLightControl(0, 0, 0);
    ReadFingerModuleData();
    while (1)
    {
        FingerEvent *Event = NULL;
        if (CircularListRead(&FingerEventList, (void *)&Event) != -1)
        {
            if (atomic_load(&Event->EvRelease) == 0)
            {
                if (Event->Func)
                    Event->Func(Event);
                atomic_store(&Event->EvRelease, 1);
                Debug("%s EvRelease:%d\n", Event->EvStr, atomic_load(&Event->EvRelease));
            }
        }
    }
    return NULL;
}

/**
 * @description: 删除指纹事件注册
 * @param {uint8_t} Arg1    删除起始索引
 * @param {uint8_t} Arg2    删除个数
 * @return {*}
 */
void DelFingerEvnetRegister(uint8_t Arg1, uint8_t Arg2)
{
    static FingerEvent Event = {.EvRelease = ATOMIC_VAR_INIT(1)};
    if (atomic_load(&Event.EvRelease))
    {
        Event.EvStr = (char *)__func__;
        Event.Func = EraseFingerprintf, Event.Arg1 = Arg1, Event.Arg2 = Arg2;
        FingerEventRegister(&Event);
    }
}
/************************
 * FUNCTION: 电平检测指纹中断
 * AUTHOR: hare 
 ***********************/
static void *DrvFingerVanueThread(void *arg)
{
    #define FINGER_GPIO 55  // 指纹中断检测脚
    #define DEBOUNCE_MS 50  // 防抖动延迟
    GPIO_LEVEL prev_level = GPIO_LEVEL_LOW;  // 记录上一次电平状态
    GPIO_LEVEL curr_level = GPIO_LEVEL_UNKNOWN;
    bool is_triggered = false;  // 触发状态标记
    if (GpioOpen(FINGER_GPIO, GPIO_DIR_IN, true) == false)
    {
        return NULL;
    }
    // 初始读取电平状态
    GpioLevelGet(FINGER_GPIO, &prev_level);
    while(1)
    {
        // 读取当前电平
        GpioLevelGet(FINGER_GPIO, &curr_level);

        // 防抖动处理：检测到电平变化后延迟确认
        if (curr_level != prev_level)
        {
            usleep(DEBOUNCE_MS * 1000);  // 等待抖动消失
            GpioLevelGet(FINGER_GPIO, &curr_level);  // 再次确认电平
            if (curr_level != prev_level)
            {
                prev_level = curr_level;  // 更新状态
            }
        }

        // 检测逻辑：高电平且未触发过
        if (curr_level == GPIO_LEVEL_HIGH && !is_triggered)
        {
            // 触发指纹处理函数
            FingerDetectHandle(1);  // 传入高电平参数
            is_triggered = true;    // 标记为已触发
            printf("Finger detected, triggered once\n");
        }
        // 检测到低电平（手指离开） 重置触发状态
        else if (curr_level == GPIO_LEVEL_LOW && is_triggered)
        {
            is_triggered = false;   // 允许下次触发
            printf("Finger released, ready for next trigger\n");
        }

        // 降低CPU占用率
        usleep(10*1000);  // 10ms检测一次
    }
}
/**
 * @description: 指纹驱动初始化
 * @return {*}
 */
int FingerprintInit(void)
{
    UartFd = UartOpen("ttySAK0", 57600, 8, 1, 'n'); //ttySAK1   new:ttySAK0
    if (UartFd < 0)
    {
        DebugLog("open ttySAK0 faild \n");
        usleep(1000 * 1000);
        return false;
    }

    FingerInfoInit();

    pthread_t Thread;
    CreateCircularList(&FingerEventList);
    pthread_create(&Thread, NULL, DrvFingerprintThread, NULL);
    pthread_detach(Thread);

    pthread_t Thread_vanue_detect;
    pthread_create(&Thread_vanue_detect, NULL, DrvFingerVanueThread, NULL);
    pthread_detach(Thread_vanue_detect);
    return 0;
}