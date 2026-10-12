/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-24 15:33:36
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-01 14:50:38
 * @FilePath: /82225-EPC/src/NumericKeypad.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "GeneralInterface.h"
#include "NumericKeypad.h"
#include "SecurityMode.h"
#include "VoiceRingPlay.h"
#include "LightControl.h"
#include "GpioControl.h"
#include "UserConfig.h"
#include "UserCard.h"
#include "Unlock.h"
#include "Timer.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define Atoi(c) (c - 48)
#define DEFAULT_FACTORY_SET_FLAG  9

static ActionRoute RoutesMap[ActionTotal];

void KeypadLightDisable(void *us)
{
    if (TimerEnablestatus(KeypadTimer))
    {
        TimerDestroy(KeypadTimer);
    }
    // KeypadLightControl(0);  // hare
}

void KeypadLightEnable(void)
{
    extern int DarkModeStatus(void);
    int LightTime = DarkModeStatus() ? (UserConfigGet()->NumKeyLightTime ? UserConfigGet()->NumKeyLightTime : 86400) * 1000 : 10 * 1000;
    if (TimerEnablestatus(KeypadTimer))
    {
        RefreshTimer(LightTime, KeypadTimer);
    }
    else
    {
        SetTimer(LightTime, KeypadTimer, KeypadLightDisable, NULL);
    }
    // KeypadLightControl(1);   //hare
}
/*********************************************************************************************************************************/

static void PushRouteStack(KeyAction Actiom);
static void PopRouteStack(void);
static RouteStack ActionStack;

static void PrintfActionStackMap(void)
{
    PrintCentered("\033[33mActionStack Top\033[0m", 49);
    for(int i = ActionStack.CurrentRoute;i >= 0 ;i --)
    {
        PrintCentered(ActionStack.Routes[i]->ActionStr, 40);
    }
    PrintCentered("\033[33mActionStack Bottom\033[0m", 49);
}

static void ModifyCardCodeTimerHandle(void *us)
{
    struct timespec time;
    GetClockTimeMs(&time);
    printf("ModifyCodeCardTimer[Disable][%d]\n",__LINE__);
    printf("No operation for 30 seconds, exit management mode!!! [%ld]\n",time.tv_sec);
    PopRouteStack();
}

static void AdminOvertimeHandle(void *us)
{
    struct timespec time;
    GetClockTimeMs(&time);
    printf("No operation for 30 seconds, exit management mode!!! [%ld]\n",time.tv_sec);
    while (ActionStack.Routes[ActionStack.CurrentRoute] != &RoutesMap[KeyStandby])
    {
        PopRouteStack();
    }
    VoiceRingPlay(Bi3, VoiceDefVol);
}

void StandbyError(void)
{
    printf("\n");
    VoiceRingPlay(Bi4, VoiceDefVol);
    return;
}

void ErrorRetrunStandby(void)
{
    printf("%s\n", __func__);
    while (ActionStack.Routes[ActionStack.CurrentRoute] != &RoutesMap[KeyStandby])
    {
        PopRouteStack();
    }
    VoiceRingPlay(Bi4, VoiceDefVol);
    return;
}

void ErrorRetrunAdmin(void)
{
    printf("%s\n", __func__);
    while (ActionStack.Routes[ActionStack.CurrentRoute] != &RoutesMap[KeyAdmin])
    {
        PopRouteStack();
    }
    VoiceRingPlay(Bi4, VoiceDefVol);
    return;
}

int EnterStandby(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if(TimerEnablestatus(AdminOutTimer))
    {
        printf("[%s][%d] Destroy AdminOutTimer[Currnet Status %s] !!!!!\n",__func__,__LINE__,TimerEnablestatus(AdminOutTimer) ? "true":"false");
        TimerDestroy(AdminOutTimer);
    }
    return 1;
}

