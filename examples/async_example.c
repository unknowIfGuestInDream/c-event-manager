/**
 * @file async_example.c
 * @brief 同步与异步事件示例
 * 
 * 本示例展示事件管理器的同步和异步处理模式：
 * - 同步事件: 立即执行，阻塞发布者
 * - 异步事件: 放入队列，稍后处理
 * - 数据复制: 异步事件的数据安全
 * 
 * 编译: gcc -o async_example async_example.c ../src/event_manager.c -I../include -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event_manager.h"

/* 定义事件类型 */
enum {
    EVENT_SYNC_MSG = 0,
    EVENT_ASYNC_MSG = 1,
    EVENT_SENSOR_DATA = 2
};

/*============================================================================
 *                          数据结构定义
 *============================================================================*/

typedef struct {
    int sensor_id;
    float temperature;
    float humidity;
    char timestamp[20];
} sensor_data_t;

/*============================================================================
 *                          事件处理函数
 *============================================================================*/

void on_sync_message(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    printf("[同步] 收到消息: %s\n", (char*)data);
}

void on_async_message(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    printf("[异步] 收到消息: %s\n", (char*)data);
}

void on_sensor_data(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    sensor_data_t* sensor = (sensor_data_t*)data;
    printf("[传感器] ID=%d, 温度=%.1f°C, 湿度=%.1f%%, 时间=%s\n",
           sensor->sensor_id, sensor->temperature, 
           sensor->humidity, sensor->timestamp);
}

/*============================================================================
 *                          同步 vs 异步演示
 *============================================================================*/

void demo_sync_async_diff(em_handle_t em)
{
    printf("\n>>> 演示1: 同步与异步的区别\n");
    printf("----------------------------------------\n");
    
    em_subscribe(em, EVENT_SYNC_MSG, on_sync_message, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, EVENT_ASYNC_MSG, on_async_message, NULL, EM_PRIORITY_NORMAL);
    
    printf("\n1. 发布同步事件(立即执行):\n");
    printf("   发布前...\n");
    em_publish_sync(em, EVENT_SYNC_MSG, "同步消息1");
    printf("   发布后(已执行完毕)\n");
    
    printf("\n2. 发布异步事件(放入队列):\n");
    printf("   发布前...\n");
    em_publish_async(em, EVENT_ASYNC_MSG, "异步消息1", 0, EM_PRIORITY_NORMAL);
    em_publish_async(em, EVENT_ASYNC_MSG, "异步消息2", 0, EM_PRIORITY_NORMAL);
    em_publish_async(em, EVENT_ASYNC_MSG, "异步消息3", 0, EM_PRIORITY_NORMAL);
    printf("   发布后(尚未执行)\n");
    printf("   队列中有 %d 个事件\n", em_get_queue_size(em));
    
    printf("\n3. 处理异步队列:\n");
    int count = em_process_all(em);
    printf("   共处理了 %d 个异步事件\n", count);
    
    em_unsubscribe_all(em, EVENT_SYNC_MSG);
    em_unsubscribe_all(em, EVENT_ASYNC_MSG);
}

/*============================================================================
 *                          数据复制演示
 *============================================================================*/

void demo_data_copy(em_handle_t em)
{
    printf("\n>>> 演示2: 异步事件的数据复制\n");
    printf("----------------------------------------\n");
    printf("说明: 异步事件可以复制数据，确保原始数据\n");
    printf("      被修改后事件仍能正确处理\n\n");
    
    em_subscribe(em, EVENT_SENSOR_DATA, on_sensor_data, NULL, EM_PRIORITY_NORMAL);
    
    /* 创建传感器数据 */
    sensor_data_t data = {
        .sensor_id = 1,
        .temperature = 25.5f,
        .humidity = 60.0f,
        .timestamp = "2024-01-15 10:30"
    };
    
    printf("1. 发布异步事件(带数据复制):\n");
    printf("   原始数据: ID=%d, 温度=%.1f\n", data.sensor_id, data.temperature);
    
    /* 发布时复制数据 */
    em_publish_async(em, EVENT_SENSOR_DATA, &data, sizeof(sensor_data_t), EM_PRIORITY_NORMAL);
    
    printf("\n2. 修改原始数据:\n");
    data.sensor_id = 999;
    data.temperature = 99.9f;
    strcpy(data.timestamp, "MODIFIED!");
    printf("   修改后: ID=%d, 温度=%.1f\n", data.sensor_id, data.temperature);
    
    printf("\n3. 处理事件(使用复制的数据):\n");
    em_process_all(em);
    printf("   (可以看到处理的是原始数据，不是修改后的)\n");
    
    em_unsubscribe_all(em, EVENT_SENSOR_DATA);
}

