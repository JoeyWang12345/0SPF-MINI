# 使用C++编译器
CXX = g++

# 指定编译选项
CXXFLAGS = -Wall -Wextra -std=c++11

# 指定链接选项(启用POSIX线程库支持)
LDFLAGS = -pthread

# 最终可执行文件名称
TARGET = WHY_ospf