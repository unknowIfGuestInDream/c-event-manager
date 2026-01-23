/**
 * @file basic_example.c
 * @brief 事件管理器基础示例
 * 
 * 本示例展示事件管理器的基本用法：
 * - 创建事件管理器
 * - 订阅事件
 * - 发布同步事件
 * - 取消订阅
 * - 销毁事件管理器
 * 
 * 编译: gcc -o basic_example basic_example.c ../src/event_manager.c -I../include -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include "event_manager.h"

/* 定义事件类型 */
enum {
    EVENT_HELLO = 0,
    EVENT_GOODBYE = 1,
    EVENT_DATA = 2
};

/* 事件回调函数 */
void on_hello(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    const char* message = (const char*)data;
    printf("[HELLO事件] 收到消息: %s\n", message ? message : "(无数据)");
}

void on_goodbye(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    const char* name = (const char*)user_data;
    printf("[GOODBYE事件] 再见, %s!\n", name ? name : "未知用户");
}

void on_data(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    if (data != NULL) {
        int* value = (int*)data;
        printf("[DATA事件] 收到数据: %d\n", *value);
    }
}

int main(void)
{
    printf("=== 事件管理器基础示例 ===\n");
    printf("版本: %s\n\n", em_version());
    
    /* 1. 创建事件管理器 */
    printf("步骤1: 创建事件管理器...\n");
    em_handle_t em = em_create();
    if (em == NULL) {
        fprintf(stderr, "错误: 无法创建事件管理器\n");
        return 1;
    }
    printf("事件管理器创建成功!\n\n");
    
    /* 2. 订阅事件 */
    printf("步骤2: 订阅事件...\n");
    
    em_error_t err;
    
    /* 订阅HELLO事件 */
    err = em_subscribe(em, EVENT_HELLO, on_hello, NULL, EM_PRIORITY_NORMAL);
    if (err != EM_OK) {
        fprintf(stderr, "错误: %s\n", em_error_string(err));
    } else {
        printf("  - 已订阅 EVENT_HELLO\n");
    }
    
    /* 订阅GOODBYE事件，传入用户数据 */
    char* username = "小明";
    err = em_subscribe(em, EVENT_GOODBYE, on_goodbye, username, EM_PRIORITY_NORMAL);
    if (err == EM_OK) {
        printf("  - 已订阅 EVENT_GOODBYE (用户数据: %s)\n", username);
    }
    
    /* 订阅DATA事件 */
    err = em_subscribe(em, EVENT_DATA, on_data, NULL, EM_PRIORITY_NORMAL);
    if (err == EM_OK) {
        printf("  - 已订阅 EVENT_DATA\n");
    }
    
    printf("\n订阅者数量:\n");
    printf("  - EVENT_HELLO: %d\n", em_get_subscriber_count(em, EVENT_HELLO));
    printf("  - EVENT_GOODBYE: %d\n", em_get_subscriber_count(em, EVENT_GOODBYE));
    printf("  - EVENT_DATA: %d\n\n", em_get_subscriber_count(em, EVENT_DATA));
    
    /* 3. 发布同步事件 */
    printf("步骤3: 发布同步事件...\n\n");
    
    /* 发布HELLO事件(带字符串数据) */
    printf(">>> 发布 EVENT_HELLO\n");
    em_publish_sync(em, EVENT_HELLO, "你好，事件管理器!");
    
    /* 发布DATA事件(带整数数据) */
    printf("\n>>> 发布 EVENT_DATA\n");
    int value = 42;
    em_publish_sync(em, EVENT_DATA, &value);
    
    /* 发布GOODBYE事件 */
    printf("\n>>> 发布 EVENT_GOODBYE\n");
    em_publish_sync(em, EVENT_GOODBYE, NULL);
    
    /* 4. 获取统计信息 */
    printf("\n步骤4: 查看统计信息...\n");
    em_stats_t stats;
    em_get_stats(em, &stats);
    printf("  - 已发布事件数: %u\n", stats.events_published);
    printf("  - 已处理事件数: %u\n", stats.events_processed);
    printf("  - 总订阅者数: %u\n\n", stats.subscribers_total);
    
    /* 5. 取消订阅 */
    printf("步骤5: 取消订阅...\n");
    err = em_unsubscribe(em, EVENT_HELLO, on_hello);
    if (err == EM_OK) {
        printf("  - 已取消订阅 EVENT_HELLO\n");
    }
    
    /* 再次发布HELLO事件(此时没有订阅者) */
    printf("\n>>> 再次发布 EVENT_HELLO (已无订阅者)\n");
    em_publish_sync(em, EVENT_HELLO, "这条消息不会被处理");
    printf("  (如预期，没有输出)\n");
    
    /* 6. 销毁事件管理器 */
    printf("\n步骤6: 销毁事件管理器...\n");
    em_destroy(em);
    printf("事件管理器已销毁\n");
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}
