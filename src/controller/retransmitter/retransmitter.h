#ifndef RETRANSMITTER_H
#define RETRANSMITTER_H

#include <vector>
#include <cstdint>
#include <pthread.h>

//不能下面这样，因为会产生循环依赖，需要前向声明
// #include "../../model/interface/interface.h"
class Interface;

//需要重传的数据包信息(OSPF的数据部分)
struct RxmtData {
    const char* data;
    size_t length;
    uint8_t type;
    uint32_t target_ip;
    uint32_t id;
    uint32_t age;
    uint32_t duration;

    RxmtData(const char* data, size_t length, uint8_t type, uint32_t target_ip, uint32_t duration);
    ~RxmtData();
};

class Retransmitter {
public:
    //与重传器关联的接口
    Interface* local_interface;

    //所有需要重传的数据包
    std::vector<RxmtData> rxmtList;
    
    //保护list的互斥锁
    pthread_mutex_t rxmt_list_lock;

    //分配给每个数据包的唯一ID
    uint32_t alloc_id;

    Retransmitter(Interface* interface);
    uint32_t addRxmtData(RxmtData rxmtData);
    void delRxmtData(uint32_t id);
    //处理数据包重传逻辑
    static void* run(void* rxmtter);
};

#endif