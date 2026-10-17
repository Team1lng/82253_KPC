/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-26 09:41:09
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-08-07 08:44:43
 * @FilePath: /project_3/src/UartIcCard.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "UserNetManage.h"
#include "VoiceRingPlay.h"
#include "SecurityMode.h"
#include "LightControl.h"
#include "UartControl.h"
#include "UartIcCard.h"
#include "UserConfig.h"
#include "UserCard.h"
#include "Unlock.h"
#include "Timer.h"
#include <string.h>

#define TTYSAK_DEVICE   "ttySAK2"   //刷卡模块接在GPIO55=UART2_RXD(原理图连接器30脚)，故用ttySAK2
int UartIcCardCheck(const char *Data)
{
    if ((Data[0] ^ Data[1] ^ Data[2] ^ Data[3]) == Data[4])
    {
        return 0;
    }
    return -1;
}

static void MasterCardAddDelExit(void *u)
{
    printf("Exit MasterCardAddDel Timer!!!\n");
}

/**********************************************************弱函数重定义************************************************************8*/
int UartIcCardModuleInit(int *DrvFd, int *DrvLen)
{
    *DrvFd = UartOpen(TTYSAK_DEVICE, 115200, 8, 1, 'n');
    if (*DrvFd < 0)
    {
        printf("open %s faild \n",TTYSAK_DEVICE);
        return -1;
    }
    *DrvLen = UART_IC_CARD_LEN;
    printf("%s  Succeed!!!\n", __func__);
    return 0;
}

