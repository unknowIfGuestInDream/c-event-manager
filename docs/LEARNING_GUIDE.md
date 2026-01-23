# 学习指南

本指南将帮助你从零开始学习事件管理器的概念和实现，适合 C 语言初学者。

## 目录

- [基础概念](#基础概念)
- [学习路径](#学习路径)
- [代码阅读指南](#代码阅读指南)
- [动手实验](#动手实验)
- [进阶主题](#进阶主题)
- [常见问题](#常见问题)
- [推荐资源](#推荐资源)

---

## 基础概念

### 什么是事件管理器？

事件管理器是一种**设计模式**，也叫做：
- 发布-订阅模式 (Publish-Subscribe)
- 观察者模式 (Observer Pattern)
- 事件驱动架构 (Event-Driven Architecture)

**核心思想**: 解耦组件之间的直接依赖

```
传统方式(紧耦合):
┌─────────┐  直接调用   ┌─────────┐
│ 按钮模块 │ ────────→ │ 显示模块 │
└─────────┘           └─────────┘
            ↓
       按钮需要知道显示模块的存在

事件驱动(松耦合):
┌─────────┐  发布事件   ┌────────────┐  分发事件   ┌─────────┐
│ 按钮模块 │ ────────→ │ 事件管理器  │ ────────→ │ 显示模块 │
└─────────┘           └────────────┘           └─────────┘
                              │                     ↑
                              └──────────────────────
                                 订阅事件

       按钮只知道事件，不知道谁在监听
```

### 为什么需要事件管理器？

1. **解耦**: 发布者和订阅者互不知道对方存在
2. **灵活性**: 可以动态添加/移除订阅者
3. **可扩展**: 新增功能不需要修改现有代码
4. **异步处理**: 支持延迟处理，不阻塞发布者

### 核心概念解释

| 概念 | 解释 | 类比 |
|-----|------|------|
| 事件 (Event) | 发生了某件事的通知 | 广播电台的节目 |
| 事件ID | 事件的唯一标识 | 广播频道号 |
| 发布 (Publish) | 触发一个事件 | 电台播放节目 |
| 订阅 (Subscribe) | 注册对某事件的兴趣 | 调到某个频道 |
| 回调 (Callback) | 事件发生时执行的函数 | 收到广播后的反应 |
| 优先级 (Priority) | 处理的先后顺序 | VIP 优先服务 |
| 同步 (Sync) | 立即执行，等待完成 | 当面对话 |
| 异步 (Async) | 放入队列，稍后执行 | 留言信箱 |

---

## 学习路径

建议按以下顺序学习：

### 阶段1: 基础使用 (1-2小时)

1. **阅读 README.md** - 了解项目结构
2. **运行 basic_example** - 看看效果
   ```bash
   make examples
   ./build/basic_example
   ```
3. **阅读 basic_example.c** - 理解基本用法
4. **修改示例** - 尝试添加新事件

**学习目标:**
- [ ] 理解 em_create/em_destroy 的作用
- [ ] 理解 em_subscribe 的参数含义
- [ ] 理解 em_publish_sync 的工作方式
- [ ] 能够写简单的回调函数

### 阶段2: 理解优先级 (1-2小时)

1. **运行 priority_example**
   ```bash
   ./build/priority_example
   ```
2. **阅读 priority_example.c** - 理解优先级机制
3. **实验**: 修改订阅顺序，观察执行顺序

**学习目标:**
- [ ] 理解订阅者优先级的作用
- [ ] 理解事件优先级的作用
- [ ] 知道什么时候用哪种优先级

### 阶段3: 同步与异步 (2-3小时)

1. **运行 async_example**
   ```bash
   ./build/async_example
   ```
2. **阅读 async_example.c** - 理解同步异步区别
3. **实验**: 
   - 在异步事件处理前修改数据，观察效果
   - 尝试 data_size=0 和 data_size>0 的区别

**学习目标:**
- [ ] 理解同步和异步的区别
- [ ] 知道什么时候用同步，什么时候用异步
- [ ] 理解数据复制的重要性

### 阶段4: 多线程 (2-3小时)

1. **运行 multithread_example**
   ```bash
   ./build/multithread_example
   ```
2. **阅读 multithread_example.c**
3. **学习 pthread 基础知识** (如果不熟悉)

**学习目标:**
- [ ] 理解多线程环境下的挑战
- [ ] 理解互斥锁的作用
- [ ] 能够在多线程程序中使用事件管理器

### 阶段5: 源码分析 (3-5小时)

1. **阅读 event_manager.h** - 理解 API 设计
2. **阅读 ARCHITECTURE.md** - 理解内部结构
3. **逐步阅读 event_manager.c**:
   - 先看数据结构定义
   - 再看 em_create/em_destroy
   - 然后看 em_subscribe/em_unsubscribe
   - 接着看 em_publish_sync
   - 最后看异步相关代码

**学习目标:**
- [ ] 理解内部数据结构设计
- [ ] 理解事件分发的流程
- [ ] 理解线程安全的实现方式

---

## 代码阅读指南

### 从哪里开始读？

建议按以下顺序阅读源码：

#### 1. 头文件 (event_manager.h)

先理解"是什么"，再理解"怎么做"。

```c
// 1. 配置宏 - 了解可配置项
#define EM_MAX_EVENT_TYPES 64

// 2. 枚举类型 - 了解可用的选项
typedef enum { EM_PRIORITY_HIGH, ... } em_priority_t;

// 3. 结构体 - 了解数据的组织方式
typedef struct { ... } em_event_t;

// 4. 函数声明 - 了解可用的操作
em_handle_t em_create(void);
```

#### 2. 创建和销毁 (event_manager.c)

理解资源的生命周期：

```c
em_handle_t em_create(void) {
    // 1. 分配内存
    em_handle_t handle = calloc(1, sizeof(struct em_manager));
    
    // 2. 初始化各个组件
    // - 订阅者列表
    // - 异步队列
    // - 统计信息
    // - 线程同步
    
    return handle;
}

em_error_t em_destroy(em_handle_t handle) {
    // 1. 停止事件循环
    // 2. 清理资源
    // 3. 释放内存
}
```

#### 3. 订阅机制

理解订阅者管理：

```c
em_error_t em_subscribe(...) {
    // 1. 参数验证
    // 2. 加锁
    // 3. 检查重复订阅
    // 4. 找空闲槽位
    // 5. 填充信息
    // 6. 解锁
}
```

#### 4. 事件分发

这是核心逻辑：

```c
static void dispatch_event(...) {
    // 1. 加锁，复制订阅者列表
    // 2. 解锁
    // 3. 执行回调(在锁外)
}
```

**问自己**: 为什么要复制订阅者列表？为什么在锁外执行回调？

### 关键代码片段解析

#### 环形队列

```c
// 入队
queue->tail = (queue->tail + 1) % EM_ASYNC_QUEUE_SIZE;

// 出队
queue->head = (queue->head + 1) % EM_ASYNC_QUEUE_SIZE;
```

用取模运算实现环形，当到达数组末尾时自动回到开头。

#### 延迟排序

```c
// 订阅时
list->sorted = false;  // 只标记，不排序

// 分发时
if (!list->sorted) {
    sort_subscribers(list);  // 需要时才排序
}
```

这是一种优化策略：减少不必要的排序操作。

#### 锁外执行回调

```c
lock_manager(handle);
// 复制需要的数据
em_subscriber_t copy[...];
// ...
unlock_manager(handle);

// 在锁外执行回调
for (...) {
    copy[i].callback(...);  // 回调可能很耗时
}
```

避免在持有锁时执行用户代码，防止死锁。

---

## 动手实验

### 实验1: 添加新事件类型

**目标**: 添加一个"温度告警"事件

1. 定义事件类型:
```c
enum {
    EVENT_TEMP_WARNING = 10  // 温度告警
};
```

2. 创建回调:
```c
void on_temp_warning(em_event_id_t id, em_event_data_t data, void* user) {
    float* temp = (float*)data;
    printf("警告: 温度过高 %.1f°C!\n", *temp);
}
```

3. 订阅和发布:
```c
em_subscribe(em, EVENT_TEMP_WARNING, on_temp_warning, NULL, EM_PRIORITY_HIGH);

float temperature = 85.5f;
em_publish_sync(em, EVENT_TEMP_WARNING, &temperature);
```

### 实验2: 实现简单的状态机

**目标**: 用事件实现一个开关灯的状态机

```c
enum {
    EVENT_TURN_ON = 0,
    EVENT_TURN_OFF = 1
};

typedef struct {
    bool light_on;
} light_state_t;

void on_turn_on(em_event_id_t id, em_event_data_t data, void* user) {
    light_state_t* state = (light_state_t*)user;
    if (!state->light_on) {
        state->light_on = true;
        printf("灯已打开\n");
    }
}

void on_turn_off(em_event_id_t id, em_event_data_t data, void* user) {
    light_state_t* state = (light_state_t*)user;
    if (state->light_on) {
        state->light_on = false;
        printf("灯已关闭\n");
    }
}
```

### 实验3: 添加日志功能

**目标**: 为所有事件添加日志

```c
// 日志回调(订阅所有关心的事件，使用高优先级)
void event_logger(em_event_id_t id, em_event_data_t data, void* user) {
    time_t now = time(NULL);
    printf("[%ld] 事件 %u 被触发\n", now, id);
}

// 注册日志回调
for (int i = 0; i < MY_EVENT_COUNT; i++) {
    em_subscribe(em, i, event_logger, NULL, EM_PRIORITY_HIGH);
}
```

### 实验4: 实现事件过滤

**目标**: 只处理满足条件的事件

```c
typedef struct {
    int threshold;
} filter_context_t;

void filtered_handler(em_event_id_t id, em_event_data_t data, void* user) {
    filter_context_t* ctx = (filter_context_t*)user;
    int* value = (int*)data;
    
    // 只处理超过阈值的事件
    if (*value > ctx->threshold) {
        printf("值 %d 超过阈值 %d\n", *value, ctx->threshold);
    }
}

filter_context_t ctx = { .threshold = 100 };
em_subscribe(em, EVENT_VALUE, filtered_handler, &ctx, EM_PRIORITY_NORMAL);
```

---

## 进阶主题

学完基础后，可以探索以下主题：

### 1. 设计模式

- **观察者模式**: 本项目的核心模式
- **命令模式**: 将请求封装为对象
- **中介者模式**: 事件管理器就是一个中介者

### 2. 并发编程

- **互斥锁 (Mutex)**: 保护共享数据
- **条件变量 (Condition Variable)**: 线程间通知
- **原子操作**: 无锁编程基础
- **死锁**: 原因和避免方法

### 3. 嵌入式系统

- **中断处理**: 硬件事件
- **RTOS 事件**: FreeRTOS, RT-Thread 的事件机制
- **内存约束**: 静态分配 vs 动态分配
- **实时性**: 响应时间要求

### 4. 性能优化

- **缓存友好**: 数据局部性
- **无锁数据结构**: 高并发场景
- **对象池**: 减少动态分配

### 5. 相关项目

- **libuv**: Node.js 的事件循环库
- **libevent**: 高性能事件通知库
- **libev**: 轻量级事件循环库

---

## 常见问题

### Q1: 回调函数中可以发布事件吗？

**可以**，但要注意：
- 同步事件会递归调用，可能栈溢出
- 建议在回调中使用异步发布

### Q2: 为什么要复制订阅者列表？

如果在遍历订阅者列表时，回调函数修改了列表（订阅或取消订阅），会导致迭代出错。

### Q3: data_size 什么时候传 0？

- `data_size = 0`: 只传指针，不复制数据。要求数据在处理前保持有效。
- `data_size > 0`: 复制数据，即使原数据被修改也没关系。

### Q4: 同步和异步怎么选？

| 场景 | 推荐 |
|-----|------|
| 简单、快速的操作 | 同步 |
| 耗时操作 | 异步 |
| 中断处理程序中 | 异步 |
| 需要知道处理结果 | 同步 |
| 多线程环境 | 异步 |

### Q5: 如何调试事件流？

1. 启用调试日志: `-DEM_ENABLE_DEBUG=1`
2. 使用统计信息: `em_get_stats()`
3. 添加日志回调(高优先级)

---

## 推荐资源

### C 语言基础

- 《C Primer Plus》
- 《C和指针》
- 《C语言程序设计现代方法》

### 数据结构

- 《数据结构与算法分析：C语言描述》
- 可视化网站: visualgo.net

### 多线程编程

- 《UNIX环境高级编程》
- 《C++并发编程实战》(概念通用)
- POSIX线程编程教程

### 设计模式

- 《Head First设计模式》
- 《设计模式：可复用面向对象软件的基础》

### 嵌入式系统

- 《嵌入式实时操作系统μC/OS-III》
- FreeRTOS 官方文档

---

## 学习建议

1. **边学边做**: 不要只看不写，动手实践很重要
2. **小步前进**: 一次只学一个概念
3. **多问为什么**: 理解设计背后的原因
4. **写注释**: 用自己的话解释代码
5. **画图**: 画出数据结构和流程图
6. **调试**: 用调试器单步执行，观察变量变化

祝你学习愉快！如有问题，欢迎创建 Issue 讨论。
