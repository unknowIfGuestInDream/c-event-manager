# 架构设计文档

本文档详细介绍事件管理器的内部架构和设计原理，帮助你深入理解实现细节。

## 目录

- [整体架构](#整体架构)
- [核心数据结构](#核心数据结构)
- [订阅者管理](#订阅者管理)
- [异步事件队列](#异步事件队列)
- [线程安全机制](#线程安全机制)
- [优先级实现](#优先级实现)
- [内存管理](#内存管理)
- [设计权衡](#设计权衡)

---

## 整体架构

### 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      应用层 (Application)                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│   │  发布事件    │    │  订阅事件    │    │  处理事件    │    │
│   │  em_publish │    │ em_subscribe│    │ em_process  │    │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘    │
│          │                  │                  │           │
├──────────┼──────────────────┼──────────────────┼───────────┤
│          ▼                  ▼                  ▼           │
│   ┌─────────────────────────────────────────────────────┐  │
│   │              事件管理器核心 (em_manager)              │  │
│   ├─────────────────────────────────────────────────────┤  │
│   │  ┌─────────────────┐  ┌─────────────────────────┐   │  │
│   │  │ 订阅者列表       │  │ 异步事件队列              │   │  │
│   │  │ subscriber_list │  │ async_queues[3]         │   │  │
│   │  │ [64个事件类型]   │  │ [HIGH/NORMAL/LOW]       │   │  │
│   │  └─────────────────┘  └─────────────────────────┘   │  │
│   │                                                     │  │
│   │  ┌─────────────────┐  ┌─────────────────────────┐   │  │
│   │  │ 统计信息         │  │ 线程同步                  │   │  │
│   │  │ em_stats_t      │  │ mutex + cond            │   │  │
│   │  └─────────────────┘  └─────────────────────────┘   │  │
│   └─────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 模块划分

1. **初始化模块**: `em_create()` / `em_destroy()`
2. **订阅模块**: `em_subscribe()` / `em_unsubscribe()`
3. **发布模块**: `em_publish_sync()` / `em_publish_async()`
4. **处理模块**: `em_process_one()` / `em_process_all()` / `em_run_loop()`
5. **工具模块**: 统计、查询等辅助功能

---

## 核心数据结构

### 事件管理器结构 (em_manager)

```c
struct em_manager {
    // 订阅者管理: 每个事件类型有一个订阅者列表
    em_subscriber_list_t event_subscribers[EM_MAX_EVENT_TYPES];
    
    // 异步事件队列: 按优先级分离(HIGH/NORMAL/LOW)
    em_priority_queue_t  async_queues[EM_PRIORITY_COUNT];
    
    // 统计信息
    em_stats_t           stats;
    
    // 事件循环控制
    volatile bool        running;
    
    // 线程同步
    pthread_mutex_t      mutex;
    pthread_cond_t       cond;
    bool                 mutex_initialized;
};
```

**设计说明:**
- 使用静态数组而非动态分配，适合嵌入式环境
- 按优先级分离队列，简化优先级处理逻辑
- `volatile` 用于线程间共享的控制变量

### 订阅者结构

```c
typedef struct {
    em_callback_t callback;   // 回调函数指针
    void*         user_data;  // 用户数据
    em_priority_t priority;   // 订阅者优先级
    bool          active;     // 是否激活
} em_subscriber_t;

typedef struct {
    em_subscriber_t subscribers[EM_MAX_SUBSCRIBERS];
    int             count;    // 当前订阅者数量
    bool            sorted;   // 是否已按优先级排序
} em_subscriber_list_t;
```

### 异步队列结构

```c
typedef struct {
    em_event_t event;     // 事件信息
    void*      data_copy; // 数据副本(如果复制了数据)
    bool       used;      // 是否使用中
} em_queue_node_t;

typedef struct {
    em_queue_node_t nodes[EM_ASYNC_QUEUE_SIZE];
    int             head;   // 队列头
    int             tail;   // 队列尾
    int             count;  // 当前数量
} em_priority_queue_t;
```

---

## 订阅者管理

### 订阅流程

```
em_subscribe(em, event_id, callback, user_data, priority)
    │
    ▼
┌─────────────────────────┐
│ 1. 参数验证              │
│    - handle != NULL     │
│    - callback != NULL   │
│    - event_id 有效      │
│    - priority 有效      │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 2. 获取互斥锁           │
│    lock_manager(handle) │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 3. 检查是否已订阅       │
│    (避免重复订阅)       │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 4. 找到空闲槽位         │
│    subscribers[i].active │
│    == false             │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 5. 填充订阅者信息       │
│    - callback           │
│    - user_data          │
│    - priority           │
│    - active = true      │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 6. 标记需要重新排序     │
│    sorted = false       │
└───────────┬─────────────┘
            ▼
┌─────────────────────────┐
│ 7. 释放互斥锁           │
│    unlock_manager(...)  │
└─────────────────────────┘
```

### 优先级排序

订阅者按优先级排序，使用**延迟排序**策略：
- 订阅时只标记 `sorted = false`
- 分发事件时才真正排序
- 使用插入排序（数据量小，且大部分已排序）

```c
static void sort_subscribers(em_subscriber_list_t* list) {
    if (list->sorted || list->count <= 1) {
        return;  // 无需排序
    }
    
    // 插入排序
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
```

---

## 异步事件队列

### 队列设计

使用**环形队列**实现 FIFO：

```
队列状态示例 (size=8):

初始状态:
    [_][_][_][_][_][_][_][_]
     ↑head
     ↑tail
     count=0

入队3个元素:
    [A][B][C][_][_][_][_][_]
     ↑head     ↑tail
     count=3

出队2个元素:
    [_][_][C][_][_][_][_][_]
           ↑head  ↑tail
           count=1

继续入队(环绕):
    [F][G][C][D][E][_][_][_]
              ↑head     ↑tail
              count=5
```

### 按优先级分离队列

```c
em_priority_queue_t async_queues[EM_PRIORITY_COUNT];
// async_queues[0] = 高优先级队列
// async_queues[1] = 普通优先级队列
// async_queues[2] = 低优先级队列
```

处理时按优先级顺序检查：

```c
for (int i = 0; i < EM_PRIORITY_COUNT; i++) {
    if (handle->async_queues[i].count > 0) {
        // 处理这个队列的事件
        break;  // 高优先级队列有事件就不检查低优先级
    }
}
```

### 数据复制机制

异步事件的数据可能在处理前被修改，因此支持数据复制：

```c
// 发布时
void* data_copy = NULL;
if (data != NULL && data_size > 0) {
    data_copy = malloc(data_size);
    memcpy(data_copy, data, data_size);
}

// 处理后释放
if (data_copy != NULL) {
    free(data_copy);
}
```

---

## 线程安全机制

### 互斥锁使用原则

1. **所有共享数据访问必须加锁**
   - 订阅者列表
   - 异步队列
   - 统计信息

2. **回调执行在锁外**
   - 避免死锁（回调中可能再发布事件）
   - 提高并发性能

3. **条件变量用于事件循环**
   - `wait`: 队列为空时等待
   - `signal`: 新事件入队时唤醒

### 关键代码模式

```c
// 模式1: 简单的读写操作
lock_manager(handle);
// ... 读写共享数据 ...
unlock_manager(handle);

// 模式2: 需要在锁外执行回调
lock_manager(handle);
// 复制需要的数据到局部变量
em_subscriber_t copy[...];
// ... 复制 ...
unlock_manager(handle);

// 在锁外执行回调
for (...) {
    copy[i].callback(...);
}

// 模式3: 事件循环等待
lock_manager(handle);
while (queue_empty && running) {
    wait_manager(handle);  // 释放锁并等待
}
// ... 处理 ...
unlock_manager(handle);
```

### 事件分发时的并发处理

```c
static void dispatch_event(em_handle_t handle, ...) {
    lock_manager(handle);
    
    // 复制订阅者列表(快照)
    em_subscriber_t subscribers_copy[EM_MAX_SUBSCRIBERS];
    int count = 0;
    for (int i = 0; i < EM_MAX_SUBSCRIBERS; i++) {
        if (list->subscribers[i].active) {
            subscribers_copy[count++] = list->subscribers[i];
        }
    }
    
    unlock_manager(handle);
    
    // 在锁外执行回调(避免死锁)
    for (int i = 0; i < count; i++) {
        subscribers_copy[i].callback(event_id, data, 
                                     subscribers_copy[i].user_data);
    }
}
```

---

## 优先级实现

### 两层优先级

1. **事件优先级**: 决定异步队列中事件的处理顺序
2. **订阅者优先级**: 决定同一事件的多个回调的执行顺序

### 事件优先级实现

```
发布高优先级事件 → async_queues[HIGH]
发布普通优先级事件 → async_queues[NORMAL]
发布低优先级事件 → async_queues[LOW]

处理时:
1. 检查 HIGH 队列，有则处理
2. HIGH 为空则检查 NORMAL 队列
3. NORMAL 也为空则检查 LOW 队列
```

### 订阅者优先级实现

```
订阅者列表: [callback_A(LOW), callback_B(HIGH), callback_C(NORMAL)]

排序后: [callback_B(HIGH), callback_C(NORMAL), callback_A(LOW)]

执行顺序: B → C → A
```

---

## 内存管理

### 静态分配 vs 动态分配

本实现主要使用**静态分配**：

```c
struct em_manager {
    // 固定大小数组，编译时确定
    em_subscriber_list_t event_subscribers[EM_MAX_EVENT_TYPES];
    em_priority_queue_t  async_queues[EM_PRIORITY_COUNT];
    ...
};
```

**优点:**
- 无内存碎片
- 无运行时分配失败风险
- 适合资源受限环境

**动态分配仅用于:**
- `em_create()` 分配管理器结构本身
- 异步事件数据复制

### 内存使用估算

```c
// 管理器基本大小
sizeof(em_manager) ≈ 
    EM_MAX_EVENT_TYPES * sizeof(em_subscriber_list_t) +
    EM_PRIORITY_COUNT * sizeof(em_priority_queue_t) +
    sizeof(em_stats_t) + 同步原语

// 默认配置(64事件，16订阅者，32队列)
// 约 64 * (16 * 32 + 8) + 3 * (32 * 48 + 12) + 24 + ...
// ≈ 35KB + 5KB ≈ 40KB
```

---

## 设计权衡

### 1. 静态数组 vs 链表

**选择: 静态数组**

| 方面 | 静态数组 | 链表 |
|------|---------|------|
| 内存效率 | 预分配，可能浪费 | 按需分配 |
| 访问速度 | O(1) 直接索引 | O(n) 遍历 |
| 缓存友好 | 连续内存，友好 | 分散内存，不友好 |
| 实现复杂度 | 简单 | 需要指针管理 |
| 嵌入式适用性 | 好(无动态分配) | 一般 |

### 2. 分离队列 vs 单队列排序

**选择: 按优先级分离队列**

- 分离队列: 简单，O(1) 入队，O(1) 出队
- 单队列排序: 需要维护有序性，复杂度更高

### 3. 延迟排序 vs 即时排序

**选择: 延迟排序**

- 订阅时不排序，只标记
- 分发时才排序
- 减少订阅操作的开销

### 4. 复制订阅者列表执行回调

**原因:**
- 回调中可能修改订阅（订阅/取消订阅）
- 避免迭代时修改导致的问题
- 允许在锁外执行回调

**代价:**
- 每次分发需要复制
- 增加栈使用

---

## 扩展点

如果需要扩展，可以考虑：

1. **动态事件类型**: 使用哈希表替代固定数组
2. **更多优先级**: 增加 `EM_PRIORITY_COUNT`
3. **事件过滤**: 在订阅时添加过滤条件
4. **事件历史**: 记录最近的事件供调试
5. **远程事件**: 跨进程/网络事件传递