int CodeUnlock(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (TimerEnablestatus(MasterCardAddTimer) || TimerEnablestatus(MasterCardAddTimer))
    {
        return 0;
    }

    printf("[Curret %s Unlock] \n", UserConfigGet()->LockWay == CardAndCodeWay ? "CardAndCode" : UserConfigGet()->LockWay == CardOrCodeWay ? "CardOrCode"
                                                                                                                                           : "CardWay");
    int Permission = NONE_TYPE;if((KeyAttr->Cursor - 1) == strlen(UserConfigGet()->UnlockCode) || (KeyAttr->Cursor - 1) == strlen(UserConfigGet()->UngateCode))
    {
        if (memcmp(KeyAttr->Buff, UserConfigGet()->UnlockCode, KeyAttr->Cursor - 1) == 0)
        {
            Permission |= LOCK_TYPE;
        }
        if (memcmp(KeyAttr->Buff, UserConfigGet()->UngateCode, KeyAttr->Cursor - 1) == 0)
        {
            Permission |= GATE_TYPE;
        }

        if(Permission != NONE_TYPE && UserConfigGet()->PublicUnlockEn)
        {
            goto finish;
        }
        printf("Public Unlock Code:%s\n", UserConfigGet()->UnlockCode);
        printf("Public Ungate Code:%s\n", UserConfigGet()->UngateCode);
    }
    
    if (UserConfigGet()->LockWay == CardWay)
    {
        goto error;
    }
    else if (UserConfigGet()->LockWay == CardAndCodeWay)
    {
        if (!TimerEnablestatus(CodeCardUnlockTimer))
            goto error;

        if (memcmp(CARD_INITIAL_CODE, KeyAttr->Buff, KeyAttr->Cursor - 1) == 0)
            goto error;

        int *CardIndex = TimerGet(CodeCardUnlockTimer)->Data;
        Permission = CardCodeVerify(*CardIndex, KeyAttr->Buff, KeyAttr->Cursor - 1);

        /* 无论成功与否结束本次密码加卡开锁 */
        TimerDestroy(CodeCardUnlockTimer);
        goto finish;
    }
    else if((Permission = UserCodeVerify(KeyAttr->Buff)))
    {
        goto finish;
    }
    
    Permission = CardCodePermission(KeyAttr->Buff, KeyAttr->Cursor - 1);

finish:
    if (Permission)
    {
        SecurityErrorReset();
        VoiceRingPlay(Bi1, VoiceDefVol);

        if (Permission & LOCK_TYPE)
            Unlock(UserConfigGet()->UnlockTime, LOCK_TYPE);
        if (Permission & GATE_TYPE)
            Unlock(UserConfigGet()->UngateTime, GATE_TYPE);
        return 1;
    }

error:
    SecurityErrorUpdate();
    printf("[%s][%d]\n",__func__,__LINE__);
    return 0;
}

int MasterCardAddDel(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    static int CardIndex;
    if (KeyAttr->Cursor != 4)
        return 0;
    // printf("[%s][%d]\n",__func__,__LINE__);

    if ((CardIndex = Atoi(KeyAttr->Buff[0]) * 100 + Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2])) >= DECK_SIZE_MAX)
        return 0;

    int IndexValid=DeckIndexPerm(CardIndex);
    // printf("[%s][%d]IndexValid:%d,CardIndex:%d\n",__func__,__LINE__,IndexValid,CardIndex);
    if(!IndexValid && TimerEnablestatus(MasterCardDelTimer))
    {
        // printf("[%s][%d]%d\n",__func__,__LINE__,IndexValid);
        return 0;
    }

    if (TimerEnablestatus(MasterCardAddTimer) && TimerGet(MasterCardAddTimer)->Data == NULL)
    {
        VoiceRingPlay(Bi2, VoiceDefVol);
        TimerGet(MasterCardAddTimer)->Data = &CardIndex;
        return 1;
    }
    else if(TimerEnablestatus(MasterCardDelTimer) && TimerGet(MasterCardDelTimer)->Data)
    {
        LockType Type = *(LockType *)(TimerGet(MasterCardDelTimer)->Data);
        UserCardPermDel(CardIndex, Type);
        VoiceRingPlay(Bi2, VoiceDefVol);
        printf("User Card %d Del Succeed!!!\n", CardIndex);
        return 1;
    }
    printf("[%s][%d]\n",__func__,__LINE__);
    return 0;
}

int ExitStandbyReady(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (TimerEnablestatus(MasterCardAddTimer) || TimerEnablestatus(MasterCardAddTimer))
    {
        return 0;
    }

    if (TimerEnablestatus(ModifyCodeCardTimer))
    {
        printf("ModifyCodeCardTimer[Disable][%d]\n",__LINE__);
        TimerDestroy(ModifyCodeCardTimer);
    }
    return 1;
}

int StandbyReadyMode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (!TimerEnablestatus(ModifyCodeCardTimer))
    {
        printf("ModifyCodeCardTimer[Enable]\n");
        SetTimer(30 * 1000, ModifyCodeCardTimer, ModifyCardCodeTimerHandle, NULL);
    }
    PushRouteStack(KeyStandbyReady);
    return 1;
}

int ReturnRoute(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    VoiceRingPlay(Bi2, VoiceDefVol);
    PopRouteStack();
    return 1;
}

int ReturnStandby(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    while (ActionStack.CurrentRoute != KeyStandby)
    {
        PopRouteStack();
    }
    VoiceRingPlay(Bi2, VoiceDefVol);
    return 1;
}

int EnterModifyCardCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    // printf("[%s] Cursor:%d,TimerEm:%d\n", __func__, KeyAttr->Cursor, TimerEnablestatus(ModifyCodeCardTimer));
    if (KeyAttr->Cursor - 1 != sizeof(UserCardGet()->Deck[0].Code))
        goto error;

    if (!TimerEnablestatus(ModifyCodeCardTimer))
        goto error;

    int *CardIndex = TimerGet(ModifyCodeCardTimer)->Data;
    if (CardIndex)
    {
        if (CardCodeVerify(*CardIndex, KeyAttr->Buff, KeyAttr->Cursor - 1))
        {
            if (TimerEnablestatus(ModifyCodeCardTimer))
            {
                printf("ModifyCodeCardTimer[Disable][%d]\n",__LINE__);
                TimerDestroy(ModifyCodeCardTimer);
            }
            PushRouteStack(KeyNewCardCode);
            VoiceRingPlay(Bi2, VoiceDefVol);
            return 1;
        }
    }
error:
    TimerDestroy(ModifyCodeCardTimer);
    return 0;
}

int EnterAffirmCardCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (KeyAttr->Cursor - 1 != sizeof(UserCardGet()->Deck[0].Code) && CurrRoute->RouteData)
    {
        return 0;
    }

    static char Buf[32] = {0};
    memset(Buf, 0, sizeof(Buf));
    memcpy(Buf, KeyAttr->Buff, KeyAttr->Cursor - 1);
    CurrRoute->RouteData = Buf;
    PushRouteStack(KeyAffiCardCode);
    VoiceRingPlay(Bi1, VoiceDefVol);
    return 1;
}

int EnterCardCodeVerify(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (KeyAttr->Cursor - 1 != sizeof(UserCardGet()->Deck[0].Code) && CurrRoute->RouteData)
    {
        goto error;
    }
    int *CardIndex = TimerGet(ModifyCodeCardTimer)->Data;

    if (memcmp(KeyAttr->Buff, CurrRoute->RouteData, KeyAttr->Cursor - 1) == 0)
    {
        printf("[Verify Pass!!! Modify Card[%d] Code Succeed]!!\n", *CardIndex);
        memcpy(UserCardGet()->Deck[*CardIndex].Code, KeyAttr->Buff, KeyAttr->Cursor - 1);
        UserCardSave();
        PopRouteStack();
        PopRouteStack();
        PopRouteStack();
        VoiceRingPlay(Bi2, VoiceDefVol);
        return 1;
    }
error:
    PopRouteStack();
    PopRouteStack();
    PopRouteStack();
    return 0;
}

int NewCardCodeVerify(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    return 1;
}

int EnterAdminMode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (KeyAttr->Cursor - 1 != strlen(UserConfigGet()->AdminCode))
    {
        printf("[%s]Not enough password bits!!\n", __func__);
        return 0;
    }

    if (memcmp(KeyAttr->Buff, UserConfigGet()->AdminCode, strlen(UserConfigGet()->AdminCode)) == 0)
    {
        if (TimerEnablestatus(ModifyCodeCardTimer))
        {
            printf("ModifyCodeCardTimer[Disable][%d]\n",__LINE__);
            TimerDestroy(ModifyCodeCardTimer);
        }
        PushRouteStack(KeyAdmin);
        printf("[%s][%d] Open AdminOutTimer !!!!!\n",__func__,__LINE__);
        SetTimer(30 * 1000, AdminOutTimer, AdminOvertimeHandle, NULL);
        VoiceRingPlay(Bi2, VoiceDefVol);
        return 1;
    }
    return 0;
}

int ExitAdminMode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if(TimerEnablestatus(AdminOutTimer))
    {
        printf("[%s][%d] Destroy AdminOutTimer[Currnet Status %s] !!!!!\n",__func__,__LINE__,TimerEnablestatus(AdminOutTimer) ? "true":"false");
        TimerDestroy(AdminOutTimer);
    }
    return 1;
}

int RestoreFactory(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    UserConfigReset();
    RefreshTimer(30 * 1000, AdminOutTimer);
    VoiceRingPlay(Bi1, VoiceDefVol);
    return 1;
}

int UnlockVoiceSwitch(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    if (Atoi(KeyAttr->Buff[2]) != 1 && Atoi(KeyAttr->Buff[2]) != 0)
        return 0;

    UserConfigGet()->UnlockVoiceEn = Atoi(KeyAttr->Buff[2]);
    UserCardSave();
    VoiceRingPlay(Bi2, VoiceDefVol);
    return 1;
}

int EnterNewAdminCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    PushRouteStack(KeyNewAdminCode);
    RefreshTimer(30 * 1000, AdminOutTimer);
    VoiceRingPlay(Bi2, VoiceDefVol);
    return 1;
}

int EnterAffirmAdminCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    static char Buff[ADMIN_CODE_LEN] = {0};
    if (KeyAttr->Cursor - 1 != strlen(UserConfigGet()->AdminCode))
    {
        printf("[%s]Not enough password bits!!\n", __func__);
        return 0;
    }
    memset(Buff, 0, sizeof(Buff));
    memcpy(Buff, KeyAttr->Buff, KeyAttr->Cursor - 1);
    CurrRoute->RouteData = Buff;
    PushRouteStack(KeyAffiAdminCode);
    VoiceRingPlay(Bi1, VoiceDefVol);
    return 1;
}

int EnterAdminCodeVerify(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (!CurrRoute->RouteData || KeyAttr->Cursor - 1 != ADMIN_CODE_LEN)
        return 0;

    if (memcmp(KeyAttr->Buff, CurrRoute->RouteData, KeyAttr->Cursor - 1) == 0)
    {
        printf("[Verify Pass!!! Modify Admin Code Succeed]!!\n");
        memcpy(UserConfigGet()->AdminCode, KeyAttr->Buff, KeyAttr->Cursor - 1);
        UserConfigSave();
        PopRouteStack();
        PopRouteStack();
        VoiceRingPlay(Bi2, VoiceDefVol);
        return 1;
    }
    printf("[AdminCodeVerify Fail]!!\n");
    return 0;
}

int EnterNewLockCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    static bool DefaultUnlockSetFlag;
    DefaultUnlockSetFlag = false;

    /* 
    *修改默认出厂Gate开锁密码 022 + DEFAULT_FACTORY_SET_FLAG
    *修改用户Gate开锁密码 022
    */
    if(KeyAttr->Cursor - 1 == 4 && Atoi(KeyAttr->Buff[3]) == DEFAULT_FACTORY_SET_FLAG)
    {
        DefaultUnlockSetFlag = true;
    }
    else if(KeyAttr->Cursor - 1 != 3)
    {
        return 0;
    }
    printf("[%s][%d]%d,%s,%d\n",__func__,__LINE__,KeyAttr->Cursor - 1,KeyAttr->Buff,DefaultUnlockSetFlag);
    RefreshTimer(30 * 1000, AdminOutTimer);
    PushRouteStack(KeyNewLockCode);
    VoiceRingPlay(Bi2, VoiceDefVol);

    RoutesMap[KeyNewLockCode].RouteData = &DefaultUnlockSetFlag;
    return 1;
}

int EnterAffirmLockCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    static char Buff[UNLOCK_CODE_MAX_LEN + 1] = {0};
    if ((KeyAttr->Cursor - 1 > UNLOCK_CODE_MAX_LEN) || (KeyAttr->Cursor - 1 < UNLOCK_CODE_MIN_LEN))
    {
        printf("[%s]The number of password bits is not in the valid range!!\n", __func__);
        return 0;
    }
    memset(Buff, 0, sizeof(Buff));
    memcpy(Buff, KeyAttr->Buff, KeyAttr->Cursor -1);
    PushRouteStack(KeyAffiLockCode);
    RoutesMap[KeyAffiLockCode].RouteData = Buff;
    VoiceRingPlay(Bi1, VoiceDefVol);
    return 1;
}

int EnterLockCodeVerify(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (!CurrRoute->RouteData || KeyAttr->Cursor - 1 != strlen((char *)CurrRoute->RouteData) || KeyAttr->Cursor - 1 > sizeof(UserConfigGet()->UnlockCode))
    {
        printf("[%s]The password length is different between the two times!!? [%p][%d][%d]\n", __func__,CurrRoute->RouteData,strlen((char *)CurrRoute->RouteData),KeyAttr->Cursor - 1);
        return 0;
    }
    // printf("[%s][%d][%s][%s]\n",__func__,__LINE__,KeyAttr->Buff,CurrRoute->RouteData);
    if (memcmp(KeyAttr->Buff, CurrRoute->RouteData, KeyAttr->Cursor -1) == 0)
    {
        bool DefaultUnlockSetFlag = RoutesMap[KeyNewLockCode].RouteData ? (*(bool *)RoutesMap[KeyNewLockCode].RouteData) : false;
        if(DefaultUnlockSetFlag)
        {
            memset(UserDefaultConfigGet()->UnlockCode,0,sizeof(UserDefaultConfigGet()->UnlockCode));
            memcpy(UserDefaultConfigGet()->UnlockCode, CurrRoute->RouteData, strlen((char *)CurrRoute->RouteData));
            UserDefaultConfigSave();
            printf("[Verify Pass!!! Modify Factory Default Lock Code Succeed]!!\n");
        }

        printf("[Verify Pass!!! Modify Lock Code Succeed]!!\n");
        memset(UserConfigGet()->UnlockCode,0,sizeof(UserConfigGet()->UnlockCode));
        memcpy(UserConfigGet()->UnlockCode, CurrRoute->RouteData, strlen((char *)CurrRoute->RouteData));

        UserConfigSave();
        PopRouteStack();
        PopRouteStack();
        VoiceRingPlay(Bi2, VoiceDefVol);
        return 1;
    }
    printf("[LockCodeVerify Fail]!!\n");
    return 0;
}

int EnterNewGateCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    static bool DefaultUnlockSetFlag;
    DefaultUnlockSetFlag = false;
    /* 
    *修改默认出厂Lock开锁密码 011 + DEFAULT_FACTORY_SET_FLAG
    *修改用户Lock开锁密码 011
    */
    if(KeyAttr->Cursor - 1 == 4 && Atoi(KeyAttr->Buff[3]) == DEFAULT_FACTORY_SET_FLAG)
    {
        DefaultUnlockSetFlag = true;
    }
    else if(KeyAttr->Cursor - 1 != 3)
    {
        return 0;
    }
    RefreshTimer(30 * 1000, AdminOutTimer);
    PushRouteStack(KeyNewGateCode);
    VoiceRingPlay(Bi2, VoiceDefVol);

    RoutesMap[KeyNewGateCode].RouteData = &DefaultUnlockSetFlag;
    return 1;
}

int EnterAffirmGateCode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    static char Buff[UNLOCK_CODE_MAX_LEN + 1] = {0};
    if ((KeyAttr->Cursor - 1 > UNLOCK_CODE_MAX_LEN) || (KeyAttr->Cursor - 1 < UNLOCK_CODE_MIN_LEN))
    {
        printf("[%s]The number of password bits is not in the valid range!!\n", __func__);
        return 0;
    }
    memset(Buff, 0, sizeof(Buff));
    memcpy(Buff, KeyAttr->Buff, KeyAttr->Cursor -1);
    PushRouteStack(KeyAffiGateCode);
    RoutesMap[KeyAffiGateCode].RouteData = Buff;
    VoiceRingPlay(Bi1, VoiceDefVol);
    return 1;
}

int EnterGateCodeVerify(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (!CurrRoute->RouteData || KeyAttr->Cursor - 1 != strlen((char *)CurrRoute->RouteData) || KeyAttr->Cursor - 1 > sizeof(UserConfigGet()->UngateCode))
    {
        printf("[%s]The password length is different between the two times!!? [%p]\n", __func__,CurrRoute->RouteData);
        return 0;
    }

    if (memcmp(KeyAttr->Buff, CurrRoute->RouteData, KeyAttr->Cursor -1) == 0)
    {
        bool DefaultUnlockSetFlag = RoutesMap[KeyNewGateCode].RouteData ? (*(bool *)RoutesMap[KeyNewGateCode].RouteData) : false;
        if(DefaultUnlockSetFlag)
        {
            memset(UserDefaultConfigGet()->UngateCode,0,sizeof(UserDefaultConfigGet()->UngateCode));
            memcpy(UserDefaultConfigGet()->UngateCode, CurrRoute->RouteData, strlen((char *)CurrRoute->RouteData));
            UserDefaultConfigSave();
            printf("[Verify Pass!!! Modify Factory Default Gate Code Succeed]!!\n");
        }

        printf("[Verify Pass!!! Modify Gate Code Succeed]!!\n");
        memset(UserConfigGet()->UngateCode,0,sizeof(UserConfigGet()->UngateCode));
        memcpy(UserConfigGet()->UngateCode, CurrRoute->RouteData, strlen((char *)CurrRoute->RouteData));
        
        UserConfigSave();
        PopRouteStack();
        PopRouteStack();
        VoiceRingPlay(Bi2, VoiceDefVol);
        return 1;
    }
    printf("[GateCodeVerify Fail]!!\n");
    return 0;
}

int SetUnlockTime(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    bool DefaultUnlockTimeFlag = false;
    /* 
    *修改默认出厂Lock开锁时间 2 + DEFAULT_FACTORY_SET_FLAG + ***(Time)
    *修改用户Lock开锁时间 2 + ***(Time)
    */
   DefaultUnlockTimeFlag = false;
    if(KeyAttr->Cursor - 1 == 4 && Atoi(KeyAttr->Buff[1]) == DEFAULT_FACTORY_SET_FLAG)
    {
        DefaultUnlockTimeFlag = true;
    }
    else if(KeyAttr->Cursor - 1 != 3)
    {
        return 0;
    }

    RefreshTimer(30 * 1000, AdminOutTimer);
    UserConfigGet()->UnlockTime = Atoi(KeyAttr->Buff[1 + DefaultUnlockTimeFlag]) * 10 + Atoi(KeyAttr->Buff[2 + DefaultUnlockTimeFlag]);
    UserConfigSave();
    if(DefaultUnlockTimeFlag)
    {
        UserDefaultConfigGet()->UnlockTime = UserConfigGet()->UnlockTime;
        UserDefaultConfigSave();
    }
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify %s Unlock Time :%dS\n", DefaultUnlockTimeFlag ? "Default Factory" : "", UserConfigGet()->UnlockTime);
    return 1;
}

