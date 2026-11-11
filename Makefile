CXX           ?= g++
CXXFLAGS_BASE := -std=c++20 -Iinclude -Ithird_party/sys_shim -Wall -Wextra -Wno-unused-parameter -MMD -MP -pthread

# Auto-detect whether standard -dev linker symlinks exist or fall back to multiarch runtime .so paths
HAS_DEV_LIBS := $(shell printf 'int main(){return 0;}\n' | $(CXX) -x c++ - -lsqlite3 -lcurl -lssl -lcrypto -lz -o /dev/null 2>/dev/null && echo 1 || echo 0)

ifeq ($(HAS_DEV_LIBS),1)
    LDLIBS := -lsqlite3 -lcurl -lssl -lcrypto -lz -lpthread
else
    MULTIARCH_DIR := $(firstword $(wildcard /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /lib/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu))
    LDLIBS := \
        $(MULTIARCH_DIR)/libsqlite3.so.0 \
        $(MULTIARCH_DIR)/libcurl.so.4 \
        $(MULTIARCH_DIR)/libssl.so.3 \
        $(MULTIARCH_DIR)/libcrypto.so.3 \
        $(MULTIARCH_DIR)/libz.so.1 \
        -lpthread
endif

RELEASE_FLAGS   := -O3 -flto=auto -DNDEBUG -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2
RELEASE_LDFLAGS := -pie -Wl,-z,relro,-z,now
ASAN_FLAGS      := -O1 -g1 -fno-omit-frame-pointer -fsanitize=address,undefined
TSAN_FLAGS      := -O1 -g1 -fno-omit-frame-pointer -fsanitize=thread

CORE_NAMES := \
    json xml crypto security config db http_client \
    drivers_local drivers_s3_webdav drivers_cloud_sftp drivers_virtual_crypt \
    vfs archive tasks http_server webdav_server api_routes

BUILD_DIR := build
BIN       := alist

REL_OBJS   := $(patsubst %,$(BUILD_DIR)/rel/%.o,$(CORE_NAMES) main)
TEST_OBJS  := $(patsubst %,$(BUILD_DIR)/test/%.o,$(CORE_NAMES)) $(BUILD_DIR)/test/test_main.o
ASAN_OBJS  := $(patsubst %,$(BUILD_DIR)/asan/%.o,$(CORE_NAMES)) $(BUILD_DIR)/asan/test_main.o
TSAN_OBJS  := $(patsubst %,$(BUILD_DIR)/tsan/%.o,$(CORE_NAMES)) $(BUILD_DIR)/tsan/test_main.o
BENCH_OBJS := $(patsubst %,$(BUILD_DIR)/bench/%.o,$(CORE_NAMES)) $(BUILD_DIR)/bench/benchmark.o

.PHONY: all release test asan tsan bench clean

all: release

release: $(BIN)

$(BUILD_DIR)/rel/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(RELEASE_FLAGS) -c $< -o $@

$(BIN): $(REL_OBJS)
	$(CXX) $(CXXFLAGS_BASE) $(RELEASE_FLAGS) $(REL_OBJS) $(RELEASE_LDFLAGS) $(LDLIBS) -s -o $(BIN)

# Fast uninstrumented test binary
$(BUILD_DIR)/test/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) -O2 -g1 -c $< -o $@

$(BUILD_DIR)/test/test_main.o: tests/test_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) -O2 -g1 -c $< -o $@

$(BUILD_DIR)/alist_tests: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS_BASE) -O2 -g1 $(TEST_OBJS) $(LDLIBS) -o $@

test: $(BUILD_DIR)/alist_tests
	./$(BUILD_DIR)/alist_tests

# AddressSanitizer + UndefinedBehaviorSanitizer
$(BUILD_DIR)/asan/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(ASAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/asan/test_main.o: tests/test_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(ASAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/alist_tests_asan: $(ASAN_OBJS)
	$(CXX) $(CXXFLAGS_BASE) $(ASAN_FLAGS) $(ASAN_OBJS) $(LDLIBS) -o $@

asan: $(BUILD_DIR)/alist_tests_asan
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ./$(BUILD_DIR)/alist_tests_asan

# ThreadSanitizer
$(BUILD_DIR)/tsan/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(TSAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/tsan/test_main.o: tests/test_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(TSAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/alist_tests_tsan: $(TSAN_OBJS)
	$(CXX) $(CXXFLAGS_BASE) $(TSAN_FLAGS) $(TSAN_OBJS) $(LDLIBS) -o $@

tsan: $(BUILD_DIR)/alist_tests_tsan
	TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 ./$(BUILD_DIR)/alist_tests_tsan

# Benchmark suite
$(BUILD_DIR)/bench/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(RELEASE_FLAGS) -c $< -o $@

$(BUILD_DIR)/bench/benchmark.o: tests/benchmark.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_BASE) $(RELEASE_FLAGS) -c $< -o $@

$(BUILD_DIR)/alist_bench: $(BENCH_OBJS)
	$(CXX) $(CXXFLAGS_BASE) $(RELEASE_FLAGS) $(BENCH_OBJS) $(RELEASE_LDFLAGS) $(LDLIBS) -o $@

bench: $(BUILD_DIR)/alist_bench
	./$(BUILD_DIR)/alist_bench

clean:
	rm -rf $(BUILD_DIR) $(BIN)

-include $(REL_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(ASAN_OBJS:.o=.d) $(TSAN_OBJS:.o=.d) $(BENCH_OBJS:.o=.d)
