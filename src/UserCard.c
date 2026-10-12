#include "UserCard.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

static CardInfo UserDeck = {.DeckSize = 0,.Deck[0 ... DECK_SIZE_MAX-1].Perm = 0};

/**
 * @description: 保存卡片信息
 * @return {*}
 */
int UserCardSave(void)
{
    int fd = open(CARD_CONFIG_PATH, O_WRONLY | O_CREAT);
    if (fd < 0)
    {
        printf("write open %s fail \n", CARD_CONFIG_PATH);
        return 0;
    }

    write(fd, &UserDeck, sizeof(CardInfo));

    close(fd);
    system("fsync -d " CARD_CONFIG_PATH);
    return 1;
}

/**
 * @description: 获取卡片信息
 * @return {*}
 */
CardInfo *UserCardGet(void)
{
    return &UserDeck;
}

/**
 * @description: 卡组数据检查
 * @return {*}
 */
static void UserDeckCheck(void)
{
    UserDeck.DeckSize = 0;
    for(int i = 0;i < DECK_SIZE_MAX;i ++)
    {
        char *Data = UserDeck.Deck[i].Data;
        if((Data[0] ^ Data[1] ^ Data[2] ^ Data[3]) != Data[4])
        {
            UserDeck.Deck[i].Perm = 0;
        }
        else if(UserDeck.Deck[i].Perm)
        {
            UserDeck.DeckSize ++;
        }
    }
    UserCardSave();
    return ;
}

/**
 * @description: 卡组初始化
 * @return {*}
 */
int UserDeckInit(void)
{
    int fd = open(CARD_CONFIG_PATH, O_RDONLY);
    if (fd < 0)
    {
        UserCardSave();
        return 0;
    }

    read(fd, &UserDeck, sizeof(CardInfo));

    close(fd);

    UserDeckCheck();

    return 1;
}

/**
 * @description:  卡组格式化
 * @return {*}
 */
int UserDeckFormat(void)
{
    UserDeck.DeckSize = 0;
    memset(&UserDeck.Deck, 0, sizeof(UserDeck.Deck));
    UserCardSave();
    return 1;
}

/**
 * @description: 添加卡片
 * @param {int} index   卡片存储索引
 * @param {char} *data  卡片数据
 * @return {*}0-失败，1-成功
 */
int UserCardAdd(int index, char *data, char permissions)
{
    if (index >= DECK_SIZE_MAX || UserDeck.DeckSize >= DECK_SIZE_MAX)
        return 0;

    if (index == -1)
    {
        for (index = 0; index < DECK_SIZE_MAX; index++)
        {
            if (UserDeck.Deck[index].Perm == 0)
            {
                memcpy(UserDeck.Deck[index].Data, data, sizeof(UserDeck.Deck[index].Data));
                memcpy(UserDeck.Deck[index].Code, CARD_INITIAL_CODE, sizeof(UserDeck.Deck[index].Code));
                UserDeck.DeckSize++;
                break;
            }
        }
    }
    else if (UserDeck.Deck[index].Perm != 0)
    {
        char *Data = UserDeck.Deck[index].Data;
        if((Data[0] ^ Data[1] ^ Data[2] ^ Data[3]) != Data[4])
        {
            printf("UserCardAdd Card %d Data Error!!!,Replace Current Card!!!\n\n",index);
            UserDeckCheck();
            memcpy(UserDeck.Deck[index].Data, data, sizeof(UserDeck.Deck[index].Data));
            memcpy(UserDeck.Deck[index].Code, CARD_INITIAL_CODE, sizeof(UserDeck.Deck[index].Code));
            UserDeck.DeckSize++;
        }
        else if (memcmp(UserDeck.Deck[index].Data, data, sizeof(UserDeck.Deck[index].Data)) != 0)
        {
            return 0;
        }
    }
    else
    {
        memcpy(UserDeck.Deck[index].Data, data, sizeof(UserDeck.Deck[index].Data));
        memcpy(UserDeck.Deck[index].Code, CARD_INITIAL_CODE, sizeof(UserDeck.Deck[index].Code));
        UserDeck.DeckSize++;
    }
    UserDeck.Deck[index].Perm |= permissions;
    printf("Add Card[%d][%x %x %x %x %x] Permission:%d\n",index,UserDeck.Deck[index].Data[0],UserDeck.Deck[index].Data[1],UserDeck.Deck[index].Data[2],UserDeck.Deck[index].Data[3],UserDeck.Deck[index].Data[4],UserDeck.Deck[index].Perm);
    UserCardSave();
    return 1;
}

/**
 * @description: 设置卡片权限，卡片权限为0时删除卡片
 * @param {int} index   卡片存储索引
 * @param {char} permissions   卡权限
 * @return {*}0-失败，1-成功
 */
int UserCardSetPerm(int index, char permissions)
{
    if (index >= DECK_SIZE_MAX)
        return 0;
    if (permissions < 0 || permissions > 3)
        return 0;

    if (UserDeck.Deck[index].Perm == 0)
        return 0;

    if (!(UserDeck.Deck[index].Perm = permissions))
    {
        memset(UserDeck.Deck[index].Data, 0, sizeof(UserDeck.Deck[index].Data));
        if(UserDeck.DeckSize)
        UserDeck.DeckSize--;
    }

    UserCardSave();
    return 1;
}

/**
 * @description: 删除卡片权限，卡片权限为0时删除卡片
 * @param {int} index   卡片存储索引，-1为删除所以卡片该权限
 * @param {char} permissions   卡权限
 * @return {*}0-失败，1-成功
 */
