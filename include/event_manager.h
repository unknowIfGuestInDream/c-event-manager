/**
 * @file event_manager.h
 * @brief 嵌入式事件管理器头文件
 * 
 * 本事件管理器支持以下特性：
 * - 多线程安全
 * - 事件优先级
 * - 同步/异步事件处理
 * - 事件订阅/发布模式
 * 
 * @author 梦里不知身是客
 * @version 1.0.0
 * @date 2026
 * @copyright MIT License
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                              配置宏定义
 *============================================================================*/

/** 最大事件类型数量 */
#ifndef EM_MAX_EVENT_TYPES
#define EM_MAX_EVENT_TYPES      64
#endif

/** 每种事件类型最大订阅者数量 */
#ifndef EM_MAX_SUBSCRIBERS
#define EM_MAX_SUBSCRIBERS      16
#endif

/** 异步事件队列大小 */
#ifndef EM_ASYNC_QUEUE_SIZE
#define EM_ASYNC_QUEUE_SIZE     32
#endif

/** 是否启用多线程支持 (1=启用, 0=禁用) */
#ifndef EM_ENABLE_THREADING
#define EM_ENABLE_THREADING     1
#endif

/** 是否启用调试日志 (1=启用, 0=禁用) */
#ifndef EM_ENABLE_DEBUG
#define EM_ENABLE_DEBUG         0
#endif

/** 是否启用 epoll 优化 (1=启用, 0=禁用) 
 *  仅在 Linux 系统上有效，需要 EM_ENABLE_THREADING=1
 *  启用后，事件循环将使用 epoll + eventfd 替代 pthread 条件变量
 *  这可以显著提高高并发场景下的性能
 */
#ifndef EM_ENABLE_EPOLL
#ifdef __linux__
#define EM_ENABLE_EPOLL         0
#else
#define EM_ENABLE_EPOLL         0
#endif
#endif

/*============================================================================
 *                              类型定义
 *============================================================================*/

/**
 * @brief 事件优先级枚举
 * 
 * 数值越小优先级越高，HIGH优先级的事件会先被处理
 */
typedef enum {
    EM_PRIORITY_HIGH    = 0,    /**< 高优先级 - 最先处理 */
    EM_PRIORITY_NORMAL  = 1,    /**< 普通优先级 - 默认 */
    EM_PRIORITY_LOW     = 2,    /**< 低优先级 - 最后处理 */
    EM_PRIORITY_COUNT   = 3     /**< 优先级数量 */
} em_priority_t;

/**
 * @brief 事件处理模式枚举
 */
typedef enum {
    EM_MODE_SYNC  = 0,  /**< 同步模式 - 立即在当前线程执行 */
    EM_MODE_ASYNC = 1   /**< 异步模式 - 放入队列，由事件循环处理 */
} em_mode_t;

/**
 * @brief 错误码枚举
 */
typedef enum {
    EM_OK                   = 0,    /**< 操作成功 */
    EM_ERR_INVALID_PARAM    = -1,   /**< 参数无效 */
    EM_ERR_NOT_INITIALIZED  = -2,   /**< 未初始化 */
    EM_ERR_ALREADY_INIT     = -3,   /**< 已经初始化 */
    EM_ERR_OUT_OF_MEMORY    = -4,   /**< 内存不足 */
    EM_ERR_QUEUE_FULL       = -5,   /**< 队列已满 */
    EM_ERR_QUEUE_EMPTY      = -6,   /**< 队列为空 */
    EM_ERR_MAX_SUBSCRIBERS  = -7,   /**< 订阅者已达上限 */
    EM_ERR_NOT_FOUND        = -8,   /**< 未找到 */
    EM_ERR_MUTEX_FAILED     = -9    /**< 互斥锁操作失败 */
} em_error_t;

/** 事件类型ID */
typedef uint32_t em_event_id_t;

/** 事件数据指针 */
typedef void* em_event_data_t;

/**
 * @brief 事件回调函数类型
 * 
 * @param event_id 事件类型ID
 * @param data 事件携带的数据
 * @param user_data 用户自定义数据(订阅时传入)
 */
typedef void (*em_callback_t)(em_event_id_t event_id, em_event_data_t data, void* user_data);

/**
 * @brief 事件结构体
 */
typedef struct {
    em_event_id_t   id;         /**< 事件ID */
    em_event_data_t data;       /**< 事件数据 */
    size_t          data_size;  /**< 数据大小(用于异步事件复制) */
    em_priority_t   priority;   /**< 优先级 */
    em_mode_t       mode;       /**< 处理模式 */
} em_event_t;

/**
 * @brief 订阅者结构体
 */
typedef struct {
    em_callback_t   callback;   /**< 回调函数 */
    void*           user_data;  /**< 用户数据 */
    em_priority_t   priority;   /**< 订阅者优先级 */
    bool            active;     /**< 是否激活 */
} em_subscriber_t;

/**
 * @brief 事件管理器统计信息
 */
typedef struct {
    uint32_t events_published;      /**< 已发布事件数 */
    uint32_t events_processed;      /**< 已处理事件数 */
    uint32_t async_queue_current;   /**< 当前异步队列中的事件数 */
    uint32_t async_queue_max;       /**< 异步队列峰值 */
    uint32_t subscribers_total;     /**< 总订阅者数 */
} em_stats_t;

/**
 * @brief 事件管理器句柄(不透明指针)
 */
typedef struct em_manager* em_handle_t;

/*============================================================================
 *                              API函数声明
 *============================================================================*/

/*--------------------------- 初始化与销毁 ----------------------------------*/

