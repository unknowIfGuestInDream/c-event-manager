/**
 * @file event_manager.c
 * @brief 嵌入式事件管理器实现
 * 
 * @author 梦里不知身是客
 * @version 1.0.0
 * @date 2026
 * @copyright MIT License
 */

#include "event_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if EM_ENABLE_THREADING
#include <pthread.h>
#endif

/* epoll 支持 (仅 Linux) */
#if EM_ENABLE_EPOLL && EM_ENABLE_THREADING
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#define EM_USE_EPOLL 1
#else
#define EM_USE_EPOLL 0
#endif
#else
#define EM_USE_EPOLL 0
#endif

/*============================================================================
 *                              版本信息
 *============================================================================*/

#define EM_VERSION_MAJOR    1
#define EM_VERSION_MINOR    0
#define EM_VERSION_PATCH    0
#define EM_VERSION_STRING   "1.0.0"

/*============================================================================
 *                              调试宏
 *============================================================================*/

#if EM_ENABLE_DEBUG
#define EM_DEBUG(fmt, ...) printf("[EM_DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define EM_DEBUG(fmt, ...) ((void)0)
#endif

/*============================================================================
 *                              内部数据结构
 *============================================================================*/

/**
 * @brief 异步事件队列节点
 */
typedef struct {
    em_event_t  event;          /**< 事件信息 */
    void*       data_copy;      /**< 数据副本(异步事件) */
    bool        used;           /**< 是否使用中 */
} em_queue_node_t;

/**
 * @brief 优先级队列
 */
typedef struct {
    em_queue_node_t nodes[EM_ASYNC_QUEUE_SIZE];   /**< 队列节点数组 */
    int             head;       /**< 队列头 */
    int             tail;       /**< 队列尾 */
    int             count;      /**< 当前数量 */
} em_priority_queue_t;

/**
 * @brief 事件类型的订阅者列表
 */
typedef struct {
    em_subscriber_t subscribers[EM_MAX_SUBSCRIBERS];  /**< 订阅者数组 */
    int             count;      /**< 订阅者数量 */
    bool            sorted;     /**< 是否已排序 */
} em_subscriber_list_t;

/**
 * @brief 事件管理器内部结构
 */
struct em_manager {
    /* 订阅者管理 */
    em_subscriber_list_t    event_subscribers[EM_MAX_EVENT_TYPES];
    
    /* 异步事件队列(按优先级分离) */
    em_priority_queue_t     async_queues[EM_PRIORITY_COUNT];
    
    /* 统计信息 */
    em_stats_t              stats;
    
    /* 事件循环控制 */
    volatile bool           running;
    
    /* 线程安全 */
#if EM_ENABLE_THREADING
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    bool                    mutex_initialized;
#endif

    /* epoll 支持 */
#if EM_USE_EPOLL
    int                     epoll_fd;       /**< epoll 文件描述符 */
    int                     event_fd;       /**< eventfd 用于通知 */
    bool                    epoll_initialized;
#endif
};

/*============================================================================
 *                              内部函数声明
 *============================================================================*/

static void sort_subscribers(em_subscriber_list_t* list);
static em_error_t enqueue_event(em_priority_queue_t* queue, const em_event_t* event, void* data_copy);
static em_error_t dequeue_event(em_priority_queue_t* queue, em_event_t* event, void** data_copy);
static void dispatch_event(em_handle_t handle, em_event_id_t event_id, em_event_data_t data);

#if EM_ENABLE_THREADING
static inline void lock_manager(em_handle_t handle) {
    if (handle && handle->mutex_initialized) {
        pthread_mutex_lock(&handle->mutex);
    }
}

static inline void unlock_manager(em_handle_t handle) {
    if (handle && handle->mutex_initialized) {
        pthread_mutex_unlock(&handle->mutex);
    }
}

static inline void signal_manager(em_handle_t handle) {
    if (handle && handle->mutex_initialized) {
#if EM_USE_EPOLL
        /* 使用 eventfd 通知 epoll */
        if (handle->epoll_initialized) {
            uint64_t val = 1;
            ssize_t ret = write(handle->event_fd, &val, sizeof(val));
            if (ret < 0) {
                EM_DEBUG("eventfd write failed");
            }
        }
#endif
        pthread_cond_signal(&handle->cond);
    }
}

