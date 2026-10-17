/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-26 14:42:21
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-07-22 10:02:24
 * @FilePath: /82225-EPC/src/UserConfig.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "UserConfig.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>

#include <stdbool.h>
#include <../common/watch_dog/ak_drv_wdt.h>
#include <ak_common.h>

#define DEFAULT_UNLOCK_TIME 5

static UserConfig UserConf;
static UserConfig UserConfDefault = {
    .AdminCode = {'1', '2', '3', '4', '\0'},
    .UnlockCode = {'2', '5', '8', '0', '\0'},
    .UngateCode = {'2', '5', '9', '0', '\0'},
    .PublicUnlockEn = 1,
    .UnlockVoiceEn = 1,
    .UngateTime = DEFAULT_UNLOCK_TIME,
    .UnlockTime = DEFAULT_UNLOCK_TIME,
    .NumKeyLightTime = 0,
    .Language = Hebrew,
    .LockWay = CardOrCodeWay,
    .SafeMode = CloseSafe,
    .CodeInfo[0 ... (PUBLIC_CODE_GROUP-1)] = {0},
    .cmd_status = true,
};

/**
 * @description: 保存默认用户配置
 * @return {*}
 */
int UserDefaultConfigSave(void)
{
    int fd = open(USER_DEFAUILT_DATA_PATH, O_WRONLY | O_CREAT);
    if (fd < 0)
    {
        printf("write open %s fail \n", USER_DEFAUILT_DATA_PATH);
        return 0;
    }

    write(fd, &UserConfDefault, sizeof(UserConfig));

    close(fd);
    system("fsync -d " USER_DEFAUILT_DATA_PATH);
    return 1;
}

/**
 * @description: 保存用户配置
 * @return {*}
 */
int UserConfigSave(void)
{
    int fd = open(USER_DATA_PATH, O_WRONLY | O_CREAT);
    if (fd < 0)
    {
        printf("write open %s fail \n", USER_DATA_PATH);
        return 0;
    }

    write(fd, &UserConf, sizeof(UserConfig));

    close(fd);
    system("fsync -d " USER_DATA_PATH);
    return 1;
}

/**
 * @description: 获取用户配置
 * @return {*}
 */
UserConfig *UserConfigGet(void)
{
    return &UserConf;
}

/**
 * @description: 获取默认用户配置
 * @return {*}
 */
UserConfig *UserDefaultConfigGet(void)
{
    return &UserConfDefault;
}

/**
 * @description: 用户配置初始化
 * @return {*}
 */
int UserConfigInit(void)
{
    int DefaultFd = open(USER_DEFAUILT_DATA_PATH,O_CREAT | O_RDONLY);
    if(DefaultFd)
    {
        UserConfig TmpUserConf;
        if(read(DefaultFd, &TmpUserConf, sizeof(TmpUserConf)))
        {
            UserConfDefault.Language = TmpUserConf.Language;
            UserConfDefault.UnlockTime = TmpUserConf.UnlockTime;
            UserConfDefault.UngateTime = TmpUserConf.UngateTime;
            memcpy(UserConfDefault.UnlockCode,TmpUserConf.UnlockCode,sizeof(TmpUserConf.UnlockCode));
            memcpy(UserConfDefault.UngateCode,TmpUserConf.UngateCode,sizeof(TmpUserConf.UngateCode));
        }
        close(DefaultFd);
    }

    int fd = open(USER_DATA_PATH, O_RDONLY);
    if (fd < 0)
    {
        UserConf = UserConfDefault;
        UserConfigSave();
        return 0;
    }

    read(fd, &UserConf, sizeof(UserConfig));

    close(fd);

    return 1;
}

/**
 * @description: 恢复用户默认配置
 * @return {*}
 */
int UserConfigReset(void)
{
    printf("[%s]\n", __func__);
    int Language = UserConf.Language;
    UserConf = UserConfDefault;
    UserConf.Language = Language;
    UserConfigSave();
    // printf("\nuserdate_cmd_state=///////==================%d\n",UserConfigGet() -> cmd_status);
    return 1;
}

