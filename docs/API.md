# API 参考文档

本文档详细描述了事件管理器的所有公开 API。

## 目录

- [数据类型](#数据类型)
- [初始化与销毁](#初始化与销毁)
- [订阅与取消订阅](#订阅与取消订阅)
- [事件发布](#事件发布)
- [事件处理](#事件处理)
- [工具函数](#工具函数)
- [错误码](#错误码)

---

## 数据类型

### em_priority_t - 优先级枚举

```c
typedef enum {
    EM_PRIORITY_HIGH    = 0,    // 高优先级 - 最先处理
    EM_PRIORITY_NORMAL  = 1,    // 普通优先级 - 默认
    EM_PRIORITY_LOW     = 2,    // 低优先级 - 最后处理
    EM_PRIORITY_COUNT   = 3     // 优先级数量
} em_priority_t;
```

优先级用于两个地方：
1. **订阅者优先级**：同一事件有多个订阅者时，高优先级订阅者先执行
2. **事件优先级**：异步队列中，高优先级事件先被处理

### em_mode_t - 处理模式枚举

```c
typedef enum {
    EM_MODE_SYNC  = 0,  // 同步模式 - 立即在当前线程执行
    EM_MODE_ASYNC = 1   // 异步模式 - 放入队列，由事件循环处理
} em_mode_t;
```

### em_error_t - 错误码枚举

```c
typedef enum {
    EM_OK                   = 0,    // 操作成功
    EM_ERR_INVALID_PARAM    = -1,   // 参数无效
    EM_ERR_NOT_INITIALIZED  = -2,   // 未初始化
    EM_ERR_ALREADY_INIT     = -3,   // 已经初始化
    EM_ERR_OUT_OF_MEMORY    = -4,   // 内存不足
    EM_ERR_QUEUE_FULL       = -5,   // 队列已满
    EM_ERR_QUEUE_EMPTY      = -6,   // 队列为空
    EM_ERR_MAX_SUBSCRIBERS  = -7,   // 订阅者已达上限
    EM_ERR_NOT_FOUND        = -8,   // 未找到
    EM_ERR_MUTEX_FAILED     = -9    // 互斥锁操作失败
} em_error_t;
```

### em_event_id_t - 事件ID类型

```c
typedef uint32_t em_event_id_t;
```

事件 ID 是一个 32 位无符号整数，范围从 0 到 `EM_MAX_EVENT_TYPES - 1`。

### em_event_data_t - 事件数据类型

```c
typedef void* em_event_data_t;
```

事件数据是一个 void 指针，可以指向任意类型的数据。

### em_callback_t - 回调函数类型

```c
typedef void (*em_callback_t)(em_event_id_t event_id, 
                               em_event_data_t data, 
                               void* user_data);
```

回调函数参数：
- `event_id`: 触发回调的事件 ID
- `data`: 事件携带的数据
- `user_data`: 订阅时传入的用户数据

### em_event_t - 事件结构体

```c
typedef struct {
    em_event_id_t   id;         // 事件ID
    em_event_data_t data;       // 事件数据
    size_t          data_size;  // 数据大小(用于异步事件复制)
    em_priority_t   priority;   // 优先级
    em_mode_t       mode;       // 处理模式
} em_event_t;
```

### em_stats_t - 统计信息结构体

```c
typedef struct {
    uint32_t events_published;      // 已发布事件数
    uint32_t events_processed;      // 已处理事件数
    uint32_t async_queue_current;   // 当前异步队列中的事件数
    uint32_t async_queue_max;       // 异步队列峰值
    uint32_t subscribers_total;     // 总订阅者数
} em_stats_t;
```

### em_handle_t - 事件管理器句柄

```c
typedef struct em_manager* em_handle_t;
```

不透明指针类型，用户不应直接访问内部结构。

---

## 初始化与销毁

### em_create()

创建事件管理器实例。

```c
em_handle_t em_create(void);
```

**返回值:**
- 成功：返回事件管理器句柄
- 失败：返回 `NULL`

**示例:**
```c
em_handle_t em = em_create();
if (em == NULL) {
    fprintf(stderr, "创建事件管理器失败\n");
    return -1;
}
```

### em_destroy()

销毁事件管理器实例。

```c
em_error_t em_destroy(em_handle_t handle);
```

**参数:**
- `handle`: 事件管理器句柄

**返回值:**
- `EM_OK`: 成功
- `EM_ERR_INVALID_PARAM`: 句柄为 NULL

**注意:**
- 销毁前会自动清理所有资源
- 如果有事件循环在运行，会先停止它

---

## 订阅与取消订阅

### em_subscribe()

订阅事件。

```c
em_error_t em_subscribe(em_handle_t handle, 
                        em_event_id_t event_id, 
                        em_callback_t callback,
                        void* user_data,
                        em_priority_t priority);
```

**参数:**
- `handle`: 事件管理器句柄
- `event_id`: 要订阅的事件 ID (0 到 EM_MAX_EVENT_TYPES-1)
- `callback`: 回调函数
- `user_data`: 用户数据（可选，传 NULL 表示不使用）
- `priority`: 订阅者优先级

**返回值:**
- `EM_OK`: 成功
- `EM_ERR_INVALID_PARAM`: 参数无效
- `EM_ERR_MAX_SUBSCRIBERS`: 该事件订阅者已达上限

**示例:**
```c
void on_event(em_event_id_t id, em_event_data_t data, void* user) {
    printf("事件 %u 触发\n", id);
}

// 基本订阅
em_subscribe(em, EVENT_ID, on_event, NULL, EM_PRIORITY_NORMAL);

// 带用户数据
int context = 42;
em_subscribe(em, EVENT_ID, on_event, &context, EM_PRIORITY_HIGH);
```

### em_unsubscribe()

取消订阅事件。

```c
em_error_t em_unsubscribe(em_handle_t handle, 
                          em_event_id_t event_id, 
                          em_callback_t callback);
```

**参数:**
- `handle`: 事件管理器句柄
- `event_id`: 事件 ID
- `callback`: 要取消的回调函数

**返回值:**
- `EM_OK`: 成功
- `EM_ERR_NOT_FOUND`: 未找到该订阅

### em_unsubscribe_all()

取消某事件的所有订阅。

```c
em_error_t em_unsubscribe_all(em_handle_t handle, em_event_id_t event_id);
```

---

## 事件发布

### em_publish_sync()

发布同步事件（立即执行）。

```c
em_error_t em_publish_sync(em_handle_t handle, 
                           em_event_id_t event_id, 
                           em_event_data_t data);
```

**参数:**
- `handle`: 事件管理器句柄
- `event_id`: 事件 ID
- `data`: 事件数据（可选）

**特点:**
- 在当前线程立即执行所有订阅者的回调
- 阻塞调用者直到所有回调执行完毕
- 数据指针直接传递给回调，不复制

**示例:**
```c
// 无数据事件
em_publish_sync(em, EVENT_TIMER, NULL);

// 带数据事件
int value = 100;
em_publish_sync(em, EVENT_VALUE, &value);
```

### em_publish_async()

发布异步事件（放入队列）。

```c
em_error_t em_publish_async(em_handle_t handle, 
                            em_event_id_t event_id, 
                            em_event_data_t data,
                            size_t data_size,
                            em_priority_t priority);
```

**参数:**
- `handle`: 事件管理器句柄
- `event_id`: 事件 ID
- `data`: 事件数据
- `data_size`: 数据大小（>0 时复制数据，=0 时只传指针）
- `priority`: 事件优先级

**返回值:**
- `EM_OK`: 成功
- `EM_ERR_QUEUE_FULL`: 队列已满
- `EM_ERR_OUT_OF_MEMORY`: 内存分配失败

**示例:**
```c
// 不复制数据（数据必须在处理前保持有效）
em_publish_async(em, EVENT_ID, &value, 0, EM_PRIORITY_NORMAL);

// 复制数据（安全，即使原数据被修改）
em_publish_async(em, EVENT_ID, &value, sizeof(value), EM_PRIORITY_HIGH);
```

### em_publish()

通用发布接口。

```c
em_error_t em_publish(em_handle_t handle, const em_event_t* event);
```

根据 `event->mode` 自动选择同步或异步发布。

---

## 事件处理

### em_process_one()

处理异步队列中的一个事件。

```c
em_error_t em_process_one(em_handle_t handle);
```

**返回值:**
- `EM_OK`: 成功处理一个事件
- `EM_ERR_QUEUE_EMPTY`: 队列为空

**示例:**
```c
// 在主循环中处理事件
while (running) {
    // 其他工作...
    
    em_error_t err = em_process_one(em);
    if (err == EM_ERR_QUEUE_EMPTY) {
        // 没有事件，休眠或做其他事
    }
}
```

### em_process_all()

处理队列中的所有事件。

```c
int em_process_all(em_handle_t handle);
```

**返回值:**
- 处理的事件数量（>=0）
- 错误时返回 -1

### em_run_loop()

启动事件循环（阻塞）。

```c
em_error_t em_run_loop(em_handle_t handle);
```

此函数会阻塞当前线程，持续处理异步事件，直到调用 `em_stop_loop()`。

**注意:** 需要在另一个线程中调用 `em_stop_loop()` 来停止循环。

### em_stop_loop()

停止事件循环。

```c
em_error_t em_stop_loop(em_handle_t handle);
```

---

## 工具函数

### em_get_stats()

获取统计信息。

```c
em_error_t em_get_stats(em_handle_t handle, em_stats_t* stats);
```

### em_reset_stats()

重置统计信息。

```c
em_error_t em_reset_stats(em_handle_t handle);
```

### em_get_subscriber_count()

获取指定事件的订阅者数量。

```c
int em_get_subscriber_count(em_handle_t handle, em_event_id_t event_id);
```

### em_has_subscribers()

检查事件是否有订阅者。

```c
bool em_has_subscribers(em_handle_t handle, em_event_id_t event_id);
```

### em_get_queue_size()

获取异步队列中的事件数量。

```c
int em_get_queue_size(em_handle_t handle);
```

### em_clear_queue()

清空异步事件队列。

```c
em_error_t em_clear_queue(em_handle_t handle);
```

### em_error_string()

获取错误码对应的字符串描述。

```c
const char* em_error_string(em_error_t error);
```

### em_version()

获取版本信息。

```c
const char* em_version(void);
```

---

## 错误码

| 错误码 | 值 | 描述 |
|-------|---|------|
| `EM_OK` | 0 | 操作成功 |
| `EM_ERR_INVALID_PARAM` | -1 | 参数无效 |
| `EM_ERR_NOT_INITIALIZED` | -2 | 未初始化 |
| `EM_ERR_ALREADY_INIT` | -3 | 已经初始化 |
| `EM_ERR_OUT_OF_MEMORY` | -4 | 内存不足 |
| `EM_ERR_QUEUE_FULL` | -5 | 队列已满 |
| `EM_ERR_QUEUE_EMPTY` | -6 | 队列为空 |
| `EM_ERR_MAX_SUBSCRIBERS` | -7 | 订阅者已达上限 |
| `EM_ERR_NOT_FOUND` | -8 | 未找到 |
| `EM_ERR_MUTEX_FAILED` | -9 | 互斥锁操作失败 |

---

## 配置宏

| 宏 | 默认值 | 描述 |
|---|---|---|
| `EM_MAX_EVENT_TYPES` | 64 | 最大事件类型数量 |
| `EM_MAX_SUBSCRIBERS` | 16 | 每种事件最大订阅者数 |
| `EM_ASYNC_QUEUE_SIZE` | 32 | 每个优先级的异步队列大小 |
| `EM_ENABLE_THREADING` | 1 | 是否启用多线程支持 |
| `EM_ENABLE_DEBUG` | 0 | 是否启用调试日志 |