static inline void wait_manager(em_handle_t handle) {
    if (handle && handle->mutex_initialized) {
        pthread_cond_wait(&handle->cond, &handle->mutex);
    }
}
#else
#define lock_manager(h)   ((void)0)
#define unlock_manager(h) ((void)0)
#define signal_manager(h) ((void)0)
#define wait_manager(h)   ((void)0)
#endif

/*============================================================================
 *                              初始化与销毁
 *============================================================================*/

em_handle_t em_create(void)
{
    em_handle_t handle = (em_handle_t)calloc(1, sizeof(struct em_manager));
    if (handle == NULL) {
        EM_DEBUG("Failed to allocate memory for event manager");
        return NULL;
    }
    
    /* 初始化订阅者列表 */
    for (int i = 0; i < EM_MAX_EVENT_TYPES; i++) {
        handle->event_subscribers[i].count = 0;
        handle->event_subscribers[i].sorted = true;
        for (int j = 0; j < EM_MAX_SUBSCRIBERS; j++) {
            handle->event_subscribers[i].subscribers[j].active = false;
        }
    }
    
    /* 初始化异步队列 */
    for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
        handle->async_queues[i].head = 0;
        handle->async_queues[i].tail = 0;
        handle->async_queues[i].count = 0;
        for (int j = 0; j < EM_ASYNC_QUEUE_SIZE; j++) {
            handle->async_queues[i].nodes[j].used = false;
            handle->async_queues[i].nodes[j].data_copy = NULL;
        }
    }
    
    /* 初始化统计信息 */
    memset(&handle->stats, 0, sizeof(em_stats_t));
    
    /* 初始化线程同步 */
#if EM_ENABLE_THREADING
    if (pthread_mutex_init(&handle->mutex, NULL) != 0) {
        EM_DEBUG("Failed to initialize mutex");
        free(handle);
        return NULL;
    }
    if (pthread_cond_init(&handle->cond, NULL) != 0) {
        EM_DEBUG("Failed to initialize condition variable");
        pthread_mutex_destroy(&handle->mutex);
        free(handle);
        return NULL;
    }
    handle->mutex_initialized = true;
#endif
    
    /* 初始化 epoll */
#if EM_USE_EPOLL
    handle->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (handle->epoll_fd < 0) {
        EM_DEBUG("Failed to create epoll fd");
#if EM_ENABLE_THREADING
        pthread_mutex_destroy(&handle->mutex);
        pthread_cond_destroy(&handle->cond);
#endif
        free(handle);
        return NULL;
    }
    
    handle->event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (handle->event_fd < 0) {
        EM_DEBUG("Failed to create eventfd");
        close(handle->epoll_fd);
#if EM_ENABLE_THREADING
        pthread_mutex_destroy(&handle->mutex);
        pthread_cond_destroy(&handle->cond);
#endif
        free(handle);
        return NULL;
    }
    
    /* 将 eventfd 添加到 epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = handle->event_fd;
    if (epoll_ctl(handle->epoll_fd, EPOLL_CTL_ADD, handle->event_fd, &ev) < 0) {
        EM_DEBUG("Failed to add eventfd to epoll");
        close(handle->event_fd);
        close(handle->epoll_fd);
#if EM_ENABLE_THREADING
        pthread_mutex_destroy(&handle->mutex);
        pthread_cond_destroy(&handle->cond);
#endif
        free(handle);
        return NULL;
    }
    
    handle->epoll_initialized = true;
    EM_DEBUG("epoll initialized (epoll_fd=%d, event_fd=%d)", handle->epoll_fd, handle->event_fd);
#endif
    
    handle->running = false;
    
    EM_DEBUG("Event manager created successfully");
    return handle;
}

em_error_t em_destroy(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    /* 停止事件循环 */
    handle->running = false;
    
#if EM_ENABLE_THREADING
    signal_manager(handle);  /* 唤醒可能等待的线程 */