/**
 * @brief 创建事件管理器实例
 * 
 * @return em_handle_t 事件管理器句柄，失败返回NULL
 * 
 * @code
 * em_handle_t em = em_create();
 * if (em == NULL) {
 *     printf("创建事件管理器失败\n");
 * }
 * @endcode
 */
em_handle_t em_create(void);

/**
 * @brief 销毁事件管理器实例
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t 错误码
 */
em_error_t em_destroy(em_handle_t handle);

/*--------------------------- 订阅与取消订阅 --------------------------------*/

/**
 * @brief 订阅事件
 * 
 * @param handle 事件管理器句柄
 * @param event_id 要订阅的事件ID
 * @param callback 回调函数
 * @param user_data 用户数据(可选，传入NULL表示不使用)
 * @param priority 订阅者优先级(同一事件有多个订阅者时的处理顺序)
 * @return em_error_t 错误码
 * 
 * @code
 * void on_button_press(em_event_id_t id, em_event_data_t data, void* user) {
 *     printf("按钮被按下\n");
 * }
 * 
 * em_subscribe(em, EVENT_BUTTON_PRESS, on_button_press, NULL, EM_PRIORITY_NORMAL);
 * @endcode
 */
em_error_t em_subscribe(em_handle_t handle, 
                        em_event_id_t event_id, 
                        em_callback_t callback,
                        void* user_data,
                        em_priority_t priority);

/**
 * @brief 取消订阅事件
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @param callback 要取消的回调函数
 * @return em_error_t 错误码
 */
em_error_t em_unsubscribe(em_handle_t handle, 
                          em_event_id_t event_id, 
                          em_callback_t callback);

/**
 * @brief 取消某事件的所有订阅
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @return em_error_t 错误码
 */
em_error_t em_unsubscribe_all(em_handle_t handle, em_event_id_t event_id);

/*--------------------------- 事件发布 --------------------------------------*/

/**
 * @brief 发布同步事件(立即执行)
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @param data 事件数据(可选)
 * @return em_error_t 错误码
 * 
 * @code
 * // 发布一个简单事件
 * em_publish_sync(em, EVENT_TIMER_TICK, NULL);
 * 
 * // 发布带数据的事件
 * int value = 42;
 * em_publish_sync(em, EVENT_VALUE_CHANGED, &value);
 * @endcode
 */
em_error_t em_publish_sync(em_handle_t handle, 
                           em_event_id_t event_id, 
                           em_event_data_t data);

/**
 * @brief 发布异步事件(放入队列)
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @param data 事件数据
 * @param data_size 数据大小(异步事件需要复制数据，0表示只复制指针)
 * @param priority 事件优先级
 * @return em_error_t 错误码
 * 
 * @code
 * // 发布异步事件
 * char message[] = "Hello";
 * em_publish_async(em, EVENT_MESSAGE, message, sizeof(message), EM_PRIORITY_NORMAL);
 * @endcode
 */
em_error_t em_publish_async(em_handle_t handle, 
                            em_event_id_t event_id, 
                            em_event_data_t data,
                            size_t data_size,
                            em_priority_t priority);

/**
 * @brief 发布事件(通用接口)
 * 
 * @param handle 事件管理器句柄
 * @param event 事件结构体指针
 * @return em_error_t 错误码
 */
em_error_t em_publish(em_handle_t handle, const em_event_t* event);

/*--------------------------- 事件处理 --------------------------------------*/

/**
 * @brief 处理异步事件队列中的一个事件
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t EM_OK表示处理了一个事件，EM_ERR_QUEUE_EMPTY表示队列为空
 * 
 * @note 在主循环中调用此函数来处理异步事件
 */
em_error_t em_process_one(em_handle_t handle);

/**
 * @brief 处理异步事件队列中的所有事件
 * 
 * @param handle 事件管理器句柄
 * @return int 处理的事件数量
 */
int em_process_all(em_handle_t handle);

/**
 * @brief 启动事件循环(阻塞)
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t 错误码
 * 
 * @note 此函数会阻塞当前线程，直到调用 em_stop_loop
 */
em_error_t em_run_loop(em_handle_t handle);

/**
 * @brief 停止事件循环
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t 错误码
 */
em_error_t em_stop_loop(em_handle_t handle);

/*--------------------------- 工具函数 --------------------------------------*/

/**
 * @brief 获取统计信息
 * 
 * @param handle 事件管理器句柄
 * @param stats 输出统计信息
 * @return em_error_t 错误码
 */
em_error_t em_get_stats(em_handle_t handle, em_stats_t* stats);

/**
 * @brief 重置统计信息
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t 错误码
 */
em_error_t em_reset_stats(em_handle_t handle);

/**
 * @brief 获取指定事件的订阅者数量
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @return int 订阅者数量，错误时返回-1
 */
int em_get_subscriber_count(em_handle_t handle, em_event_id_t event_id);

/**
 * @brief 检查事件是否有订阅者
 * 
 * @param handle 事件管理器句柄
 * @param event_id 事件ID
 * @return bool 有订阅者返回true
 */
bool em_has_subscribers(em_handle_t handle, em_event_id_t event_id);

/**
 * @brief 获取异步队列中的事件数量
 * 
 * @param handle 事件管理器句柄
 * @return int 队列中的事件数量，错误时返回-1
 */
int em_get_queue_size(em_handle_t handle);

/**
 * @brief 清空异步事件队列
 * 
 * @param handle 事件管理器句柄
 * @return em_error_t 错误码
 */
em_error_t em_clear_queue(em_handle_t handle);

/**
 * @brief 获取错误码对应的字符串描述
 * 
 * @param error 错误码
 * @return const char* 错误描述字符串
 */
const char* em_error_string(em_error_t error);

/**
 * @brief 获取版本信息
 * 
 * @return const char* 版本字符串
 */
const char* em_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_MANAGER_H */
