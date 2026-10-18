CXX ?= g++
RM  := rm -rf

APP      := mtg
TEST_APP := mtg-tests
SRC_DIR  := src
BUILD    := build
MODE     ?= debug
OBJ_DIR  := $(BUILD)/obj/$(MODE)
BIN_DIR  := $(BUILD)/bin

CORE_SRCS := $(shell find $(SRC_DIR)/core -name '*.cpp' -print)
CLI_SRCS  := $(shell find $(SRC_DIR)/cli -name '*.cpp' -print)
TEST_SRCS := $(shell find tests -maxdepth 1 -name '*.cpp' -print)

CORE_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CLI_OBJS  := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CLI_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))
DEPS      := $(CORE_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

PKG_CFLAGS := $(shell pkg-config --cflags sqlite3 libcurl liblzma 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs sqlite3 libcurl liblzma 2>/dev/null)

ifeq ($(strip $(PKG_LIBS)),)
  PKG_LIBS := -lsqlite3 -lcurl -llzma
endif

STD  := -std=c++20
WARN := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnull-dereference
DEP  := -MMD -MP
INC  := -I$(SRC_DIR)

COMMON_CXXFLAGS := $(STD) $(WARN) $(DEP) $(INC) $(PKG_CFLAGS)

ifeq ($(MODE),debug)
  CXXFLAGS := $(COMMON_CXXFLAGS) -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS  := -fsanitize=address,undefined
else ifeq ($(MODE),release)
  CXXFLAGS := $(COMMON_CXXFLAGS) -O2 -DNDEBUG
  LDFLAGS  :=
else
  $(error Unknown MODE=$(MODE). Use debug or release)
endif

LDLIBS := $(PKG_LIBS)

.PHONY: all debug release clean run test format

all: $(BIN_DIR)/$(APP)

debug:
	@$(MAKE) MODE=debug all

release:
	@$(MAKE) MODE=release all

$(BIN_DIR)/$(APP): $(CORE_OBJS) $(CLI_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/$(TEST_APP): $(CORE_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	$(BIN_DIR)/$(APP)

test: $(BIN_DIR)/$(TEST_APP)
	$(BIN_DIR)/$(TEST_APP)

format:
	clang-format -i $$(find src tests -name '*.cpp' -o -name '*.hpp')

clean:
	$(RM) $(BUILD)

-include $(DEPS)