#endif
    
    lock_manager(handle);
    
    /* 清理异步队列中的数据副本 */
    for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
        for (int j = 0; j < EM_ASYNC_QUEUE_SIZE; j++) {
            if (handle->async_queues[i].nodes[j].data_copy != NULL) {
                free(handle->async_queues[i].nodes[j].data_copy);
                handle->async_queues[i].nodes[j].data_copy = NULL;
            }
        }
    }
    
    unlock_manager(handle);
    
#if EM_USE_EPOLL
    /* 清理 epoll 资源 - 先设置标志位防止其他线程使用 */
    if (handle->epoll_initialized) {
        handle->epoll_initialized = false;
        close(handle->event_fd);
        close(handle->epoll_fd);
        EM_DEBUG("epoll resources cleaned up");
    }
#endif
    
#if EM_ENABLE_THREADING
    if (handle->mutex_initialized) {
        pthread_mutex_destroy(&handle->mutex);
        pthread_cond_destroy(&handle->cond);
    }
#endif
    
    free(handle);
    
    EM_DEBUG("Event manager destroyed");
    return EM_OK;
}

/*============================================================================
 *                              订阅与取消订阅
 *============================================================================*/

em_error_t em_subscribe(em_handle_t handle, 
                        em_event_id_t event_id, 
                        em_callback_t callback,
                        void* user_data,
                        em_priority_t priority)
{
    if (handle == NULL || callback == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (priority >= EM_PRIORITY_COUNT) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    
    em_subscriber_list_t* list = &handle->event_subscribers[event_id];
    
    /* 检查是否已达到订阅者上限 */
    if (list->count >= EM_MAX_SUBSCRIBERS) {
        unlock_manager(handle);
        return EM_ERR_MAX_SUBSCRIBERS;
    }
    
    /* 检查是否已经订阅(避免重复) */
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (list->subscribers[i].active && 
            list->subscribers[i].callback == callback) {
            unlock_manager(handle);
            return EM_OK;  /* 已经订阅，直接返回成功 */
        }
    }
    
    /* 找到空闲槽位 */
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (!list->subscribers[i].active) {
            list->subscribers[i].callback = callback;
            list->subscribers[i].user_data = user_data;
            list->subscribers[i].priority = priority;
            list->subscribers[i].active = true;
            list->count++;
            list->sorted = false;  /* 需要重新排序 */
            handle->stats.subscribers_total++;
            
            EM_DEBUG("Subscribed to event %u (priority=%d)", event_id, priority);
            unlock_manager(handle);
            return EM_OK;
        }
    }
    
    unlock_manager(handle);
    return EM_ERR_MAX_SUBSCRIBERS;
}

em_error_t em_unsubscribe(em_handle_t handle, 
                          em_event_id_t event_id, 
                          em_callback_t callback)
{
    if (handle == NULL || callback == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    
    em_subscriber_list_t* list = &handle->event_subscribers[event_id];
    
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (list->subscribers[i].active && 
            list->subscribers[i].callback == callback) {
            list->subscribers[i].active = false;
            list->subscribers[i].callback = NULL;
            list->subscribers[i].user_data = NULL;
            list->count--;
            handle->stats.subscribers_total--;
            
            EM_DEBUG("Unsubscribed from event %u", event_id);
            unlock_manager(handle);
            return EM_OK;
        }
    }
    
    unlock_manager(handle);
    return EM_ERR_NOT_FOUND;
}

em_error_t em_unsubscribe_all(em_handle_t handle, em_event_id_t event_id)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    
    em_subscriber_list_t* list = &handle->event_subscribers[event_id];
    
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (list->subscribers[i].active) {
            list->subscribers[i].active = false;
            list->subscribers[i].callback = NULL;
            list->subscribers[i].user_data = NULL;
            handle->stats.subscribers_total--;
        }
    }
    list->count = 0;
    list->sorted = true;
    
    EM_DEBUG("Unsubscribed all from event %u", event_id);
    unlock_manager(handle);
    return EM_OK;
}

/*============================================================================
 *                              事件发布
 *============================================================================*/

em_error_t em_publish_sync(em_handle_t handle, 
                           em_event_id_t event_id, 
                           em_event_data_t data)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    handle->stats.events_published++;
    unlock_manager(handle);
    
    /* 同步事件直接分发 */
    dispatch_event(handle, event_id, data);
    
    EM_DEBUG("Published sync event %u", event_id);
    return EM_OK;
}

