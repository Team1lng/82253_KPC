/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2023-10-10 10:03:37
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-12-12 09:13:18
 * @FilePath: /project_2/main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "CircularList.h"
#include "GeneralInterface.h"
#include "ak_common.h"
#include "GpioControl.h"
#include "UserConfig.h"
#include "UserCard.h"
#include <fcntl.h>
#include "ak_log.h"
#include "LightControl.h"
#include "NetworkCommon.h"

#include <sys/time.h>
static unsigned long long current_system_timestamp = 0;
// static pthread_mutex_t timestamp_mutex = PTHREAD_MUTEX_INITIALIZER;

#define NETMSG_ENABLE
#define VOICE_AO_ENABLE
#define VIDEO_TRANSFER_ENABLE
#define AUDIO_TRANSFER_ENABLE
#define ADC_DETECT_ENABLE
#define EPOLL_GPIO_ENABLE
#define TIMER_ENABLE
#define LOCK_ENABLE
#define IR_FEED_ENABLE
#define USER_MANAGEMENT

#ifdef FINGERPRINT_ENABLE
    #include"Fingerprint.h"
#endif

#ifdef USER_MANAGEMENT
#include "UserNetManage.h"
#endif

#ifdef IR_FEED_ENABLE
#include "InfraredDetect.h"
#endif

#ifdef LOCK_ENABLE
#include "Unlock.h"
#endif

#ifdef TIMER_ENABLE
#include "Timer.h"
#endif

#ifdef EPOLL_GPIO_ENABLE
#include "EpollGpioEvent.h"
#endif

#ifdef ADC_DETECT_ENABLE
#include "AdcControl.h"
#endif

#ifdef FINGERPRINT_ENABLE
#include "Fingerprint.h"
#endif

#ifdef VIDEO_TRANSFER_ENABLE
#define MOTION_DETECT_ENABLE
#endif

#ifdef KEYPAD_ENABLE
#include "DrvNumericKeypad.h"
#endif

#ifdef CARD_ENABLE
#include "DrvSwipeCard.h"
#endif

#ifdef MOTION_DETECT_ENABLE
#include "VideoInput.h"
#include "MotionDetect.h"
#include "SmartVisionPlatform.h"
#endif

#ifdef VOICE_AO_ENABLE
#include "VoiceDecode.h"
#include "VoiceRingPlay.h"
#include "AudioPlay.h"
#endif

#ifdef NETMSG_ENABLE
#include "NetMsgComm.h"
#endif

#ifdef VIDEO_TRANSFER_ENABLE
#include "VideoInput.h"
#include "VideoTransfer.h"
#endif

#ifdef AUDIO_TRANSFER_ENABLE
#include "AudioTransfer.h"
#endif

#ifdef ITS_ENABLE
#include "ak_its.h"
#endif

#ifdef ATS_ENABLE
#include "ak_ats.h"
#endif

static void AKPlatformSdkInit(void)
{
    sdk_run_config Config;
    memset(&Config, 0, sizeof(Config));
    Config.mem_trace_flag = SDK_RUN_NORMAL;
    Config.isp_tool_server_flag = 0;
    Config.audio_tool_server_flag = 0;
#ifdef ITS_ENABLE
    Config.isp_tool_server_flag = 1;
#endif
#ifdef ATS_ENABLE
    Config.audio_tool_server_flag = 1;
#endif
    ak_sdk_init(&Config);
    ak_print_set_level(MODULE_ID_SVP, LOG_LEVEL_NORMAL);
}

