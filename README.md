# C 事件管理器 (Event Manager)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

一个专为嵌入式系统设计的轻量级 C 语言事件管理器，支持多线程、优先级和同步/异步事件处理。

## ✨ 特性

- 🔒 **多线程安全** - 使用互斥锁保护关键数据结构
- 🎯 **事件优先级** - 支持高、普通、低三级优先级
- ⚡ **同步/异步** - 灵活选择事件处理模式
- 📦 **轻量级** - 适合资源受限的嵌入式环境
- 🔧 **可配置** - 通过宏定义调整资源使用
- 📚 **完整文档** - 详细的 API 文档和学习指南

## 🚀 快速开始

### 编译

```bash
# 编译库和示例
make

# 运行测试
make test

# 运行所有示例
make run-examples
```

### 基本用法

```c
#include "event_manager.h"

// 定义事件类型
enum {
    EVENT_BUTTON_PRESS = 0,
    EVENT_SENSOR_DATA = 1
};

// 事件回调函数
void on_button(em_event_id_t id, em_event_data_t data, void* user) {
    printf("按钮被按下!\n");
}

int main() {
    // 创建事件管理器
    em_handle_t em = em_create();
    
    // 订阅事件
    em_subscribe(em, EVENT_BUTTON_PRESS, on_button, NULL, EM_PRIORITY_NORMAL);
    
    // 发布同步事件
    em_publish_sync(em, EVENT_BUTTON_PRESS, NULL);
    
    // 销毁
    em_destroy(em);
    return 0;
}
```

## 📖 文档

- [API 参考](docs/API.md) - 完整的 API 文档
- [架构设计](docs/ARCHITECTURE.md) - 内部实现原理
- [学习指南](docs/LEARNING_GUIDE.md) - 从基础到进阶的学习路径

## 📁 项目结构

```
.
├── include/
│   └── event_manager.h     # 头文件(API定义)
├── src/
│   └── event_manager.c     # 实现代码
├── examples/
│   ├── basic_example.c     # 基础示例
│   ├── priority_example.c  # 优先级示例
│   ├── async_example.c     # 异步事件示例
│   └── multithread_example.c # 多线程示例
├── tests/
│   └── test_event_manager.c # 单元测试
├── docs/
│   ├── API.md              # API文档
│   ├── ARCHITECTURE.md     # 架构文档
│   └── LEARNING_GUIDE.md   # 学习指南
├── Makefile                # 构建脚本
└── README.md               # 本文件
```

## ⚙️ 配置选项

可以在编译时通过定义宏来调整配置：

| 宏 | 默认值 | 描述 |
|---|---|---|
| `EM_MAX_EVENT_TYPES` | 64 | 最大事件类型数量 |
| `EM_MAX_SUBSCRIBERS` | 16 | 每种事件最大订阅者数 |
| `EM_ASYNC_QUEUE_SIZE` | 32 | 异步事件队列大小 |
| `EM_ENABLE_THREADING` | 1 | 是否启用多线程支持 |
| `EM_ENABLE_DEBUG` | 0 | 是否启用调试日志 |
| `EM_ENABLE_EPOLL` | 0 | 是否启用 epoll 优化(仅 Linux) |

### epoll 优化

在 Linux 系统上，可以启用 epoll 来优化事件循环的性能。epoll 使用 `eventfd` 作为补充通知机制，
可以提高事件循环在空闲时的唤醒效率。

**启用条件：**
- 仅在 Linux 系统上可用
- 需要同时启用 `EM_ENABLE_THREADING=1`

**使用方式：**
```bash
gcc -DEM_ENABLE_EPOLL=1 -DEM_ENABLE_THREADING=1 ...
```

示例:
```bash
gcc -DEM_MAX_EVENT_TYPES=128 -DEM_ENABLE_DEBUG=1 ...
```

## 🧪 测试

```bash
# 运行所有测试
make test

# 带调试信息编译
make debug
make test
```

## 📋 示例

### 基础示例
展示事件管理器的基本用法：创建、订阅、发布、销毁。

```bash
make examples
./build/basic_example
```

### 优先级示例
展示订阅者优先级和事件优先级的使用。

```bash
./build/priority_example
```

### 异步事件示例
展示同步和异步事件的区别，以及异步队列的管理。

```bash
./build/async_example
```

### 多线程示例
展示多线程环境下的安全使用。

```bash
./build/multithread_example
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

---

如有问题或建议，请创建 Issue。祝学习愉快！🎉
