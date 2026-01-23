/**
 * @file test_event_manager.c
 * @brief 事件管理器单元测试
 * 
 * 简单的测试框架，用于验证事件管理器的核心功能。
 * 
 * 编译: gcc -o test_event_manager test_event_manager.c ../src/event_manager.c -I../include -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "event_manager.h"

/*============================================================================
 *                              测试框架
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    do { \
        printf("测试: %s ... ", name); \
        tests_run++; \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("通过\n"); \
        tests_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        printf("失败: %s\n", msg); \
        tests_failed++; \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) ASSERT_TRUE((a) == (b), msg)
#define ASSERT_NE(a, b, msg) ASSERT_TRUE((a) != (b), msg)
#define ASSERT_NULL(ptr, msg) ASSERT_TRUE((ptr) == NULL, msg)
#define ASSERT_NOT_NULL(ptr, msg) ASSERT_TRUE((ptr) != NULL, msg)

/*============================================================================
 *                              测试辅助
 *============================================================================*/

static int callback_counter = 0;
static int last_event_id = -1;
static void* last_data = NULL;
static void* last_user_data = NULL;
static int last_data_value = 0;  /* 用于存储数据值（用于数据复制测试） */

static void reset_counters(void)
{
    callback_counter = 0;
    last_event_id = -1;
    last_data = NULL;
    last_user_data = NULL;
    last_data_value = 0;
}

static void test_callback(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    callback_counter++;
    last_event_id = event_id;
    last_data = data;
    last_user_data = user_data;
    /* 如果data是int指针，存储其值 */
    if (data != NULL) {
        last_data_value = *(int*)data;
    }
}

static void test_callback2(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    callback_counter += 10;  /* 不同的增量以区分 */
}

static void test_callback3(em_event_id_t event_id, em_event_data_t data, void* user_data)
{
    (void)event_id;
    (void)data;
    (void)user_data;
    callback_counter += 100;  /* 不同的增量以区分 */
}

/*============================================================================
 *                              创建/销毁测试
 *============================================================================*/

void test_create_destroy(void)
{
    TEST_START("创建和销毁事件管理器");
    
    em_handle_t em = em_create();
    ASSERT_NOT_NULL(em, "em_create 返回 NULL");
    
    em_error_t err = em_destroy(em);
    ASSERT_EQ(err, EM_OK, "em_destroy 失败");
    
    TEST_PASS();
}

void test_destroy_null(void)
{
    TEST_START("销毁 NULL 句柄");
    
    em_error_t err = em_destroy(NULL);
    ASSERT_EQ(err, EM_ERR_INVALID_PARAM, "应返回 EM_ERR_INVALID_PARAM");
    
    TEST_PASS();
}

/*============================================================================
 *                              订阅测试
 *============================================================================*/

void test_subscribe_basic(void)
{
    TEST_START("基本订阅");
    
    em_handle_t em = em_create();
    ASSERT_NOT_NULL(em, "创建失败");
    
    em_error_t err = em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_OK, "订阅失败");
    
    int count = em_get_subscriber_count(em, 0);
    ASSERT_EQ(count, 1, "订阅者数量不正确");
    
    em_destroy(em);
    TEST_PASS();
}

void test_subscribe_with_user_data(void)
{
    TEST_START("带用户数据订阅");
    
    em_handle_t em = em_create();
    int user_value = 42;
    
    em_subscribe(em, 0, test_callback, &user_value, EM_PRIORITY_NORMAL);
    
    reset_counters();
    em_publish_sync(em, 0, NULL);
    
    ASSERT_EQ(last_user_data, &user_value, "用户数据不匹配");
    
    em_destroy(em);
    TEST_PASS();
}

void test_subscribe_multiple(void)
{
    TEST_START("多个订阅者");
    
    em_handle_t em = em_create();
    
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 0, test_callback2, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 0, test_callback3, NULL, EM_PRIORITY_NORMAL);
    
    ASSERT_EQ(em_get_subscriber_count(em, 0), 3, "订阅者数量不正确");
    
    reset_counters();
    em_publish_sync(em, 0, NULL);
    
    ASSERT_EQ(callback_counter, 111, "不是所有回调都执行了");
    
    em_destroy(em);
    TEST_PASS();
}

void test_subscribe_duplicate(void)
{
    TEST_START("重复订阅");
    
    em_handle_t em = em_create();
    
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);  /* 重复 */
    
    /* 重复订阅应该被忽略 */
    ASSERT_EQ(em_get_subscriber_count(em, 0), 1, "重复订阅未被忽略");
    
    em_destroy(em);
    TEST_PASS();
}