int MasterCardSwipeDelect(char *Data)
{
    if(TimerEnablestatus(AdminOutTimer) || TimerEnablestatus(ModifyCodeCardTimer))
    {
        return 0;
    }
    MasterCardType TmpTypeInfo = {NONE_TYPE,AddMasterCard};
    static MasterCardType TypeInfo = {NONE_TYPE,AddMasterCard};
    static int MasterCardSwipeCount = 0;
    
    if(MasterCardTypeVerify(Data,&TmpTypeInfo))
    {
        if(!TimerEnablestatus(MasterCardSwipeTimer))
        {
                MasterCardSwipeCount = 1;
                TypeInfo = TmpTypeInfo;
                SetTimer(30*1000,MasterCardSwipeTimer,NULL,NULL);
                VoiceRingPlay(Bi1, VoiceDefVol);
                printf("Start MasterCardSwipeTimer Detect!!!\n");

                if(TypeInfo.LockAttr & LOCK_TYPE )
                {
                    Unlock(UserConfigGet()->UnlockTime, LOCK_TYPE);
                }
                if(TypeInfo.LockAttr & GATE_TYPE )
                {
                    Unlock(UserConfigGet()->UngateTime, GATE_TYPE);
                }
                return 1;
        }
        else
        {          
            MasterCardSwipeCount ++;
            /* 当母卡刷新累计三次以上，关闭增删卡操作模式 */
            if(MasterCardSwipeCount > 3)
            {
                printf("Exit MasterCardSwipeTimer And Add Del Card Mode!!!\n");
                if(TimerEnablestatus(MasterCardAddTimer))
                {
                    RefreshTimer(1,MasterCardAddTimer);
                }
                else if(TimerEnablestatus(MasterCardDelTimer))
                {
                    /* 第四次刷增卡，删除全部与母卡开锁权限类型相同卡片，退出删卡 */
                    if(TypeInfo.AddDelAttr != TmpTypeInfo.AddDelAttr)
                    {
                        printf("%s %d TmpTypeInfo.LockAttr:%d\n",__func__,__LINE__,TmpTypeInfo.LockAttr);
                        UserCardPermDel(-1,TmpTypeInfo.LockAttr);
                        UserCardSave();
                    }
                    RefreshTimer(1,MasterCardDelTimer);
                }

                if(TimerEnablestatus(MasterCardSwipeTimer))
                {
                    RefreshTimer(1,MasterCardSwipeTimer);
                }
                VoiceRingPlay(Bi3, VoiceDefVol);
                return 1;
            }

            /* 若连续母卡刷卡时类型不一致，重新计数且更改类型 */
            if(TypeInfo.AddDelAttr != TmpTypeInfo.AddDelAttr && TypeInfo.LockAttr & TmpTypeInfo.LockAttr)
            {
                MasterCardSwipeCount = 1;
                TypeInfo = TmpTypeInfo;
                RefreshTimer(30*1000,MasterCardSwipeTimer);
                VoiceRingPlay(Bi1, VoiceDefVol);
                return 1;
            }

            if(MasterCardSwipeCount == 3)
            {
                printf("Start Open MasterCard%sTimer!!!\n",TypeInfo.AddDelAttr == AddMasterCard ? "Add" : "Del");
                SetTimer(30*1000,TypeInfo.AddDelAttr == AddMasterCard ? MasterCardAddTimer : MasterCardDelTimer,MasterCardAddDelExit,TypeInfo.AddDelAttr == AddMasterCard ? NULL : &TypeInfo.LockAttr);
                CardLightFlashes();
                RefreshTimer(30*1000,MasterCardSwipeTimer);
                VoiceRingPlay(Bi2, VoiceDefVol);
                return 1;
            }
        }
        VoiceRingPlay(Bi1, VoiceDefVol);
        return 1;
    }
    else if(TimerEnablestatus(MasterCardSwipeTimer))
    {
        if(TimerEnablestatus(MasterCardDelTimer))
        {
            int CardIndex = UserCardSearch(Data);
            if (CardIndex != -1)
            {
                printf("Del Card Index:%d,Type:%d\n", CardIndex, TypeInfo.LockAttr);
                if (UserCardPermDel(CardIndex, TypeInfo.LockAttr ))
                {
                    UserCardSave();
                    printf("Del Card [%d] Succeed!!!\n", CardIndex);
                }
            }
            RefreshTimer(30*1000,MasterCardSwipeTimer);
            RefreshTimer(30*1000,MasterCardDelTimer);
            VoiceRingPlay(Bi2, VoiceDefVol);
            return 1;
        }
        else if(TimerEnablestatus(MasterCardAddTimer))
        {
            RefreshTimer(30*1000,MasterCardSwipeTimer);
            RefreshTimer(30*1000,MasterCardAddTimer);
            if(TimerGet(MasterCardAddTimer)->Data == NULL)
            {
                VoiceRingPlay(Bi4, VoiceDefVol);
                return 1;
            }
            int localIndex = UserCardSearch(Data);
            int CardIndex = *((char *)(TimerGet(MasterCardAddTimer)->Data));
            TimerGet(MasterCardAddTimer)->Data = NULL;
            /* 若这张卡没有添加过或在输入的位号相同则进行下一步操作 */
            if(localIndex == -1 || localIndex == CardIndex)
            {
                if (UserCardAdd(CardIndex, Data, localIndex == -1 ? TypeInfo.LockAttr : (DeckIndexPerm(CardIndex) | TypeInfo.LockAttr)))
                {
                    printf("Add Card %d Succeed!!!\n",CardIndex);
                    UserCardSave();
                    VoiceRingPlay(Bi2, VoiceDefVol);
                    return 1;
                }
            }
            VoiceRingPlay(Bi4, VoiceDefVol);
            return 1;
        }
        else
        {
            /* 若母卡连续刷三次途中有用户卡刷新，则退出母卡三刷增删卡模式检测 */
            printf("Action Error !!! Exit MasterCardSwipeTimer.\n");
            TimerDestroy(MasterCardSwipeTimer);
            return 0;
        }
    }
    
    return 0;
}

