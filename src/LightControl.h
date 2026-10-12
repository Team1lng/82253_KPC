/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-06-06 09:43:29
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-08-05 13:40:58
 * @FilePath: /Non_visual_indoor/src/LightControl.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef LIGHT_CONTROL_H
#define LIGHT_CONTROL_H

#define LIGHT(MACRO)                    \
    /**                                 \
     * @description: 刷卡灯光控制 \
     * @param {int} en 使能标志位  \
     * @return {*}                      \
     */                                 \
    MACRO(Card, 4)                     \
    /**                                 \
     * @description: 键盘背光控制 \
     * @param {int} en 使能标志位  \
     * @return {*}                      \
     */                                 \
    MACRO(Keypad, 6)                   \
    /**                                 \
     * @description: 红外灯光控制 \
     * @param {int} en 使能标志位  \
     * @return {*}                      \
     */                                 \
    MACRO(Infrared, 5)  \
    /**                                 \
     * @description: 通话灯光控制 \
     * @param {int} en 使能标志位  \
     * @return {*}                      \
     */                                 \
    MACRO(Talk, 7)\
    /**                                 \
     * @description: Door开锁控制 \
     * @param {int} en 使能标志位  \
     * @return {*}        hare              \
     */                                 \
    MACRO(Gateopen, 6)\
    /**                                 \
     * @description: Gate开锁控制 \
     * @param {int} en 使能标志位  \
     * @return {*}      hare                \
     */                                 \
    MACRO(Dooropen, 7)

#define CONTROL_FUNC_DECLA(Func, ...) \
    int Func##LightControl(int en);

/**
 * @description: 灯光控制函数声明
 * @return {*}
 */
LIGHT(CONTROL_FUNC_DECLA)

/**
 * @description: 刷卡灯光闪烁
 * @return {*}
 */
void CardLightFlashes(void);

/**
 * @description: 按键灯光控制
 * @param {int} index 按键索引 [-1 - 全关] [0~3 - 开启]
 * @return {*}
 */
void KeyLightControl(int index);

/**
 * @description: 灯光引脚初始化
 * @return {*}
 */
void LightGpioInit(void);

int is_network_audio_send_package_open(void);
void network_audio_send_package_start(void);
void network_audio_send_package_stop(void);

int is_network_video_send_package_open(void);
void network_video_send_package_start(void);
void network_video_send_package_stop(void);
#endif