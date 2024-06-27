//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_LSDB_H
#define WHY_OSPF_LSDB_H
#include "../message/message.h"
#include "../interface/interface.h"

#include <vector>
#include <cstdint>

class LSDB {
public:
    std::vector<LSARouter*> lsa_routers;
    //用于保护lsa_routers的互斥锁
    pthread_mutex_t router_lock;
    std::vector<LSANetwork*> lsa_networks;
    pthread_mutex_t network_lock;

    //LSA最大存活时间
    uint16_t max_age = 3600;
    //最大洪泛时间差
    uint16_t max_age_diff = 900;
    
    LSDB();
    ~LSDB();
    LSARouter* getRouterLSA(uint32_t link_state_id, uint32_t advertise_router_id);
    LSANetwork* getNetworkLSA(uint32_t link_state_id, uint32_t advertise_router_id);
    LSANetwork* getNetworkLSA(uint32_t link_state_id);

    //对LSA的操作
    void addLSA(char* lsu_lsa_pos);
    void deleteLSA(uint8_t type, uint32_t link_state_id, uint32_t advertise_router_id);
    void floodLSA(LSA* lsa, std::vector<Interface*> interfaces);
};

extern LSDB lsdb;

#endif //WHY_OSPF_LSDB_H