int SetUngateTime(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    bool DefaultUnlockTimeFlag = false;
    /* 
    *修改默认出厂Gate开锁时间 4 + DEFAULT_FACTORY_SET_FLAG + ***(Time)
    *修改用户Gate开锁时间 4 + ***(Time)
    */
   DefaultUnlockTimeFlag = false;
    if(KeyAttr->Cursor - 1 == 4 && Atoi(KeyAttr->Buff[1]) == DEFAULT_FACTORY_SET_FLAG)
    {
        DefaultUnlockTimeFlag = true;
    }
    else if(KeyAttr->Cursor - 1 != 3)
    {
        return 0;
    }

    RefreshTimer(30 * 1000, AdminOutTimer);
    UserConfigGet()->UngateTime = Atoi(KeyAttr->Buff[1 + DefaultUnlockTimeFlag]) * 10 + Atoi(KeyAttr->Buff[2 + DefaultUnlockTimeFlag]);
    UserConfigSave();
    if(DefaultUnlockTimeFlag)
    {
        UserDefaultConfigGet()->UngateTime = UserConfigGet()->UngateTime;
        UserDefaultConfigSave();
    }
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify %s UnGate Time :%dS\n", DefaultUnlockTimeFlag ? "Default Factory" : "", UserConfigGet()->UngateTime);
    return 1;
}

int SetBacklightTime(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 4)
        return 0;

    UserConfigGet()->NumKeyLightTime = Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2]);
    UserConfigSave();
    KeypadLightEnable();
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify Backlight Time :%dS\n", UserConfigGet()->NumKeyLightTime);
    return 1;
}

int SetLockWay(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 4)
        return 0;

    int LockWay = Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2]);
    if (LockWay != CardWay && LockWay != CardOrCodeWay && LockWay != CardAndCodeWay)
    {
        return 0;
    }

    UserConfigGet()->LockWay = LockWay;
    UserConfigSave();
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify Lock Way  [%s]\n", LockWay == CardWay ? "CardWay" : LockWay == CardOrCodeWay ? "CardOrCodeWay"
                                                                                                : "CardAndCodeWay");
    return 1;
}

int SetSafeMode(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 4)
        return 0;

    int SafeMode = Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2]);
    if (SafeMode != CloseSafe && SafeMode != LockMode && SafeMode != AlarmMode)
    {
        return 0;
    }
    UserConfigGet()->SafeMode = SafeMode;
    UserConfigSave();
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify Safe Mode [%s]\n", UserConfigGet()->SafeMode == CloseSafe ? "CloseSafe" : UserConfigGet()->SafeMode == LockMode ? "LockMode"
                                                                                                                                   : "AlarmMode");
    return 1;
}

int SetPublicUnlockEn(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 4)
        return 0;

    int PublicUnlockEn = Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2]);
    if (PublicUnlockEn != 1 && PublicUnlockEn != 0)
    {
        return 0;
    }
    UserConfigGet()->PublicUnlockEn = PublicUnlockEn;
    UserConfigSave();
    VoiceRingPlay(Bi2, VoiceDefVol);
    printf("Modify Safe Mode [%s]\n", UserConfigGet()->PublicUnlockEn ? "Enable" : "Disable");
    return 1;
}

void CloseAddMasterCardMode(void *us)
{
    printf("[Exit Add Master User Card Mode]!!!!\n\n");
    while (ActionStack.CurrentRoute != KeyStandby)
    {
        PopRouteStack();
    }
}

int AddLockMasterCard(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 2)
        return 0;

    SetTimer(30 * 1000, AddMasterCardTimer, CloseAddMasterCardMode, &UserCardGet()->MasterCard[LockMasterCard].Card[AddMasterCard]);
    PushRouteStack(KeyAddLockMasterCard);
    VoiceRingPlay(Bi2, VoiceDefVol);
    return 1;
}

int ExitAddLockMasterCard(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    TimerDestroy(AddCardTimer);
    return 1;
}

int AddGateMasterCard(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    RefreshTimer(30 * 1000, AdminOutTimer);
    if (KeyAttr->Cursor != 2)
        return 0;
        
    SetTimer(30 * 1000, AddMasterCardTimer, CloseAddMasterCardMode, &UserCardGet()->MasterCard[GateMasterCard].Card[AddMasterCard]);
    PushRouteStack(KeyAddGateMasterCard);
    VoiceRingPlay(Bi2, VoiceDefVol);

    return 1;
}

int ExitAddGateMasterCard(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    TimerDestroy(DelCardTimer);
    return 1;
}

