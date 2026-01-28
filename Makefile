# =============================================================================
# 事件管理器 Makefile
# =============================================================================

# 编译器配置
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I$(INC_DIR)
LDFLAGS = -lpthread

# 目录配置
SRC_DIR = src
INC_DIR = include
EXAMPLES_DIR = examples
TESTS_DIR = tests
BUILD_DIR = build

# 源文件
SRCS = $(SRC_DIR)/event_manager.c
OBJS = $(BUILD_DIR)/event_manager.o

# 示例程序
EXAMPLES = $(BUILD_DIR)/basic_example \
           $(BUILD_DIR)/priority_example \
           $(BUILD_DIR)/async_example \
           $(BUILD_DIR)/multithread_example

# 测试程序
TESTS = $(BUILD_DIR)/test_event_manager

# 静态库
LIB = $(BUILD_DIR)/libeventmanager.a

# 调试版本
DEBUG_CFLAGS = $(CFLAGS) -g -O0 -DEM_ENABLE_DEBUG=1
# 发布版本
RELEASE_CFLAGS = $(CFLAGS) -O2

# 默认目标
.PHONY: all
all: $(BUILD_DIR) $(LIB) $(EXAMPLES)

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译静态库
$(LIB): $(OBJS)
	ar rcs $@ $^

# 编译目标文件
$(BUILD_DIR)/event_manager.o: $(SRC_DIR)/event_manager.c $(INC_DIR)/event_manager.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译示例程序
$(BUILD_DIR)/basic_example: $(EXAMPLES_DIR)/basic_example.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/priority_example: $(EXAMPLES_DIR)/priority_example.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/async_example: $(EXAMPLES_DIR)/async_example.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/multithread_example: $(EXAMPLES_DIR)/multithread_example.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 编译测试程序
$(BUILD_DIR)/test_event_manager: $(TESTS_DIR)/test_event_manager.c $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 构建示例
.PHONY: examples
examples: $(BUILD_DIR) $(OBJS) $(EXAMPLES)

# 构建测试
.PHONY: tests
tests: $(BUILD_DIR) $(OBJS) $(TESTS)

# 运行测试
.PHONY: test
test: tests
	@echo "=== 运行测试 ==="
	$(BUILD_DIR)/test_event_manager

# 运行所有示例
.PHONY: run-examples
run-examples: examples
	@echo "=== 运行基础示例 ==="
	$(BUILD_DIR)/basic_example
	@echo ""
	@echo "=== 运行优先级示例 ==="
	$(BUILD_DIR)/priority_example
	@echo ""
	@echo "=== 运行异步示例 ==="
	$(BUILD_DIR)/async_example
	@echo ""
	@echo "=== 运行多线程示例 ==="
	$(BUILD_DIR)/multithread_example

# 调试版本
.PHONY: debug
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean all

# 发布版本
.PHONY: release
release: CFLAGS = $(RELEASE_CFLAGS)
release: clean all

# epoll优化版本(仅Linux)
EPOLL_CFLAGS = -Wall -Wextra -std=c11 -I$(INC_DIR) -DEM_ENABLE_EPOLL=1
.PHONY: epoll
epoll: CFLAGS = $(EPOLL_CFLAGS)
epoll: clean all

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# 安装(到系统目录)
.PHONY: install
install: $(LIB)
	install -d /usr/local/include
	install -d /usr/local/lib
	install -m 644 $(INC_DIR)/event_manager.h /usr/local/include/
	install -m 644 $(LIB) /usr/local/lib/

# 卸载
.PHONY: uninstall
uninstall:
	rm -f /usr/local/include/event_manager.h
	rm -f /usr/local/lib/libeventmanager.a

# 帮助
.PHONY: help
help:
	@echo "事件管理器构建系统"
	@echo ""
	@echo "目标:"
	@echo "  all          - 构建库和所有示例(默认)"
	@echo "  examples     - 只构建示例"
	@echo "  tests        - 构建测试"
	@echo "  test         - 构建并运行测试"
	@echo "  run-examples - 构建并运行所有示例"
	@echo "  debug        - 调试版本(带调试符号和日志)"
	@echo "  release      - 发布版本(优化)"
	@echo "  epoll        - epoll优化版本(仅Linux)"
	@echo "  clean        - 清理构建文件"
	@echo "  install      - 安装到系统目录"
	@echo "  uninstall    - 从系统目录卸载"
	@echo "  help         - 显示此帮助信息"
