/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-01-29 08:54:39
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-01-29 17:11:19
 * @FilePath: /82225-EPC/src/SecurityMode.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef _SECURITY_MODE_H_
#define _SECURITYMODE_H_

/**
 * @description: 清零错误次数
 * @return {*}
 */
void SecurityErrorReset(void);

/**
 * @description: 错误操作计数
 * @return {*}
 */
int SecurityErrorUpdate(void);
#endif