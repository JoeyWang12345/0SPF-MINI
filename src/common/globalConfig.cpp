//
// Created by LEGION on 2024/6/14.
//
#include "globalConfig.h"
#include <arpa/inet.h>

namespace WHYConfig {
    const char* local_name = "ens33";
    uint32_t router_id = ntohl(inet_addr("192.168.206.4"));
    std::vector<Interface*> interfaces;
    std::map<uint32_t, Interface*> iptointerface;

    pthread_attr_t thread_attr;
}
bool exiting = false;
pthread_mutex_t LSA_seq_lock;
size_t LSA_seq_num;//lsa序列号
struct in_addr ip_addr;
