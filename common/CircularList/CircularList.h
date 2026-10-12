/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-13 13:47:42
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-18 14:15:45
 * @FilePath: /project_3/common/CircularList.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef _CIRCULAR_LIST_H_
#define _CIRCULAR_LIST_H_
#include "List.h"
#include <unistd.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <pthread.h>

#define ASSERT(condition, message)                                                         \
    do                                                                                     \
    {                                                                                      \
        if (!(condition))                                                                  \
        {                                                                                  \
            if (message)                                                                   \
            {                                                                              \
                fprintf(stderr, "Assertion failed: %s\nFile: %s\nLine: %d\nMessage: %s\n", \
                        #condition, __FILE__, __LINE__, message);                          \
            }                                                                              \
            abort();                                                                       \
        }                                                                                  \
    } while (0)

#define LIST_NODE_MAX 10
typedef struct
{
    void *Data;
    struct list_head Ptr;
} ListNode;

typedef struct
{
    atomic_int Inited;
    atomic_int WriteIndex;
    atomic_int ReadIndex;
    atomic_int RequestIndex;
    atomic_int RecvBlock;
    pthread_t ListLockId;
    struct list_head Head;
    ListNode NodeList[LIST_NODE_MAX];
    sem_t Sem;
    pthread_mutex_t Mutex;
    const char *ListName;
} CircularList;

#define CIRCULAR_LIST_INIT(LIST) { \
    .Inited = 0,                   \
    .WriteIndex = 0,               \
    .ReadIndex = 0,                \
    .RequestIndex = 0,             \
    .ListName = #LIST,             \
};

#define DECLARE_LIST(LIST) \
    static CircularList LIST = CIRCULAR_LIST_INIT(LIST);

/**
 * @description: 创建环形队列,建议事先使用DECLARE_LIST宏定义列表
 * @param {CircularList} *List  队列头指针
 * @return {*}
 */
int CreateCircularList(CircularList *List);

/**
 * @description: 设置环形队列接收阻塞状态
 * @param {CircularList} *List
 * @return {*}
 */
int CircularListRecvBolck(CircularList *List, int Block);

/**
 * @description: 数据写入环形队列
 * @param {CircularList} *List  队列头指针
 * @param {void} *Node  写入数据
 * @return {*}
 */
int CircularListWrite(CircularList *List, ListNode *Node);

/**
 * @description: 读取环形队列数据
 * @param {CircularList} *List  队列头指针
 * @param {void} *Node  读出数据缓存
 * @return {*}
 */
int CircularListRead(CircularList *List, void **Data);

/**
 * @description:    环形队列为空
 * @param {CircularList} *List  队列头指针
 * @return {*}
 */
int CircularListEmpty(CircularList *List);

/**
 * @description:    申请队列节点
 * @param {CircularList} *List  队列头指针
 * @return {*}
 */
ListNode *CircularListRequest(CircularList *List);

/**
 * @description: 环形上锁
 * @return {*}
 */
int CircularListLock(CircularList *List);

/**
 * @description: 环形解锁
 * @return {*}
 */
int CircularListUnlock(CircularList *List);
#endif