em_error_t em_publish_async(em_handle_t handle, 
                            em_event_id_t event_id, 
                            em_event_data_t data,
                            size_t data_size,
                            em_priority_t priority)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (priority >= EM_PRIORITY_COUNT) {
        return EM_ERR_INVALID_PARAM;
    }
    
    /* 准备事件数据副本 */
    void* data_copy = NULL;
    if (data != NULL && data_size > 0) {
        data_copy = malloc(data_size);
        if (data_copy == NULL) {
            return EM_ERR_OUT_OF_MEMORY;
        }
        memcpy(data_copy, data, data_size);
    }
    
    em_event_t event = {
        .id = event_id,
        .data = data_copy ? data_copy : data,
        .data_size = data_size,
        .priority = priority,
        .mode = EM_MODE_ASYNC
    };
    
    lock_manager(handle);
    
    em_error_t result = enqueue_event(&handle->async_queues[priority], &event, data_copy);
    
    if (result == EM_OK) {
        handle->stats.events_published++;
        
        /* 更新队列统计 */
        uint32_t total = 0;
        for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
            total += handle->async_queues[i].count;
        }
        handle->stats.async_queue_current = total;
        if (total > handle->stats.async_queue_max) {
            handle->stats.async_queue_max = total;
        }
        
        EM_DEBUG("Published async event %u (priority=%d)", event_id, priority);
        
#if EM_ENABLE_THREADING
        signal_manager(handle);  /* 通知事件循环有新事件 */
#endif
    } else {
        /* 入队失败，释放数据副本 */
        if (data_copy != NULL) {
            free(data_copy);
        }
    }
    
    unlock_manager(handle);
    return result;
}

em_error_t em_publish(em_handle_t handle, const em_event_t* event)
{
    if (handle == NULL || event == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    if (event->mode == EM_MODE_SYNC) {
        return em_publish_sync(handle, event->id, event->data);
    } else {
        return em_publish_async(handle, event->id, event->data, 
                               event->data_size, event->priority);
    }
}

/*============================================================================
 *                              事件处理
 *============================================================================*/

em_error_t em_process_one(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    em_event_t event;
    void* data_copy = NULL;
    em_error_t result = EM_ERR_QUEUE_EMPTY;
    
    lock_manager(handle);
    
    /* 
     * 按优先级顺序处理(HIGH -> NORMAL -> LOW)
     * 注意: 此处依赖于优先级枚举值按升序排列:
     * EM_PRIORITY_HIGH=0, EM_PRIORITY_NORMAL=1, EM_PRIORITY_LOW=2
     */
    for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
        if (handle->async_queues[i].count > 0) {
            result = dequeue_event(&handle->async_queues[i], &event, &data_copy);
            if (result == EM_OK) {
                /* 更新队列统计 */
                uint32_t total = 0;
                for (int j = 0; j < EM_PRIORITY_COUNT; j++) {
                    total += handle->async_queues[j].count;
                }
                handle->stats.async_queue_current = total;
                break;
            }
        }
    }
    
    unlock_manager(handle);
    
    /* 在锁外执行事件分发(避免死锁) */
    if (result == EM_OK) {
        dispatch_event(handle, event.id, event.data);
        
        /* 释放数据副本 */
        if (data_copy != NULL) {
            free(data_copy);
        }
    }
    
    return result;
}

int em_process_all(em_handle_t handle)
{
    if (handle == NULL) {
        return -1;
    }
    
    int count = 0;
    while (em_process_one(handle) == EM_OK) {
        count++;
    }
    
    return count;
}