int UartIcCardModuleHandle(int *DrvFd, char *Data)
{
    static int CardIndex = 0;
    if (UartIcCardCheck(Data) == -1)
    {
        return -1;
    }

    if (TimerEnablestatus(SecirityTriggerTimer))
    {
        return -1;
    }

    CardLightFlashes();

    if(MasterCardSwipeDelect(Data))
    {
        return 0;
    }

    printf("CARD : [%02x %02x %02x %02x %02x]\n", Data[0], Data[1], Data[2], Data[3], Data[4]);
    CardIndex = UserCardSearch(Data);
    if (CardIndex != -1)
    {
        printf("Card %d Exist,Code:", CardIndex);
        for (int i = 0; i < sizeof(UserCardGet()->Deck[CardIndex].Code); i++)
        {
            printf("%c", UserCardGet()->Deck[CardIndex].Code[i]);
        }
        printf("\n");
    }
    

    if (TimerEnablestatus(AddCardTimer))
    {
        int ret = 0;
        int IfNetManage = (TimerGet(AddCardTimer)->Data) ? 0 : 1;
        signed char Index = IfNetManage == 0 ? *((char *)(TimerGet(AddCardTimer)->Data)) : -1;

        if (CardIndex == -1)
        {
            if ((ret = UserCardAdd(Index, Data, 1)))
            {
                printf("Add Card [%d] Succeed!!!\n", Index);
                UserCardSave();
                RefreshTimer(30 * 1000, AddCardTimer);

                goto AddFinish;
                return 1;
            }
        }
        printf("Add Card [%d] Fail!!!\n", Index);

    AddFinish:
        VoiceRingPlay(ret ? Bi2 : Bi4, VoiceDefVol);
        if (IfNetManage)
        {
            NetManageShortPack(1, ManageAddCard, ret ? 1 : 9, 0);
        }
        return 0;
    }
    else if (TimerEnablestatus(DelCardTimer))
    {
        if (CardIndex != -1)
        {
            printf("Del Card Index:%d\n", CardIndex);
            if (UserCardSetPerm(CardIndex, 0))
            {
                UserCardSave();
                VoiceRingPlay(Bi3, VoiceDefVol);
                printf("Del Card [%d] Succeed!!!\n", CardIndex);
                return 1;
            }
        }
        VoiceRingPlay(Bi4, VoiceDefVol);
        printf("Del Card [%d] Fail!!!\n", CardIndex);
        return 0;
    }
    else if (TimerEnablestatus(ModifyCodeCardTimer))
    {
        TimerGet(ModifyCodeCardTimer)->Data = &CardIndex;
        printf("[ModifyCodeCardTimer] CardIndex:%d\n", CardIndex);
        RefreshTimer(30 * 1000, ModifyCodeCardTimer);
        VoiceRingPlay(Bi1, VoiceDefVol);
        return 1;
    }
    else if (TimerEnablestatus(AddMasterCardTimer))
    {
        int CardSize = sizeof(UserCardGet()->MasterCard[LockMasterCard].Card[AddMasterCard]);
        static char CardBuf[sizeof(UserCardGet()->MasterCard[LockMasterCard].Card[AddMasterCard])] = {0};

        if(TimerGet(AddMasterCardTimer)->Data == &UserCardGet()->MasterCard[LockMasterCard].Card[AddMasterCard] || 
            TimerGet(AddMasterCardTimer)->Data == &UserCardGet()->MasterCard[GateMasterCard].Card[AddMasterCard])
        {
            /* 母卡类型可以不同，但功能不能冲突，添加型不能与删除型共用 */
            if(memcmp(Data,UserCardGet()->MasterCard[LockMasterCard].Card[DelMasterCard],CardSize) == 0 ||
                memcmp(Data,UserCardGet()->MasterCard[GateMasterCard].Card[DelMasterCard],CardSize) == 0)
                {
                    printf("Error !!! Master Type Conflicts\n");
                    RefreshTimer(1,AddMasterCardTimer);
                    goto MasetrAddError;
                }

            memcpy(CardBuf,Data,CardSize);
            TimerGet(AddMasterCardTimer)->Data += CardSize;
            printf("Add  Master AddCard!!!\n");
            VoiceRingPlay(Bi1, VoiceDefVol);
            return 1;
        }
        else if(TimerGet(AddMasterCardTimer)->Data && memcmp(CardBuf,Data,CardSize) != 0)
        {
            /* 母卡类型可以不同，但功能不能冲突，添加型不能与删除型共用 */
            if(memcmp(Data,UserCardGet()->MasterCard[LockMasterCard].Card[AddMasterCard],CardSize) == 0 ||
                memcmp(Data,UserCardGet()->MasterCard[GateMasterCard].Card[AddMasterCard],CardSize) == 0)
                {
                    printf("Error !!! Master Type Conflicts\n");
                    RefreshTimer(1,AddMasterCardTimer);
                    goto MasetrAddError;
                }

            printf("Add Master Del Card!!!\n");
            memcpy(TimerGet(AddMasterCardTimer)->Data - CardSize,CardBuf,CardSize);
            memcpy(TimerGet(AddMasterCardTimer)->Data,Data,CardSize);
            UserCardSave();
            RefreshTimer(1,AddMasterCardTimer);
            VoiceRingPlay(Bi2, VoiceDefVol);
            for(int i = LockMasterCard ;i <= GateMasterCard;i ++)
            {
                printf("Add%sMasterCard:[%x %x %x %x %x]\n",i?"Gate":"Lock",UserCardGet()->MasterCard[i].Card[AddMasterCard][0],
                UserCardGet()->MasterCard[i].Card[AddMasterCard][1],
                UserCardGet()->MasterCard[i].Card[AddMasterCard][2],
                UserCardGet()->MasterCard[i].Card[AddMasterCard][3],
                UserCardGet()->MasterCard[i].Card[AddMasterCard][4]);
                printf("Del%sMasterCard:[%x %x %x %x %x]\n",i?"Gate":"Lock",UserCardGet()->MasterCard[i].Card[DelMasterCard][0],
                UserCardGet()->MasterCard[i].Card[DelMasterCard][1],
                UserCardGet()->MasterCard[i].Card[DelMasterCard][2],
                UserCardGet()->MasterCard[i].Card[DelMasterCard][3],
                UserCardGet()->MasterCard[i].Card[DelMasterCard][4]);
            }
            return 1;
        }
        MasetrAddError:
        RefreshTimer(1,AddMasterCardTimer);
        VoiceRingPlay(Bi4, VoiceDefVol);
        return 1;
    }

    if (CardIndex != -1)
    {
        if (UserConfigGet()->LockWay == CardAndCodeWay)
        {
            if (TimerEnablestatus(CodeCardUnlockTimer))
            {
                TimerDestroy(CodeCardUnlockTimer);
                goto error;
            }
            else
            {
                SetTimer(30 * 1000, CodeCardUnlockTimer, NULL, &CardIndex);
                VoiceRingPlay(Bi1, VoiceDefVol);
                return 1;
            }
        }
        else
        {
            SecurityErrorReset();
            // VoiceRingPlay(Bi1, VoiceDefVol);   hare

            if (UserCardGet()->Deck[CardIndex].Perm & LOCK_TYPE)
                Unlock(UserConfigGet()->UnlockTime, LOCK_TYPE);
            if (UserCardGet()->Deck[CardIndex].Perm & GATE_TYPE)
                Unlock(UserConfigGet()->UngateTime, GATE_TYPE);

            printf("Verify Card Succeed!!! Permissions[%s]\n", UserCardGet()->Deck[CardIndex].Perm == LOCK_TYPE ? "Lock" : UserCardGet()->Deck[CardIndex].Perm == GATE_TYPE ? "Gate"
                                                                                                                                                                            : "Total");
            return 1;
        }
    }
error:
    VoiceRingPlay(Bi4, VoiceDefVol);
    SecurityErrorUpdate();
    printf("Verify Card  Fail!!!\n");
    return 0;
}

int UartIcCardModuleClear(int Fd)
{
    UartClear(Fd);
    return 0;
}
/***********************************************************************************************************************/