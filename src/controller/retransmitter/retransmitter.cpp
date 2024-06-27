#include "../../common/common.h"
#include "retransmitter.h"
#include "../message/messageController.h"

RxmtData::RxmtData(const char* data, size_t data_length, uint8_t type, uint32_t target_ip, uint32_t duration) {
    this->data = data;
    this->length = data_length;
    this->type = type;
    this->target_ip = target_ip;
    this->age = 0;
    this->duration = duration;
}

RxmtData::~RxmtData() {
    
}

Retransmitter::Retransmitter(Interface* interface) {
    local_interface = interface;
    alloc_id = 0;
    pthread_mutex_init(&rxmt_list_lock, NULL);
}

//定期检查和处理待发送的数据包，根据年龄和存活时间决定是否重传
void* Retransmitter::run(void* rxmtter) {
    Retransmitter* retransmitter = (Retransmitter*) rxmtter;
    while (true) {
        //上锁，保证同步
        pthread_mutex_lock(&retransmitter->rxmt_list_lock);

        //遍历数据包列表
        for (auto& rxmt_data : retransmitter->rxmtList) {
            rxmt_data.age++;
            //如果数据包当前年龄等于存活时间
            if (rxmt_data.age == rxmt_data.duration) {
                rxmt_data.age = 0;
                //发送数据包
                sendPacket(rxmt_data.data, rxmt_data.length, rxmt_data.type, rxmt_data.target_ip, retransmitter->local_interface);
            }
        }
        //解锁
        pthread_mutex_unlock(&retransmitter->rxmt_list_lock);
        //休眠一秒
        sleep(1);
    }
}

uint32_t Retransmitter::addRxmtData(RxmtData rxmt_data) {
    rxmt_data.id = alloc_id++;
    pthread_mutex_lock(&rxmt_list_lock);
    rxmtList.push_back(rxmt_data);
    pthread_mutex_unlock(&rxmt_list_lock);
    //打印以便调试
    printf("Add rxmt data: type %d, id %d\n", rxmt_data.type, rxmt_data.id);
    return rxmt_data.id;
}

void Retransmitter::delRxmtData(uint32_t id) {
    pthread_mutex_lock(&rxmt_list_lock);
    for (auto pos = rxmtList.begin(); pos != rxmtList.end(); pos++) {
        if (pos->id == id) {
            printf("Del rxmt data: type %d, id %d\n", pos->type, pos->id);
            rxmtList.erase(pos);
            break;
        }
    }
    pthread_mutex_unlock(&rxmt_list_lock);
}   