em_error_t em_run_loop(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    handle->running = true;
    
    EM_DEBUG("Event loop started");
    
#if EM_USE_EPOLL
    /* 使用 epoll 的事件循环 */
    struct epoll_event events[1];
    
    while (handle->running) {
        lock_manager(handle);
        
        /* 检查是否有待处理的事件 */
        bool has_events = false;
        for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
            if (handle->async_queues[i].count > 0) {
                has_events = true;
                break;
            }
        }
        
        unlock_manager(handle);
        
        if (has_events) {
            /* 处理所有待处理的事件 */
            em_process_all(handle);
        } else if (handle->running && handle->epoll_initialized) {
            /* 使用 epoll 等待新事件，超时 100ms */
            int nfds = epoll_wait(handle->epoll_fd, events, 1, 100);
            if (nfds > 0) {
                /* 清空 eventfd 的计数器 */
                uint64_t val;
                ssize_t ret = read(handle->event_fd, &val, sizeof(val));
                if (ret > 0) {
                    EM_DEBUG("epoll woke up, eventfd val=%lu", (unsigned long)val);
                } else if (ret < 0) {
                    EM_DEBUG("eventfd read failed");
                }
            }
        }
    }
#else
    /* 原始的条件变量事件循环 */
    while (handle->running) {
        lock_manager(handle);
        
        /* 检查是否有待处理的事件 */
        bool has_events = false;
        for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
            if (handle->async_queues[i].count > 0) {
                has_events = true;
                break;
            }
        }
        
        if (!has_events && handle->running) {
#if EM_ENABLE_THREADING
            /* 等待新事件 */
            wait_manager(handle);
#endif
        }
        
        unlock_manager(handle);
        
        /* 处理所有待处理的事件 */
        em_process_all(handle);
    }
#endif
    
    EM_DEBUG("Event loop stopped");
    return EM_OK;
}

em_error_t em_stop_loop(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    handle->running = false;
    
#if EM_ENABLE_THREADING
    lock_manager(handle);
    signal_manager(handle);  /* 唤醒事件循环 */
    unlock_manager(handle);
#endif
    
    EM_DEBUG("Event loop stop requested");
    return EM_OK;
}

/*============================================================================
 *                              工具函数
 *============================================================================*/

em_error_t em_get_stats(em_handle_t handle, em_stats_t* stats)
{
    if (handle == NULL || stats == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    memcpy(stats, &handle->stats, sizeof(em_stats_t));
    unlock_manager(handle);
    
    return EM_OK;
}

em_error_t em_reset_stats(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    
    /* 保留当前订阅者数量和队列状态 */
    uint32_t subscribers = handle->stats.subscribers_total;
    uint32_t queue_current = handle->stats.async_queue_current;
    
    memset(&handle->stats, 0, sizeof(em_stats_t));
    
    handle->stats.subscribers_total = subscribers;
    handle->stats.async_queue_current = queue_current;
    
    unlock_manager(handle);
    
    return EM_OK;
}

int em_get_subscriber_count(em_handle_t handle, em_event_id_t event_id)
{
    if (handle == NULL || event_id >= EM_MAX_EVENT_TYPES) {
        return -1;
    }
    
    lock_manager(handle);
    int count = handle->event_subscribers[event_id].count;
    unlock_manager(handle);
    
    return count;
}

bool em_has_subscribers(em_handle_t handle, em_event_id_t event_id)
{
    return em_get_subscriber_count(handle, event_id) > 0;
}

int em_get_queue_size(em_handle_t handle)
{
    if (handle == NULL) {
        return -1;
    }
    
    lock_manager(handle);
    
    int total = 0;
    for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
        total += handle->async_queues[i].count;
    }
    
    unlock_manager(handle);
    return total;
}

em_error_t em_clear_queue(em_handle_t handle)
{
    if (handle == NULL) {
        return EM_ERR_INVALID_PARAM;
    }
    
    lock_manager(handle);
    
    for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
        em_priority_queue_t* queue = &handle->async_queues[i];
        
        /* 释放所有数据副本 */
        for (int j = 0; j < EM_ASYNC_QUEUE_SIZE; j++) {
            if (queue->nodes[j].data_copy != NULL) {
                free(queue->nodes[j].data_copy);
                queue->nodes[j].data_copy = NULL;
            }
            queue->nodes[j].used = false;
        }
        
        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;
    }
    
    handle->stats.async_queue_current = 0;
    
    unlock_manager(handle);
    
    EM_DEBUG("Async queue cleared");
    return EM_OK;
}

