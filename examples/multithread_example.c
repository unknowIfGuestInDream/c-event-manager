/**
 * @file multithread_example.c
 * @brief 多线程事件处理示例
 * 
 * 本示例展示事件管理器在多线程环境下的使用：
 * - 多个线程同时发布事件
 * - 独立的事件处理线程
 * - 线程安全的订阅/取消订阅
 * 
 * 编译: gcc -o multithread_example multithread_example.c ../src/event_manager.c -I../include -lpthread
 * 
 * 注意: 需要启用 EM_ENABLE_THREADING (默认已启用)
 */

#define _DEFAULT_SOURCE  /* for usleep */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "event_manager.h"

/* 定义事件类型 */
enum {
    EVENT_SENSOR_1 = 0,
    EVENT_SENSOR_2 = 1,
    EVENT_SENSOR_3 = 2,
    EVENT_STOP = 3
};

/* 全局事件管理器 */
static em_handle_t g_em = NULL;

/* 统计 */
static volatile int g_events_received = 0;
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/*============================================================================
 *                          事件处理函数
 *============================================================================*/

void on_sensor_event(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    const char* sensor_name = (const char*)user_data;
    int* value = (int*)data;
    
    pthread_mutex_lock(&g_stats_mutex);
    g_events_received++;
    int count = g_events_received;
    pthread_mutex_unlock(&g_stats_mutex);
    
    printf("[%s] 传感器%u 数据: %d (累计接收: %d)\n", 
           sensor_name, event_id, value ? *value : 0, count);
}

void on_stop_event(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    printf("[系统] 收到停止信号\n");
}

/*============================================================================
 *                          线程函数
 *============================================================================*/

/**
 * @brief 传感器数据生产者线程
 */
typedef struct {
    int sensor_id;
    em_event_id_t event_id;
    int interval_ms;
    int count;
} producer_args_t;

void* producer_thread(void* arg)
{
    producer_args_t* args = (producer_args_t*)arg;
    
    printf("[生产者%d] 启动，间隔=%dms，总数=%d\n", 
           args->sensor_id, args->interval_ms, args->count);
    
    for (int i = 0; i < args->count; i++) {
        int value = args->sensor_id * 1000 + i;
        
        /* 发布异步事件(带数据复制) */
        em_error_t err = em_publish_async(g_em, args->event_id, 
                                          &value, sizeof(int), 
                                          EM_PRIORITY_NORMAL);
        
        if (err != EM_OK) {
            printf("[生产者%d] 发布失败: %s\n", args->sensor_id, em_error_string(err));
        }
        
        usleep(args->interval_ms * 1000);
    }
    
    printf("[生产者%d] 完成\n", args->sensor_id);
    return NULL;
}

/**
 * @brief 事件处理消费者线程
 */
void* consumer_thread(void* arg)
{
    (void)arg;
    printf("[消费者] 启动\n");
    
    while (1) {
        /* 处理一个事件 */
        em_error_t err = em_process_one(g_em);
        
        if (err == EM_ERR_QUEUE_EMPTY) {
            /* 队列为空，短暂休眠后重试 */
            usleep(1000);  /* 1ms */
        }
        
        /* 检查是否应该停止(通过检查统计来判断) */
        pthread_mutex_lock(&g_stats_mutex);
        int received = g_events_received;
        pthread_mutex_unlock(&g_stats_mutex);
        
        /* 当接收到足够多事件后退出 */
        if (received >= 30) {  /* 3个生产者各10个事件 */
            break;
        }
    }
    
    printf("[消费者] 完成\n");
    return NULL;
}

/*============================================================================
 *                          演示函数
 *============================================================================*/