void test_subscribe_invalid_params(void)
{
    TEST_START("无效参数订阅");
    
    em_handle_t em = em_create();
    em_error_t err;
    
    /* NULL 句柄 */
    err = em_subscribe(NULL, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_ERR_INVALID_PARAM, "应拒绝 NULL 句柄");
    
    /* NULL 回调 */
    err = em_subscribe(em, 0, NULL, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_ERR_INVALID_PARAM, "应拒绝 NULL 回调");
    
    /* 无效事件 ID */
    err = em_subscribe(em, EM_MAX_EVENT_TYPES + 1, test_callback, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_ERR_INVALID_PARAM, "应拒绝无效事件 ID");
    
    /* 无效优先级 */
    err = em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_COUNT + 1);
    ASSERT_EQ(err, EM_ERR_INVALID_PARAM, "应拒绝无效优先级");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              取消订阅测试
 *============================================================================*/

void test_unsubscribe_basic(void)
{
    TEST_START("基本取消订阅");
    
    em_handle_t em = em_create();
    
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(em_get_subscriber_count(em, 0), 1, "订阅失败");
    
    em_error_t err = em_unsubscribe(em, 0, test_callback);
    ASSERT_EQ(err, EM_OK, "取消订阅失败");
    ASSERT_EQ(em_get_subscriber_count(em, 0), 0, "订阅者未被移除");
    
    em_destroy(em);
    TEST_PASS();
}

void test_unsubscribe_not_found(void)
{
    TEST_START("取消不存在的订阅");
    
    em_handle_t em = em_create();
    
    em_error_t err = em_unsubscribe(em, 0, test_callback);
    ASSERT_EQ(err, EM_ERR_NOT_FOUND, "应返回 NOT_FOUND");
    
    em_destroy(em);
    TEST_PASS();
}

void test_unsubscribe_all(void)
{
    TEST_START("取消所有订阅");
    
    em_handle_t em = em_create();
    
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 0, test_callback2, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 0, test_callback3, NULL, EM_PRIORITY_NORMAL);
    ASSERT_EQ(em_get_subscriber_count(em, 0), 3, "订阅失败");
    
    em_error_t err = em_unsubscribe_all(em, 0);
    ASSERT_EQ(err, EM_OK, "取消所有订阅失败");
    ASSERT_EQ(em_get_subscriber_count(em, 0), 0, "订阅者未被全部移除");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              同步事件测试
 *============================================================================*/

void test_publish_sync_basic(void)
{
    TEST_START("基本同步发布");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    reset_counters();
    
    int data = 12345;
    em_error_t err = em_publish_sync(em, 0, &data);
    
    ASSERT_EQ(err, EM_OK, "发布失败");
    ASSERT_EQ(callback_counter, 1, "回调未执行");
    ASSERT_EQ(last_event_id, 0, "事件 ID 不匹配");
    ASSERT_EQ(last_data, &data, "数据不匹配");
    
    em_destroy(em);
    TEST_PASS();
}

void test_publish_sync_no_subscribers(void)
{
    TEST_START("无订阅者同步发布");
    
    em_handle_t em = em_create();
    reset_counters();
    
    em_error_t err = em_publish_sync(em, 0, NULL);
    
    ASSERT_EQ(err, EM_OK, "发布应成功");
    ASSERT_EQ(callback_counter, 0, "不应有回调执行");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              异步事件测试
 *============================================================================*/

void test_publish_async_basic(void)
{
    TEST_START("基本异步发布");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    reset_counters();
    
    em_error_t err = em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_OK, "异步发布失败");
    
    /* 发布后应该还没执行 */
    ASSERT_EQ(callback_counter, 0, "异步事件不应立即执行");
    ASSERT_EQ(em_get_queue_size(em), 1, "队列大小不正确");
    
    /* 处理事件 */
    err = em_process_one(em);
    ASSERT_EQ(err, EM_OK, "处理事件失败");
    ASSERT_EQ(callback_counter, 1, "回调未执行");
    ASSERT_EQ(em_get_queue_size(em), 0, "队列应为空");
    
    em_destroy(em);
    TEST_PASS();
}

