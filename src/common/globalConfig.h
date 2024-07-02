//
// Created by LEGION on 2024/6/14.
//
#include "../model/interface/interface.h"

#include <cstdint>
#include <vector>
#include <map>
#include <pthread.h>

#ifndef WHY_OSPF_GLOBALCONFIG_H
#define WHY_OSPF_GLOBALCONFIG_H

namespace WHYConfig {
    // extern const char* local_name;
    extern uint32_t router_id;
    extern std::vector<Interface*> interfaces;
    extern std::map<uint32_t, Interface*> iptointerface;

    //初始化线程
    extern pthread_attr_t thread_attr;
}
extern bool exiting;
extern pthread_mutex_t LSA_seq_lock;
extern size_t LSA_seq_num;
extern struct in_addr ip_addr;

#endif //WHY_OSPF_GLOBALCONFIG_H