void demo_multithread(void)
{
    printf("\n>>> 演示: 多线程事件处理\n");
    printf("----------------------------------------\n");
    printf("场景: 3个生产者线程发布事件，1个消费者线程处理\n\n");
    
    /* 创建事件管理器 */
    g_em = em_create();
    if (g_em == NULL) {
        fprintf(stderr, "错误: 无法创建事件管理器\n");
        return;
    }
    
    /* 订阅事件 */
    em_subscribe(g_em, EVENT_SENSOR_1, on_sensor_event, "传感器1处理器", EM_PRIORITY_NORMAL);
    em_subscribe(g_em, EVENT_SENSOR_2, on_sensor_event, "传感器2处理器", EM_PRIORITY_NORMAL);
    em_subscribe(g_em, EVENT_SENSOR_3, on_sensor_event, "传感器3处理器", EM_PRIORITY_NORMAL);
    
    /* 准备线程参数 */
    producer_args_t args1 = {1, EVENT_SENSOR_1, 100, 10};
    producer_args_t args2 = {2, EVENT_SENSOR_2, 150, 10};
    producer_args_t args3 = {3, EVENT_SENSOR_3, 200, 10};
    
    /* 创建线程 */
    pthread_t producer1, producer2, producer3, consumer;
    
    printf("启动线程...\n\n");
    
    pthread_create(&consumer, NULL, consumer_thread, NULL);
    pthread_create(&producer1, NULL, producer_thread, &args1);
    pthread_create(&producer2, NULL, producer_thread, &args2);
    pthread_create(&producer3, NULL, producer_thread, &args3);
    
    /* 等待线程完成 */
    pthread_join(producer1, NULL);
    pthread_join(producer2, NULL);
    pthread_join(producer3, NULL);
    pthread_join(consumer, NULL);
    
    printf("\n所有线程已完成\n");
    
    /* 显示统计 */
    em_stats_t stats;
    em_get_stats(g_em, &stats);
    printf("\n统计信息:\n");
    printf("  已发布事件: %u\n", stats.events_published);
    printf("  已处理事件: %u\n", stats.events_processed);
    printf("  队列峰值: %u\n", stats.async_queue_max);
    
    /* 清理 */
    em_destroy(g_em);
    g_em = NULL;
}

/*============================================================================
 *                          使用事件循环的演示
 *============================================================================*/

static volatile int g_loop_running = 1;
static volatile int g_loop_events = 0;

void on_loop_event(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    int* value = (int*)data;
    printf("[事件循环] 处理事件: %d\n", value ? *value : 0);
    g_loop_events++;
    
    /* 处理完10个事件后停止 */
    if (g_loop_events >= 10) {
        printf("[事件循环] 达到目标事件数，请求停止\n");
        em_stop_loop(g_em);
    }
}

void* loop_producer_thread(void* arg)
{
    (void)arg;
    printf("[循环生产者] 启动\n");
    
    for (int i = 1; i <= 10; i++) {
        usleep(100000);  /* 100ms */
        
        int value = i;
        em_publish_async(g_em, 0, &value, sizeof(int), EM_PRIORITY_NORMAL);
        printf("[循环生产者] 发布事件 %d\n", i);
    }
    
    printf("[循环生产者] 完成\n");
    return NULL;
}

void demo_event_loop(void)
{
    printf("\n>>> 演示: 使用事件循环\n");
    printf("----------------------------------------\n");
    printf("场景: 使用 em_run_loop 阻塞式处理事件\n\n");
    
    /* 重置计数器 */
    g_loop_events = 0;
    g_loop_running = 1;
    
    /* 创建事件管理器 */
    g_em = em_create();
    if (g_em == NULL) {
        fprintf(stderr, "错误: 无法创建事件管理器\n");
        return;
    }
    
    em_subscribe(g_em, 0, on_loop_event, NULL, EM_PRIORITY_NORMAL);
    
    /* 启动生产者线程 */
    pthread_t producer;
    pthread_create(&producer, NULL, loop_producer_thread, NULL);
    
    printf("启动事件循环(将阻塞直到收到停止信号)...\n\n");
    
    /* 运行事件循环(阻塞) */
    em_run_loop(g_em);
    
    printf("\n事件循环已退出\n");
    
    /* 等待生产者完成 */
    pthread_join(producer, NULL);
    
    /* 清理 */
    em_destroy(g_em);
    g_em = NULL;
}

int main(void)
{
    printf("=== 多线程事件处理示例 ===\n");
    printf("版本: %s\n", em_version());
    
#if !EM_ENABLE_THREADING
    printf("\n警告: 多线程支持未启用!\n");
    printf("请在编译时定义 EM_ENABLE_THREADING=1\n");
    return 1;
#endif
    
    /* 运行演示 */
    demo_multithread();
    demo_event_loop();
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}