int UserCardPermDel(int index, char permissions)
{
    if (index >= DECK_SIZE_MAX)
        return 0;
    if (permissions < 0 || permissions > 3)
        return 0;
    
    if(index == -1)
    {
        for (index = 0; index < DECK_SIZE_MAX; index++)
        {
            if (UserDeck.Deck[index].Perm && !(UserDeck.Deck[index].Perm &= ~permissions))
            {
                memset(UserDeck.Deck[index].Data, 0, sizeof(UserDeck.Deck[index].Data));
                UserDeck.Deck[index].Perm = 0;
                if(UserDeck.DeckSize)
                UserDeck.DeckSize--;
            }
        }
    }
    else if (UserDeck.Deck[index].Perm && !(UserDeck.Deck[index].Perm &= ~permissions))
    {
        memset(UserDeck.Deck[index].Data, 0, sizeof(UserDeck.Deck[index].Data));
        UserDeck.Deck[index].Perm = 0;
        if(UserDeck.DeckSize)
        UserDeck.DeckSize--;
    }
    UserCardSave();
    return 1;
}

/**
 * @description: 用户卡搜索
 * @param {char} *data  卡片数据
 * @return {*}卡片索引
 */
int UserCardSearch(char *data)
{
    if (!UserDeck.DeckSize)
        return -1;

    int DeckSize = UserDeck.DeckSize;
    for (int i = 0; i < sizeof(UserDeck.Deck) / sizeof(UserCard); i++)
    {
        if (UserDeck.Deck[i].Perm)
        {
            if (memcmp(UserDeck.Deck[i].Data, data, sizeof(UserDeck.Deck[i].Data)) == 0)
            {
                return i;
            }

            if (!(--DeckSize))
                break;
        }
    }
    return -1;
}

/**
 * @description: 卡片密码校验
 * @param {int} Index   卡片索引
 * @param {char} *Code  卡片密码
 * @param {int} CodeLen 密码长度
 * @return {*}0-失败，x-权限
 */
int CardCodeVerify(int Index, char *Code, int CodeLen)
{
    if (CodeLen != sizeof(UserDeck.Deck[0].Code))
        return 0;

    if (memcmp(UserDeck.Deck[Index].Code, Code, CodeLen) == 0)
    {
        return UserDeck.Deck[Index].Perm;
    }
    return 0;
}

/**
 * @description: 搜索密码权限
 * @param {char} *Code  密码
 * @param {int} CodeLen 密码长度
 * @return {*}0-失败，x-权限
 */
int CardCodePermission(char *Code, int CodeLen)
{
    if (CodeLen != sizeof(UserDeck.Deck[0].Code))
        return 0;
    if (memcmp(Code, CARD_INITIAL_CODE, strlen(CARD_INITIAL_CODE)) == 0)
        return 0;

    int DeckSize = UserDeck.DeckSize;
    for (int i = 0; i < sizeof(UserDeck.Deck) / sizeof(UserCard); i++)
    {
        if (UserDeck.Deck[i].Perm)
        {
            if (memcmp(UserDeck.Deck[i].Code, Code, sizeof(UserDeck.Deck[i].Code)) == 0)
            {
                return UserDeck.Deck[i].Perm;
            }

            if (!(--DeckSize))
                break;
        }
    }
    return 0;
}

/**
 * @description: 获取卡组索引权限
 * @param {char} Index 索引
 * @return {*}0-空，x-权限
 */
char DeckIndexPerm(int Index)
{
    if (Index >= DECK_SIZE_MAX)
        return 0;
    return UserDeck.Deck[Index].Perm;
}

/**
 * @description: 获取卡组权限及卡号
 * @param {char} *Deck  卡组缓存
 * @return {*} 卡组数据大小
 */
int UserDeckPermGet(unsigned char **Deck)
{
    unsigned char Buffer[DECK_SIZE_MAX][6];
    static unsigned char DeckData[1280];
    memset(DeckData, 0, sizeof(DeckData));
    DeckData[0] = UserDeck.DeckSize;
    for (int i = 0; i < DECK_SIZE_MAX; i++)
    {
        memcpy(&Buffer[i], &UserDeck.Deck[i], 6);
    }
    memcpy(&DeckData[1], Buffer, sizeof(Buffer));
    *Deck = DeckData;
    // printf("DeckData[%p]\n", DeckData);
    return sizeof(DeckData);
}

/**
 * @description: 母卡类型校验
 * @param {char} *Code  卡片信息
 * @param {MasterCardType} *TypeInfo  主卡类型信息
 * @return {MasterCardType}0-失败，1-成功
 */
int MasterCardTypeVerify(char *Data,MasterCardType *TypeInfo)
{
    int result = 0;
    if (Data == NULL || TypeInfo == NULL)
        return 0;

    int CodeLen = sizeof(UserDeck.MasterCard[0].Card[0]);
    for(int i = LockMasterCard;i <= GateMasterCard;i ++)
    {
        for(int j = AddMasterCard;j <= DelMasterCard;j ++)
        {
            if (memcmp(UserDeck.MasterCard[i].Card[j], Data, CodeLen) == 0)
            {
                result = 1;
                TypeInfo->LockAttr |= (1 << i);
                TypeInfo->AddDelAttr = j;
            }
        }
    }
    // printf("%s %d\n",__func__,TypeInfo->LockAttr);
    return result;
}