#ifdef EPOLL_GPIO_ENABLE
static int HouseSwitchHandle(int Level)
{
    printf("[%s][%d]\n", __func__, Level);
    system("reboot");
    return 0;
}
int HouseSwitchEpollEventInit(struct EpollEvent *Event)
{
#define HOUSE_SWITCH_GPIO 26
    GPIO_LEVEL Level = GPIO_LEVEL_UNKNOWN;
    if (GpioOpen(HOUSE_SWITCH_GPIO, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioLevelGet(HOUSE_SWITCH_GPIO, &Level);
    GpioEdge(HOUSE_SWITCH_GPIO, Level == GPIO_LEVEL_LOW ? RISING_EDGE : FALLING_EDGE);
#ifdef NETMSG_ENABLE
    NetworkDevice DevId = Level == GPIO_LEVEL_LOW ? DEVICE_OUTDOOR_1 : DEVICE_OUTDOOR_2;
    NetLocalDeviceIDSet(DevId);
#endif
#ifdef VIDEO_TRANSFER_ENABLE
    VideoNetSocketProtocolSet(DevId);
#endif
#ifdef AUDIO_TRANSFER_ENABLE
    AudioNetSocketProtocolSet(DevId);
#endif
    if (Event == NULL)
    {
        return 0;
    }

    char Path[64] = {0};
    memset(Path, 0, sizeof(Path));
    sprintf(Path, "/sys/class/gpio/gpio%d/value", HOUSE_SWITCH_GPIO);
    Event->Fd = open(Path, O_RDONLY);
    Event->TriggerLevel = !Level;
    Event->EpollEventHandle = HouseSwitchHandle;
    return 0;
}
#endif
/**********************为了防止启动被load_isp_conf 损坏，增加心跳检测*********************** */
// 获取当前时间戳（毫秒）
static unsigned long long get_timestamp(void) {
    struct timeval val;
    // gettimeofday在大部分系统中是线程安全的
    if (gettimeofday(&val, NULL) != 0) {
        perror("gettimeofday failed");
        return 0;
    }
    return val.tv_sec * 1000 + val.tv_usec / 1000;
}
// 重置系统时间戳（线程安全）
static void reset_system_timestamp(void) {
    unsigned long long new_ts = get_timestamp();
    // 加锁保护共享变量的写入
    // if (pthread_mutex_lock(&timestamp_mutex) != 0) {
    //     perror("pthread_mutex_lock failed");
    //     return;
    // }
    current_system_timestamp = new_ts;
    // pthread_mutex_unlock(&timestamp_mutex);
}
static void* systemtick_thrad(void* arg){

    while(1){
        unsigned long long current_ts = get_timestamp();
        unsigned long long last_ts;

        // 加锁保护共享变量的读取
        // if (pthread_mutex_lock(&timestamp_mutex) != 0) {
        //     perror("pthread_mutex_lock failed");
        //     usleep(1000 * 100); // 等待100ms后重试
        //     continue;
        // }
        last_ts = current_system_timestamp;
        // pthread_mutex_unlock(&timestamp_mutex);
        if (current_ts > last_ts)
        {
            if ((current_ts - last_ts) > 10 * 1000) {
                printf("\n#####################################\n");
                printf("--------system bad ....;reboot...\n");
                printf("#####################################\n");
                // 释放互斥锁资源
                // pthread_mutex_destroy(&timestamp_mutex);
                exit(0); // 使用EXIT_FAILURE更合理
            }
        }
        
        usleep(1000*100);
    }
    return  NULL;
}
static void systemtick_init(void){

    pthread_t tid;
    reset_system_timestamp();
    pthread_create(&tid,NULL,systemtick_thrad,NULL);
    pthread_detach(tid);
}
/**********door切换检测**************/
GPIO_LEVEL level = GPIO_LEVEL_UNKNOWN;
static void *house_and_systemtick(void *arg)
{
    while (1)
    {
        GPIO_LEVEL level_in = GPIO_LEVEL_LOW;
        if (GpioLevelGet(26, &level_in) && level_in != level)
        {
            level = level_in;
            HouseSwitchHandle(level_in);
        }
        usleep(1000);
    }
    return NULL;
}
/******************************** 内核报错缓存区满报错测试并重启 *****************************************/
// kernel_errno_text
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>  // 用于杀死klogd/syslogd
#define LOG_FILE "/proc/kmsg"
#define BUFFER_SIZE 2048
static volatile int g_stop_monitor = 0;

// 崩溃关键词（包含你日志中的空指针错误）
static const char *crash_keywords[] = {
    "Unable to handle kernel",
    "Internal error: Oops",
    "Kernel panic",
    "Out of memory:",
    "Null pointer dereference",
    NULL
};

// 停止监控的接口
void stop_kernel_log_monitor(void) {
    g_stop_monitor = 1;
}

// 关键：停止抢占日志的进程（klogd/syslogd）
static void stop_log_daemons(void) {
    // 嵌入式常见的日志守护进程名称，覆盖所有可能
    const char *daemons[] = {"klogd", "syslogd", "rsyslogd", "ulogd", NULL};
    
    printf("[INFO] Try to stop log daemons (avoid log preemption)...\n");
    for (int i = 0; daemons[i] != NULL; i++) {
        // 杀死进程（-9强制杀死，2>/dev/null忽略“无此进程”的错误）
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "killall -9 %s 2>/dev/null", daemons[i]);
        system(cmd);
        printf("[INFO] Stopped %s (if exists)\n", daemons[i]);
    }
}

static void* monitor_kernel_logs(void* arg) {
    FILE *log_file = NULL;
    char buffer[BUFFER_SIZE];

    // 第一步：停止日志守护进程，独占/proc/kmsg
    stop_log_daemons();

    // 第二步：打开内核日志（一次性打开，持续读取）
    log_file = fopen(LOG_FILE, "r");
    if (log_file == NULL) {
        fprintf(stderr, "[ERROR] Open %s failed: %s (errno: %d)\n", 
                LOG_FILE, strerror(errno), errno);
        // 重试一次（应对临时文件系统问题）
        sleep(2);
        log_file = fopen(LOG_FILE, "r");
        if (log_file == NULL) {
            fprintf(stderr, "[ERROR] Reopen %s failed, exit monitor\n", LOG_FILE);
            return NULL;
        }
    }
    printf("[INFO] Monitor started: exclusive read %s\n", LOG_FILE);
    printf("[INFO] Watching crash keywords: ");
    for (int i = 0; crash_keywords[i]; i++) {
        printf("%s, ", crash_keywords[i]);
    }
    printf("\n");

    // 第三步：持续读取日志，匹配关键词
    while (!g_stop_monitor) {
        memset(buffer, 0, BUFFER_SIZE);
        // fgets阻塞等待新日志，不会遗漏
        if (fgets(buffer, BUFFER_SIZE - 1, log_file) == NULL) {
            fprintf(stderr, "[WARN] Read %s failed: %s, retry...\n", 
                    LOG_FILE, strerror(errno));
            fclose(log_file);
            sleep(2);
            log_file = fopen(LOG_FILE, "r");
            if (log_file == NULL) {
                fprintf(stderr, "[ERROR] Reopen %s failed, exit monitor\n", LOG_FILE);
                break;
            }
            continue;
        }

        // 调试关键：打印收到的所有日志（确认是否收到测试日志）
        printf("[DEBUG] Received kernel log: %s\n", buffer);

        // 匹配崩溃关键词
        for (int i = 0; crash_keywords[i] != NULL; i++) {
            if (strstr(buffer, crash_keywords[i]) != NULL) {
                printf("[ALERT] Crash detected! Keyword: %s\n", crash_keywords[i]);
                printf("[ALERT] Log content: %s\n", buffer);
                system("echo '[ALERT] Crash detected, sync and reboot' > /dev/kmsg");
                sync();  // 同步文件系统
                sleep(1);
                system("reboot -f");  // 强制重启
                fclose(log_file);
                return NULL;
            }
        }
    }

    // 优雅退出
    fclose(log_file);
    printf("[INFO] Monitor stopped\n");
    return NULL;
}
/***********************************/
static void HardwareInitConfig(void)
{
    LightGpioInit();

#ifdef EPOLL_GPIO_ENABLE
    // EpollGpioEventInit();
#endif

#ifdef CARD_ENABLE
    DrvSwipeCardInit();
#endif

#ifdef KEYPAD_ENABLE
    DrvNumericKeypadInit();
#endif

#ifdef ADC_DETECT_ENABLE
    AkDrvAdcInit();
#endif

#ifdef LOCK_ENABLE
    // LockGpioInit();   hare

#endif
}
/****************喂狗*******************/
static void feed_dog(void) 
{
    static unsigned long last_feed_sec = 0; // 上一次喂狗的累计秒数
    struct ak_timeval timeval;
    ak_get_ostime(&timeval);
    
    if (timeval.sec - last_feed_sec >= 1) { // 每隔1秒喂一次
        watch_dog_feed();
        last_feed_sec = timeval.sec;
    }
}
/***********************************/
int main(int argc, char *argv[])
{
    Debug("\n\n###########################[%s]########################\n", IPC_MODEL);
    Debug("# Compile Time:%s-%s\n", __DATE__, __TIME__);
    Debug("####################################################################\n\n");

    
    AKPlatformSdkInit();

    systemtick_init();   //leo

    HardwareInitConfig();

    /**********door切换检测**************/
    if (GpioOpen(26, GPIO_DIR_IN, true) == false)
    {
        return -1;
    }
    GpioLevelGet(26, &level);
    
    NetworkDevice DevId = level == GPIO_LEVEL_LOW ? DEVICE_OUTDOOR_1 : DEVICE_OUTDOOR_2;
    VideoNetSocketProtocolSet(DevId);
    AudioNetSocketProtocolSet(DevId);
    NetLocalDeviceIDSet(DevId);
    /************************************/

    UserConfigInit();

    #ifdef FINGERPRINT_ENABLE
    FingerprintInit();    //指纹
    #endif

    UserDeckInit();

#ifdef VIDEO_TRANSFER_ENABLE
    NetVideoTransferInit();
#endif

#ifdef AUDIO_TRANSFER_ENABLE
    NetAudioTransferInit();
#endif

#ifdef NETMSG_ENABLE
    NetMsgCommInit();
    LockGpioInit();  //hare 2025.7.4
#endif

#ifdef VOICE_AO_ENABLE
#ifndef ATS_ENABLE
    VoiceRingPlayInit();
#else
    ak_ats_start(8012);
#endif
#endif

#ifdef USER_MANAGEMENT
    UserNetManageInit();
#endif

    // SystemEventLoop();   2025.8.23 hare set
    InfraredDetectInit_main();

    pthread_t Thread;
    pthread_create(&Thread, NULL, house_and_systemtick, NULL);
    pthread_detach(Thread);

    pthread_t monitor_thread;
    int ret;

    // 启动内核崩溃监控线程
    ret = pthread_create(&monitor_thread, NULL, monitor_kernel_logs, NULL);
    pthread_detach(monitor_thread);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] Create monitor thread failed: %s\n", strerror(ret));
        return -1;
    } 

    watchdog_open(); // 喂狗程序初始化

    unsigned long timestamp = os_get_second();
	bool waiting_for_reboot  = false;
	struct ak_timeval start_time, current_time ;
    while (1)
    {
        usleep(1000*1000);
        reset_system_timestamp();
        feed_dog(); // 喂狗

        /********************************12小时检测重启机制*******************************************/
        // printf("82225-duration =               %lu \n",os_get_second() - timestamp);
        if ((os_get_second() - timestamp > 43200  ) && (is_network_video_send_package_open()==false) && (!TimerEnablestatus(AddCardTimer)) && (!TimerEnablestatus(AddFingerTimer)))  //43200
		{
			if (!waiting_for_reboot) {
				ak_get_ostime(&start_time);
                waiting_for_reboot = true;
                printf("检测各条件满足,将在10秒后重启...\n");

			}
			else {
                // 已在等待中，检查是否已过10秒
                ak_get_ostime(&current_time);
                
                // 计算已等待的秒数
                unsigned long elapsed_sec = current_time.sec - start_time.sec;

                // 总等待时间是否达到10秒
                if (elapsed_sec > 10) {
					sync(); 
					sleep(1);
					system("reboot -f");  // 强制重启
                    break;
                }
			}
		}else{
			// 状态变为false，取消等待
            if (waiting_for_reboot) {
                waiting_for_reboot = false;
                printf("状态已不满足，取消重启,重新计时!\n");
            }
		}
		/***************************************************************************/
    }

    return 0;
}