int SetLanguage(Keyboard *KeyAttr, struct ActionRoute *CurrRoute)
{
    int ret = 0;
    if (KeyAttr->Cursor == 5)
    {
        /* 修改默认出厂语言指令 9 + DEFAULT_FACTORY_SET_FLAG + language */
        if(Atoi(KeyAttr->Buff[1]) == DEFAULT_FACTORY_SET_FLAG)
        {
            int Language = Atoi(KeyAttr->Buff[2]) * 10 + Atoi(KeyAttr->Buff[3]);
            if (Language < LanguageTotal)
            {
                UserDefaultConfigGet()->Language = UserConfigGet()->Language = Language;
                UserConfigSave();
                UserDefaultConfigSave();
                VoiceRingPlay(Bi2, VoiceDefVol);
                printf("Modify Default Language [%d]\n", Language);
                ret = 1;
            goto set_language_exit;
            }
        }
    }
    if (KeyAttr->Cursor == 4)
    {

    /* 修改语言指令 9 + language */
        int Language = Atoi(KeyAttr->Buff[1]) * 10 + Atoi(KeyAttr->Buff[2]);
        if (Language < LanguageTotal)
        {
            UserConfigGet()->Language = Language;
            UserConfigSave();
            VoiceRingPlay(Bi2, VoiceDefVol);
            printf("Modify Language [%d]\n", UserConfigGet()->Language);
            ret = 1;
            goto set_language_exit;
        }
    }

set_language_exit:
    RefreshTimer(30 * 1000, AdminOutTimer);
    return ret;
}

static ActionRoute RoutesMap[ActionTotal] = {
    [KeyStandby] = {"[Standby Mode]", NULL, EnterStandby, StandbyError, NULL, {KeyStandbyReady, KeyCodeUnlock,KeyMasterAddDelCard}, 3},
    [KeyCodeUnlock] = {"[Code Unlock]", NULL, CodeUnlock, StandbyError, NULL, {0}, 0},
    [KeyMasterAddDelCard] = {"[Master Card Add Or Del]", NULL, MasterCardAddDel, StandbyError, NULL, {0}, 0},
    [KeyStandbyReady] = {"[KeyStandbyReady Mode]", "*", StandbyReadyMode, ErrorRetrunStandby, ExitStandbyReady, {KeyAdmin, KeyNewCardCode}, 2},
    [KeyStandbyReturn] = {"[Return Standby]", "*", ReturnStandby, ErrorRetrunStandby, NULL, {0}, 0},
    [KeyReturn] = {"[Return]", "*", ReturnRoute, NULL, NULL, {0}, 0},

    [KeyNewCardCode] = {"[New Card Code]", NULL, EnterModifyCardCode, ErrorRetrunStandby, NULL, {KeyStandbyReturn, KeyAffiCardCode}, 2},
    [KeyAffiCardCode] = {"[Affirm Card Code]", NULL, EnterAffirmCardCode, ErrorRetrunStandby, NULL, {KeyReturn, KeyCardCodeVerify}, 2},
    [KeyCardCodeVerify] = {"[Card Code Verify]", NULL, EnterCardCodeVerify, ErrorRetrunStandby, NULL, {0}, 0},

    [KeyAdmin] = {
        "[Admin Mode]",
        NULL,
        EnterAdminMode,
        ErrorRetrunStandby,
        ExitAdminMode,
        {KeyStandbyReturn,
         KeyReset,
         KeyUnlockVoiceSwitch,
         KeyNewAdminCode,
         KeyNewLockCode,
         KeyNewGateCode,
         KeyUnlockTime,
         KeyUngateTime,
         KeyBacklightTime,
         KeyLockWay,
         KeySafeMode,
         KeyPublicUnlockEn,
         KeyAddLockMasterCard,
         KeyAddGateMasterCard,
         KeyLanguage},
        15},

    [KeyReset] = {"[restore factory setting]", "099", RestoreFactory, NULL, NULL, {0}, 0},

    [KeyUnlockVoiceSwitch] = {"[Unlock Voice Switch]", "08", UnlockVoiceSwitch, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyNewAdminCode] = {"[New Admin Code]", "077#", EnterNewAdminCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyAffiAdminCode}, 2},
    [KeyAffiAdminCode] = {"[Affirm New Admin Code]", NULL, EnterAffirmAdminCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyAdminCodeVerify}, 2},
    [KeyAdminCodeVerify] = {"[New Admin Code Verify]\n", NULL, EnterAdminCodeVerify, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyNewLockCode] = {"[New Lock Code]", "011", EnterNewLockCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyAffiLockCode}, 2},
    [KeyAffiLockCode] = {"[Affirm New Lock Code]", NULL, EnterAffirmLockCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyLockCodeVerify}, 2},
    [KeyLockCodeVerify] = {"[New Lock Code Verify]", NULL, EnterLockCodeVerify, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyNewGateCode] = {"[New Gate Code]", "022", EnterNewGateCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyAffiGateCode}, 2},
    [KeyAffiGateCode] = {"[Affirm New Gate Code]", NULL, EnterAffirmGateCode, ErrorRetrunAdmin, NULL, {KeyReturn, KeyGateCodeVerify}, 2},
    [KeyGateCodeVerify] = {"[New Gate Code Verify]", NULL, EnterGateCodeVerify, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyBacklightTime] = {"[Setting Backlight Time]", "1", SetBacklightTime, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyUnlockTime] = {"[Setting UnLock Time]", "2", SetUnlockTime, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyLockWay] = {"[Setting Lock Way]", "3", SetLockWay, ErrorRetrunAdmin, NULL, {0}, 0},
    [KeyUngateTime] = {"[Setting UnGate Time]", "4", SetUngateTime, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeySafeMode] = {"[Setting Safe Mode]", "5", SetSafeMode, ErrorRetrunAdmin, NULL, {0}, 0},
    [KeyPublicUnlockEn] = {"[Setting Public Unlock En]\n", "6", SetPublicUnlockEn, ErrorRetrunAdmin, NULL, {0}, 0},

    [KeyAddLockMasterCard] = {"[Add Lock Master User Card]", "7", AddLockMasterCard, ErrorRetrunAdmin, ExitAddLockMasterCard, {KeyReturn}, 1},
    [KeyAddGateMasterCard] = {"[Add Gate Master User Card]", "8", AddGateMasterCard, ErrorRetrunAdmin, ExitAddGateMasterCard, {KeyReturn}, 1},

    [KeyLanguage] = {"[Modify Language]", "9", SetLanguage, ErrorRetrunAdmin, NULL, {0}, 0},
};
/*********************************************************************************************************************************/