/*============================================================================
 *                          主循环模式演示
 *============================================================================*/

static int event_count = 0;
static const int MAX_EVENTS = 5;

void on_periodic_event(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    int* value = (int*)data;
    printf("  处理周期事件 #%d\n", *value);
    event_count++;
}

void demo_main_loop(em_handle_t em)
{
    printf("\n>>> 演示3: 主循环处理模式\n");
    printf("----------------------------------------\n");
    printf("说明: 在主循环中调用 em_process_one 或\n");
    printf("      em_process_all 来处理异步事件\n\n");
    
    #define EVENT_PERIODIC 10
    
    em_subscribe(em, EVENT_PERIODIC, on_periodic_event, NULL, EM_PRIORITY_NORMAL);
    
    /* 发布一些事件 */
    printf("发布 %d 个异步事件...\n", MAX_EVENTS);
    for (int i = 1; i <= MAX_EVENTS; i++) {
        /* 注意: 这里不复制数据，因为我们立即处理 */
        int value = i;
        em_publish_async(em, EVENT_PERIODIC, &value, sizeof(int), EM_PRIORITY_NORMAL);
    }
    
    printf("队列大小: %d\n\n", em_get_queue_size(em));
    
    printf("模拟主循环:\n");
    int loop_count = 0;
    event_count = 0;
    
    while (event_count < MAX_EVENTS && loop_count < 100) {
        /* 主循环中的其他工作... */
        
        /* 处理一个事件 */
        em_error_t err = em_process_one(em);
        if (err == EM_ERR_QUEUE_EMPTY) {
            /* 没有事件，做其他事情 */
        }
        
        loop_count++;
    }
    
    printf("\n循环次数: %d, 处理事件数: %d\n", loop_count, event_count);
    
    em_unsubscribe_all(em, EVENT_PERIODIC);
}

/*============================================================================
 *                          队列管理演示
 *============================================================================*/

void demo_queue_management(em_handle_t em)
{
    printf("\n>>> 演示4: 队列管理\n");
    printf("----------------------------------------\n");
    
    #define EVENT_QUEUE_TEST 20
    
    /* 发布一些事件 */
    printf("1. 发布 10 个异步事件...\n");
    for (int i = 0; i < 10; i++) {
        em_publish_async(em, EVENT_QUEUE_TEST, NULL, 0, EM_PRIORITY_NORMAL);
    }
    printf("   队列大小: %d\n", em_get_queue_size(em));
    
    printf("\n2. 处理 3 个事件...\n");
    for (int i = 0; i < 3; i++) {
        em_process_one(em);
    }
    printf("   队列大小: %d\n", em_get_queue_size(em));
    
    printf("\n3. 清空队列...\n");
    em_clear_queue(em);
    printf("   队列大小: %d\n", em_get_queue_size(em));
    
    printf("\n4. 查看统计信息:\n");
    em_stats_t stats;
    em_get_stats(em, &stats);
    printf("   已发布事件: %u\n", stats.events_published);
    printf("   已处理事件: %u\n", stats.events_processed);
    printf("   队列峰值: %u\n", stats.async_queue_max);
}

int main(void)
{
    printf("=== 同步与异步事件示例 ===\n");
    printf("版本: %s\n", em_version());
    
    /* 创建事件管理器 */
    em_handle_t em = em_create();
    if (em == NULL) {
        fprintf(stderr, "错误: 无法创建事件管理器\n");
        return 1;
    }
    
    /* 运行各个演示 */
    demo_sync_async_diff(em);
    demo_data_copy(em);
    demo_main_loop(em);
    demo_queue_management(em);
    
    /* 清理 */
    em_destroy(em);
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}
