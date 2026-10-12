/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-12-12 09:05:34
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-01-23 21:28:07
 * @FilePath: /project_3/src/GeneralInterface.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "GeneralInterface.h"
#include "CircularList.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"

/**
 * @description: 获取时间差
 * @param {timespec} *last_time 基准时间
 * @return {*}  时间差值
 */
unsigned long long DiffClockTimeMs(struct timespec *last_time)
{
    struct timespec curr_time;
    unsigned long long diff;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    diff = (curr_time.tv_sec - last_time->tv_sec) * 1000 + (curr_time.tv_nsec - last_time->tv_nsec) / 1000000;

    return diff;
}

/**
 * @description: 获取时间
 * @param {timespec} *time 时间缓存
 * @return {*}  时间差值
 */
void GetClockTimeMs(struct timespec *time)
{
    clock_gettime(CLOCK_MONOTONIC, time);
    return;
}

/**
 * @description: 获取当前文件编译时间
 * @return {*}
 */
struct tm FetchCompileTime(void)
{
    static struct tm *pt = NULL;
    if (pt == NULL)
    {
        const char *s_date = __DATE__;
        const char *s_time = __TIME__;

        // 解析日期时间字符串，获取对应的时间结构体
        static struct tm t;
        strptime(s_date, "%b %d %Y", &t);
        strptime(s_time, "%H:%M:%S", &t);
        pt = &t;
    }
    return *pt;
}

/**
 * @description: 居中打印并空格补余
 * @param {char*} str 字符串
 * @param {int} width 整体长度
 * @return {*}
 */
void PrintCentered(const char *str, int width) {
    int len = strlen(str);
    int padding = width - len - 2; // 减去两边的[]符号
    if (padding > 0) {
        int left_padding = padding / 2;      // 左侧填充空格
        int right_padding = padding - left_padding; // 右侧填充空格
        printf("[%*s%s%*s]\n", left_padding, "", str, right_padding, "");
    } else {
        // 如果字符串长度大于或等于总宽度，直接输出原字符串
        printf("[%s]\n", str);
    }
}

DECLARE_LIST(SystemEventList)

/**
 * @description: 系统事件循环
 * @return {*}
 */
int SystemEventLoop(void)
{
    CreateCircularList(&SystemEventList);
    while (1)
    {
        SEvent *Event = NULL;
        if (CircularListRead(&SystemEventList, (void *)&Event) != -1)
        {
            Event->Func(Event);
            atomic_store(&Event->Valid, 0);
        }
    }
    return 1;
}

/**
 * @description: 系统事件注册
 * @return {*}
 */
int SystemEventRegister(SEvent *Event)
{
    assert(Event != NULL);
    if(atomic_load(&Event->Valid))
    {
        return 0;
    }

    ListNode *Node = CircularListRequest(&SystemEventList);
    if (Node != NULL)
    {
        Node->Data = Event;
        atomic_store(&Event->Valid, 1);
        // printf("[%s] Event:%p\n", __func__, Event);
        if (CircularListWrite(&SystemEventList, Node) == -1)
        {
            atomic_store(&Event->Valid, 0);
            printf("[%s] SystemEventList Write Fail\n", __func__);
            return 0;
        }
        return 1;
    }
    return 0;
}