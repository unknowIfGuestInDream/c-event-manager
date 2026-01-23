/**
 * @file priority_example.c
 * @brief 事件优先级示例
 * 
 * 本示例展示事件管理器的优先级功能：
 * - 订阅者优先级: 同一事件的不同订阅者按优先级顺序执行
 * - 事件优先级: 异步队列中不同优先级的事件按顺序处理
 * 
 * 编译: gcc -o priority_example priority_example.c ../src/event_manager.c -I../include -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include "event_manager.h"

/* 定义事件类型 */
enum {
    EVENT_TEST = 0,
    EVENT_TASK = 1
};

/*============================================================================
 *                          订阅者优先级示例
 *============================================================================*/

void handler_high(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    printf("  [高优先级处理器] 我先执行!\n");
}

void handler_normal(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    printf("  [普通优先级处理器] 我第二个执行\n");
}

void handler_low(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    printf("  [低优先级处理器] 我最后执行\n");
}

void demo_subscriber_priority(em_handle_t em)
{
    printf("\n>>> 演示1: 订阅者优先级\n");
    printf("----------------------------------------\n");
    printf("说明: 三个处理器以不同顺序订阅同一事件，\n");
    printf("      但按优先级顺序执行(高->普通->低)\n\n");
    
    /* 故意以相反顺序订阅，验证优先级排序 */
    em_subscribe(em, EVENT_TEST, handler_low, NULL, EM_PRIORITY_LOW);
    printf("订阅: 低优先级处理器 (priority=%d)\n", EM_PRIORITY_LOW);
    
    em_subscribe(em, EVENT_TEST, handler_high, NULL, EM_PRIORITY_HIGH);
    printf("订阅: 高优先级处理器 (priority=%d)\n", EM_PRIORITY_HIGH);
    
    em_subscribe(em, EVENT_TEST, handler_normal, NULL, EM_PRIORITY_NORMAL);
    printf("订阅: 普通优先级处理器 (priority=%d)\n", EM_PRIORITY_NORMAL);
    
    printf("\n发布事件 EVENT_TEST:\n");
    em_publish_sync(em, EVENT_TEST, NULL);
    
    /* 清理 */
    em_unsubscribe_all(em, EVENT_TEST);
}

/*============================================================================
 *                          事件优先级示例
 *============================================================================*/

void task_handler(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    const char* task_name = (const char*)data;
    printf("  处理任务: %s\n", task_name ? task_name : "未知任务");
}

void demo_event_priority(em_handle_t em)
{
    printf("\n>>> 演示2: 异步事件优先级\n");
    printf("----------------------------------------\n");
    printf("说明: 发布多个不同优先级的异步事件，\n");
    printf("      高优先级事件总是先被处理\n\n");
    
    em_subscribe(em, EVENT_TASK, task_handler, NULL, EM_PRIORITY_NORMAL);
    
    /* 发布多个异步事件(故意以相反顺序发布) */
    printf("发布异步事件(以相反顺序):\n");
    
    printf("  - 低优先级任务: 后台同步\n");
    em_publish_async(em, EVENT_TASK, "低优先级: 后台同步", 0, EM_PRIORITY_LOW);
    
    printf("  - 普通优先级任务: 保存数据\n");
    em_publish_async(em, EVENT_TASK, "普通优先级: 保存数据", 0, EM_PRIORITY_NORMAL);
    
    printf("  - 高优先级任务: 紧急告警\n");
    em_publish_async(em, EVENT_TASK, "高优先级: 紧急告警", 0, EM_PRIORITY_HIGH);
    
    printf("  - 高优先级任务: 关键操作\n");
    em_publish_async(em, EVENT_TASK, "高优先级: 关键操作", 0, EM_PRIORITY_HIGH);
    
    printf("  - 低优先级任务: 日志清理\n");
    em_publish_async(em, EVENT_TASK, "低优先级: 日志清理", 0, EM_PRIORITY_LOW);
    
    printf("\n队列中事件数: %d\n", em_get_queue_size(em));
    
    printf("\n处理异步事件(按优先级顺序):\n");
    int processed = em_process_all(em);
    printf("\n共处理 %d 个事件\n", processed);
    
    /* 清理 */
    em_unsubscribe_all(em, EVENT_TASK);
}

/*============================================================================
 *                          混合优先级示例
 *============================================================================*/

typedef struct {
    int id;
    const char* description;
} task_info_t;

void detailed_handler_high(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    task_info_t* task = (task_info_t*)data;
    if (task) {
        printf("  [高优先级处理] 任务#%d: %s\n", task->id, task->description);
    }
}

void detailed_handler_normal(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)user_data;
    task_info_t* task = (task_info_t*)data;
    if (task) {
        printf("  [普通优先级处理] 任务#%d: %s\n", task->id, task->description);
    }
}

void demo_mixed_priority(em_handle_t em)
{
    printf("\n>>> 演示3: 混合优先级(订阅者+事件)\n");
    printf("----------------------------------------\n");
    printf("说明: 每个事件有两个订阅者(高和普通优先级)，\n");
    printf("      同时事件本身也有不同优先级\n\n");
    
    /* 订阅两个不同优先级的处理器 */
    em_subscribe(em, EVENT_TASK, detailed_handler_normal, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, EVENT_TASK, detailed_handler_high, NULL, EM_PRIORITY_HIGH);
    
    /* 准备任务数据 */
    task_info_t tasks[] = {
        {1, "高优先级事件"},
        {2, "低优先级事件"},
        {3, "普通优先级事件"}
    };
    
    /* 发布不同优先级的事件 */
    printf("发布事件:\n");
    printf("  - 任务1: 高优先级\n");
    em_publish_async(em, EVENT_TASK, &tasks[0], 0, EM_PRIORITY_HIGH);
    
    printf("  - 任务2: 低优先级\n");
    em_publish_async(em, EVENT_TASK, &tasks[1], 0, EM_PRIORITY_LOW);
    
    printf("  - 任务3: 普通优先级\n");
    em_publish_async(em, EVENT_TASK, &tasks[2], 0, EM_PRIORITY_NORMAL);
    
    printf("\n处理顺序(事件优先级 -> 订阅者优先级):\n");
    em_process_all(em);
    
    /* 清理 */
    em_unsubscribe_all(em, EVENT_TASK);
}

int main(void)
{
    printf("=== 事件优先级示例 ===\n");
    printf("版本: %s\n", em_version());
    
    /* 创建事件管理器 */
    em_handle_t em = em_create();
    if (em == NULL) {
        fprintf(stderr, "错误: 无法创建事件管理器\n");
        return 1;
    }
    
    /* 运行各个演示 */
    demo_subscriber_priority(em);
    demo_event_priority(em);
    demo_mixed_priority(em);
    
    /* 清理 */
    em_destroy(em);
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}