void test_publish_async_with_data_copy(void)
{
    TEST_START("异步发布带数据复制");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    reset_counters();
    
    int data = 42;
    em_error_t err = em_publish_async(em, 0, &data, sizeof(int), EM_PRIORITY_NORMAL);
    ASSERT_EQ(err, EM_OK, "异步发布失败");
    
    /* 修改原始数据 */
    data = 999;
    
    /* 处理事件 */
    em_process_one(em);
    
    /* 验证收到的是复制的数据(值为42) - 使用在回调中保存的值 */
    ASSERT_EQ(last_data_value, 42, "数据复制不正确");
    
    em_destroy(em);
    TEST_PASS();
}

void test_process_all(void)
{
    TEST_START("处理所有异步事件");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    reset_counters();
    
    /* 发布多个事件 */
    for (int i = 0; i < 5; i++) {
        em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    }
    
    ASSERT_EQ(em_get_queue_size(em), 5, "队列大小不正确");
    
    int processed = em_process_all(em);
    ASSERT_EQ(processed, 5, "处理数量不正确");
    ASSERT_EQ(callback_counter, 5, "回调执行次数不正确");
    ASSERT_EQ(em_get_queue_size(em), 0, "队列应为空");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              优先级测试
 *============================================================================*/

static int priority_order[3];
static int priority_index = 0;

static void high_priority_handler(em_event_id_t id, em_event_data_t data, void* user)
{
    (void)id; (void)data; (void)user;
    priority_order[priority_index++] = 0;
}

static void normal_priority_handler(em_event_id_t id, em_event_data_t data, void* user)
{
    (void)id; (void)data; (void)user;
    priority_order[priority_index++] = 1;
}

static void low_priority_handler(em_event_id_t id, em_event_data_t data, void* user)
{
    (void)id; (void)data; (void)user;
    priority_order[priority_index++] = 2;
}

void test_subscriber_priority(void)
{
    TEST_START("订阅者优先级");
    
    em_handle_t em = em_create();
    
    /* 以相反顺序订阅 */
    em_subscribe(em, 0, low_priority_handler, NULL, EM_PRIORITY_LOW);
    em_subscribe(em, 0, high_priority_handler, NULL, EM_PRIORITY_HIGH);
    em_subscribe(em, 0, normal_priority_handler, NULL, EM_PRIORITY_NORMAL);
    
    priority_index = 0;
    em_publish_sync(em, 0, NULL);
    
    /* 验证执行顺序: HIGH(0) -> NORMAL(1) -> LOW(2) */
    ASSERT_EQ(priority_order[0], 0, "高优先级应最先执行");
    ASSERT_EQ(priority_order[1], 1, "普通优先级应第二执行");
    ASSERT_EQ(priority_order[2], 2, "低优先级应最后执行");
    
    em_destroy(em);
    TEST_PASS();
}

void test_event_priority(void)
{
    TEST_START("事件优先级");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 1, test_callback, NULL, EM_PRIORITY_NORMAL);
    em_subscribe(em, 2, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    reset_counters();
    
    /* 以相反顺序发布不同优先级的事件 */
    em_publish_async(em, 2, NULL, 0, EM_PRIORITY_LOW);     /* 事件2, 低优先级 */
    em_publish_async(em, 1, NULL, 0, EM_PRIORITY_NORMAL);  /* 事件1, 普通优先级 */
    em_publish_async(em, 0, NULL, 0, EM_PRIORITY_HIGH);    /* 事件0, 高优先级 */
    
    /* 处理第一个(应该是高优先级的事件0) */
    em_process_one(em);
    ASSERT_EQ(last_event_id, 0, "高优先级事件应最先处理");
    
    /* 处理第二个(应该是普通优先级的事件1) */
    em_process_one(em);
    ASSERT_EQ(last_event_id, 1, "普通优先级事件应第二处理");
    
    /* 处理第三个(应该是低优先级的事件2) */
    em_process_one(em);
    ASSERT_EQ(last_event_id, 2, "低优先级事件应最后处理");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              队列管理测试
 *============================================================================*/

void test_clear_queue(void)
{
    TEST_START("清空队列");
    
    em_handle_t em = em_create();
    
    for (int i = 0; i < 10; i++) {
        em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    }
    ASSERT_EQ(em_get_queue_size(em), 10, "队列大小不正确");
    
    em_error_t err = em_clear_queue(em);
    ASSERT_EQ(err, EM_OK, "清空失败");
    ASSERT_EQ(em_get_queue_size(em), 0, "队列未清空");
    
    em_destroy(em);
    TEST_PASS();
}

void test_queue_empty(void)
{
    TEST_START("空队列处理");
    
    em_handle_t em = em_create();
    
    em_error_t err = em_process_one(em);
    ASSERT_EQ(err, EM_ERR_QUEUE_EMPTY, "应返回队列为空");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              统计测试
 *============================================================================*/

void test_statistics(void)
{
    TEST_START("统计信息");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    /* 发布一些事件 */
    em_publish_sync(em, 0, NULL);
    em_publish_sync(em, 0, NULL);
    em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    em_publish_async(em, 0, NULL, 0, EM_PRIORITY_NORMAL);
    
    em_process_all(em);
    
    em_stats_t stats;
    em_error_t err = em_get_stats(em, &stats);
    ASSERT_EQ(err, EM_OK, "获取统计失败");
    
    ASSERT_EQ(stats.events_published, 5, "发布数不正确");
    ASSERT_EQ(stats.events_processed, 5, "处理数不正确");
    ASSERT_EQ(stats.subscribers_total, 1, "订阅者数不正确");
    
    em_destroy(em);
    TEST_PASS();
}

void test_reset_statistics(void)
{
    TEST_START("重置统计信息");
    
    em_handle_t em = em_create();
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    
    em_publish_sync(em, 0, NULL);
    em_publish_sync(em, 0, NULL);
    
    em_reset_stats(em);
    
    em_stats_t stats;
    em_get_stats(em, &stats);
    
    ASSERT_EQ(stats.events_published, 0, "发布数应为0");
    ASSERT_EQ(stats.events_processed, 0, "处理数应为0");
    ASSERT_EQ(stats.subscribers_total, 1, "订阅者数应保留");
    
    em_destroy(em);
    TEST_PASS();
}

/*============================================================================
 *                              工具函数测试
 *============================================================================*/

void test_has_subscribers(void)
{
    TEST_START("检查是否有订阅者");
    
    em_handle_t em = em_create();
    
    ASSERT_TRUE(!em_has_subscribers(em, 0), "应无订阅者");
    
    em_subscribe(em, 0, test_callback, NULL, EM_PRIORITY_NORMAL);
    ASSERT_TRUE(em_has_subscribers(em, 0), "应有订阅者");
    
    em_unsubscribe(em, 0, test_callback);
    ASSERT_TRUE(!em_has_subscribers(em, 0), "应无订阅者");
    
    em_destroy(em);
    TEST_PASS();
}

void test_error_string(void)
{
    TEST_START("错误码字符串");
    
    const char* str = em_error_string(EM_OK);
    ASSERT_NOT_NULL(str, "返回空字符串");
    ASSERT_TRUE(strlen(str) > 0, "字符串为空");
    
    str = em_error_string(EM_ERR_INVALID_PARAM);
    ASSERT_NOT_NULL(str, "返回空字符串");
    
    str = em_error_string((em_error_t)999);  /* 未知错误码 */
    ASSERT_NOT_NULL(str, "返回空字符串");
    
    TEST_PASS();
}

void test_version(void)
{
    TEST_START("版本信息");
    
    const char* ver = em_version();
    ASSERT_NOT_NULL(ver, "版本为空");
    ASSERT_TRUE(strlen(ver) > 0, "版本字符串为空");
    
    TEST_PASS();
}

/*============================================================================
 *                              主函数
 *============================================================================*/

int main(void)
{
    printf("=== 事件管理器单元测试 ===\n");
    printf("版本: %s\n\n", em_version());
    
    /* 创建/销毁 */
    test_create_destroy();
    test_destroy_null();
    
    /* 订阅 */
    test_subscribe_basic();
    test_subscribe_with_user_data();
    test_subscribe_multiple();
    test_subscribe_duplicate();
    test_subscribe_invalid_params();
    
    /* 取消订阅 */
    test_unsubscribe_basic();
    test_unsubscribe_not_found();
    test_unsubscribe_all();
    
    /* 同步事件 */
    test_publish_sync_basic();
    test_publish_sync_no_subscribers();
    
    /* 异步事件 */
    test_publish_async_basic();
    test_publish_async_with_data_copy();
    test_process_all();
    
    /* 优先级 */
    test_subscriber_priority();
    test_event_priority();
    
    /* 队列管理 */
    test_clear_queue();
    test_queue_empty();
    
    /* 统计 */
    test_statistics();
    test_reset_statistics();
    
    /* 工具函数 */
    test_has_subscribers();
    test_error_string();
    test_version();
    
    /* 结果汇总 */
    printf("\n=== 测试结果 ===\n");
    printf("运行: %d\n", tests_run);
    printf("通过: %d\n", tests_passed);
    printf("失败: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n所有测试通过!\n");
        return 0;
    } else {
        printf("\n有测试失败!\n");
        return 1;
    }
}