static void PushRouteStack(KeyAction Actiom)
{
    assert(Actiom >= KeyStandby && Actiom < ActionTotal);
    ActionRoute **CurrRoute = &(ActionStack.Routes[ActionStack.CurrentRoute]);

    if ((*CurrRoute) == NULL)
    {
        (*CurrRoute) = &RoutesMap[Actiom];
        printf("Enter %s\n", (*CurrRoute)->ActionStr);
    }
    else
    {
        for (int i = 0; i < (*CurrRoute)->NextRouteCount; i++)
        {
            if ((*CurrRoute)->NextRoute[i] == Actiom)
            {
                RoutesMap[Actiom].RouteData = (*CurrRoute)->RouteData;
                ActionStack.CurrentRoute++;
                printf("%s", (*CurrRoute)->ActionStr);
                ActionStack.Routes[ActionStack.CurrentRoute] = &RoutesMap[Actiom];

                struct timespec time;
                GetClockTimeMs(&time);
                printf(" => %s [%ld]\n", ActionStack.Routes[ActionStack.CurrentRoute]->ActionStr,time.tv_sec);
                break;
            }
        }
    }
}


static void PopRouteStack(void)
{
    if (ActionStack.CurrentRoute)
    {
        if (ActionStack.Routes[ActionStack.CurrentRoute]->ExitHandle)
            ActionStack.Routes[ActionStack.CurrentRoute]->ExitHandle(NULL, NULL);

        ActionStack.CurrentRoute--;
        
        struct timespec time;
        GetClockTimeMs(&time);
        printf("[%d]%s [%ld]\n", ActionStack.CurrentRoute,ActionStack.Routes[ActionStack.CurrentRoute]->ActionStr,time.tv_sec);

    }
}

int KeypadProess(Keyboard *KeyAttr)
{
    assert(KeyAttr->Cursor > 0);
    ActionRoute **CurrRoute = &(ActionStack.Routes[ActionStack.CurrentRoute]);
    for (int i = 0; i < (*CurrRoute)->NextRouteCount; i++)
    {
        ActionRoute Route = RoutesMap[(*CurrRoute)->NextRoute[i]];
        // printf("KeypadProess:%s,NextRouteCount:%d,Route.Command:%p\n", Route.ActionStr, Route.NextRouteCount, Route.Command);
        // for (int i = 0; i < Route.NextRouteCount; i++)
        // {
        // printf("NextRoute[%d]:%d\n", i, Route.NextRoute[i]);
        // }
        if (Route.Process)
        {
            if (Route.Command == NULL || memcmp(Route.Command, KeyAttr->Buff, strlen(Route.Command)) == 0)
            {
                if (Route.Process(KeyAttr, (*CurrRoute)))
                {
                    PrintfActionStackMap();
                    return 1;
                }
            }
        }
    }
    
    if ((*CurrRoute)->ErrorHandle)
        (*CurrRoute)->ErrorHandle();

    return 0;
}

int NumericKeypadInit(void)
{
    memset(&ActionStack, 0, sizeof(ActionStack));
    PushRouteStack(KeyStandby);
    return 1;
}