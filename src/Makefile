include ../config.mk

# 使用find命令递归查找所有的.cpp文件
SRCS = $(shell find . -name '*.cpp')

# 将所有.cpp文件对应生成的.o文件存储在OBJS中
OBJS = $(SRCS:.cpp=.o)

# 伪标签
.PHONY: all clean

# 最终目标是生成可执行文件
all: $(TARGET)

# 编译规则
%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 链接所有对象文件生成最终的可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $(TARGET)

# 清理操作，删除生成的目标文件和可执行文件
clean:
	rm -rf $(TARGET) $(OBJS)