//
// Created by LEGION on 2024/6/14.
//
//旨在提供通用的头文件，只会被.cpp直接引用，不在.h文件中引用
#ifndef WHY_OSPF_COMMON_H
#define WHY_OSPF_COMMON_H

//数据类型相关
#include <cstdint>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include <unistd.h>

//线程相关
#include <pthread.h>

//网络相关
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

//内核相关
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

//全局设置
#include "globalConfig.h"

#endif //WHY_OSPF_COMMON_H