const char* em_error_string(em_error_t error)
{
    switch (error) {
        case EM_OK:                 return "Success";
        case EM_ERR_INVALID_PARAM:  return "Invalid parameter";
        case EM_ERR_NOT_INITIALIZED: return "Not initialized";
        case EM_ERR_ALREADY_INIT:   return "Already initialized";
        case EM_ERR_OUT_OF_MEMORY:  return "Out of memory";
        case EM_ERR_QUEUE_FULL:     return "Queue is full";
        case EM_ERR_QUEUE_EMPTY:    return "Queue is empty";
        case EM_ERR_MAX_SUBSCRIBERS: return "Maximum subscribers reached";
        case EM_ERR_NOT_FOUND:      return "Not found";
        case EM_ERR_MUTEX_FAILED:   return "Mutex operation failed";
        default:                    return "Unknown error";
    }
}

const char* em_version(void)
{
    return EM_VERSION_STRING;
}

/*============================================================================
 *                              内部函数实现
 *============================================================================*/

/**
 * @brief 对订阅者列表按优先级排序(插入排序)
 */
static void sort_subscribers(em_subscriber_list_t* list)
{
    if (list->sorted || list->count <= 1) {
        return;
    }
    
    /* 简单的插入排序 */
    for (int i = 1; i < EM_MAX_SUBSCRIBERS; i++) {
        if (!list->subscribers[i].active) continue;
        
        em_subscriber_t temp = list->subscribers[i];
        int j = i - 1;
        
        while (j >= 0 && list->subscribers[j].active && 
               list->subscribers[j].priority > temp.priority) {
            list->subscribers[j + 1] = list->subscribers[j];
            j--;
        }
        
        list->subscribers[j + 1] = temp;
    }
    
    list->sorted = true;
}

/**
 * @brief 将事件加入队列
 */
static em_error_t enqueue_event(em_priority_queue_t* queue, 
                                const em_event_t* event, 
                                void* data_copy)
{
    if (queue->count >= EM_ASYNC_QUEUE_SIZE) {
        return EM_ERR_QUEUE_FULL;
    }
    
    int idx = queue->tail;
    queue->nodes[idx].event = *event;
    queue->nodes[idx].data_copy = data_copy;
    queue->nodes[idx].used = true;
    
    queue->tail = (queue->tail + 1) % EM_ASYNC_QUEUE_SIZE;
    queue->count++;
    
    return EM_OK;
}

/**
 * @brief 从队列取出事件
 */
static em_error_t dequeue_event(em_priority_queue_t* queue, 
                                em_event_t* event, 
                                void** data_copy)
{
    if (queue->count == 0) {
        return EM_ERR_QUEUE_EMPTY;
    }
    
    int idx = queue->head;
    *event = queue->nodes[idx].event;
    *data_copy = queue->nodes[idx].data_copy;
    
    queue->nodes[idx].used = false;
    queue->nodes[idx].data_copy = NULL;
    
    queue->head = (queue->head + 1) % EM_ASYNC_QUEUE_SIZE;
    queue->count--;
    
    return EM_OK;
}

/**
 * @brief 分发事件到所有订阅者
 */
static void dispatch_event(em_handle_t handle, 
                          em_event_id_t event_id, 
                          em_event_data_t data)
{
    if (event_id >= EM_MAX_EVENT_TYPES) {
        return;
    }
    
    lock_manager(handle);
    
    em_subscriber_list_t* list = &handle->event_subscribers[event_id];
    
    /* 确保按优先级排序 */
    if (!list->sorted) {
        sort_subscribers(list);
    }
    
    /* 复制订阅者列表(避免在回调中修改) */
    em_subscriber_t subscribers_copy[EM_MAX_SUBSCRIBERS];
    int count = 0;
    
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (list->subscribers[i].active) {
            subscribers_copy[count++] = list->subscribers[i];
        }
    }
    
    handle->stats.events_processed++;
    
    unlock_manager(handle);
    
    /* 在锁外调用回调(避免死锁) */
    for (int i = 0; i < count; i++) {
        if (subscribers_copy[i].callback != NULL) {
            subscribers_copy[i].callback(event_id, data, subscribers_copy[i].user_data);
        }
    }
    
    EM_DEBUG("Dispatched event %u to %d subscribers", event_id, count);
}