/**
 * @description: 设置用户密码权限
 * @param {int} index   密码索引
 * @param {char} Permission   密码权限
 * @return {*}
 */
void UserCodeSetPerm(int Index,char Permission)
{
    if(Index < PUBLIC_CODE_GROUP)
    {
        if(UserConf.CodeInfo[Index].Type != Permission)
        {
            UserConf.CodeInfo[Index].Type = Permission;
            UserConfigSave();
        }
    }
}

/**
 * @description: 添加用户密码
 * @param {int} Code   密码(十进制表示)
 * @return {*}
 */
void UserCodeAdd(int Code)
{
    for(int Index = 0;Index < PUBLIC_CODE_GROUP;Index ++)
    {
        if(UserConf.CodeInfo[Index].Type == 0)
        {
            memset(UserConf.CodeInfo[Index].Password,0,PUBLIC_CODE_LEN);
            for(int j = PUBLIC_CODE_LEN-1; j >= 0 ;j --)
            {
                UserConf.CodeInfo[Index].Password[j] = Code%10;
                Code = Code/10;
            }
            UserConf.CodeInfo[Index].Type = 1;
            break;
        }
    }
    UserConfigSave();
}

/**
 * @description: 删除用户密码
 * @param {int} Index   密码索引
 * @return {*}
 */
void UserCodeDel(int Index)
{
    if(Index == 200)
    {
        memset( UserConf.CodeInfo,0,sizeof(UserCodeInfo)*PUBLIC_CODE_GROUP);        
    }
    else if(Index < PUBLIC_CODE_GROUP)
    {
        memset( UserConf.CodeInfo[Index].Password,0,PUBLIC_CODE_LEN);       
         UserConf.CodeInfo[Index].Type = 0;
    }
    UserConfigSave();
}

/**
 * @description: 用户密码校验
 * @param {int} Index   密码(字符串表示)
 * @return {*}
 */
int UserCodeVerify(char *Code)
{
    #define Atoi(c) (c - 48)
    char TmpCode[PUBLIC_CODE_LEN] = {Atoi(Code[0]),Atoi(Code[1]),Atoi(Code[2]),Atoi(Code[3])};
    for(int Index = 0;Index < PUBLIC_CODE_GROUP;Index ++)
    {
        /* 只需要四位密码 */
        if(UserConf.CodeInfo[Index].Type != 0 && memcmp(UserConf.CodeInfo[Index].Password,TmpCode,PUBLIC_CODE_LEN - 1) == 0)
        {
            return UserConf.CodeInfo[Index].Type;
        }
    }
    return 0;
}

/**
 * @description: 获取密码组权限及数据
 * @param {char} *Deck  缓存
 * @return {*} 组数据大小
 */
int UserCodeInfoGet(unsigned char **Deck)
{
    unsigned char Buffer[PUBLIC_CODE_GROUP][sizeof(UserCodeInfo)];
    static unsigned char DeckData[1280];
    memset(DeckData, 0, sizeof(DeckData));
    
    for(int i = 0;i < PUBLIC_CODE_GROUP;i ++)
    {
        if(UserConf.CodeInfo[i].Type)
        {
            DeckData[0] ++;
            memcpy(&Buffer[i], &UserConf.CodeInfo[i], sizeof(UserCodeInfo));
        }
    }

    memcpy(&DeckData[1], Buffer, sizeof(Buffer));
    *Deck = DeckData;
    // printf("DeckData[%p]\n", DeckData);
    return sizeof(DeckData);
}
/******************喂狗*************************/
static bool watch_dog_flag = false;

void watchdog_open(void)
{
	ak_drv_wdt_open(5);  //liu: 10 -> 5
	watch_dog_flag = true;
}

void watch_dog_close(void)
{
	if (watch_dog_flag)
	{
        printf("\n*************关门放狗**************\n");
		ak_drv_wdt_close();
		watch_dog_flag = false;
	}
}

void watch_dog_feed(void)
{
	if (watch_dog_flag == true)
	{
		ak_drv_wdt_feed();
	}
}

// 获取当前时间秒
unsigned long os_get_second(void)
{
	struct ak_timeval tv;
	ak_get_ostime(&tv);
	return tv.sec;
}