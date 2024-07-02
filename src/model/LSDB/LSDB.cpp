//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "../message/message.h"
#include "LSDB.h"
#include "../../controller/message/messageController.h"

LSDB::LSDB() {
    pthread_mutex_init(&router_lock, NULL);
    pthread_mutex_init(&network_lock, NULL);
}

LSDB::~LSDB() {
    for (LSARouter* lsa : lsa_routers) {
        delete lsa;
    }
    for (LSANetwork* lsa : lsa_networks) {
        delete lsa;
    }
}

//从LSDB获取RouterLSA
LSARouter* LSDB::getRouterLSA(uint32_t link_state_id, uint32_t advertise_router_id) {
    for (LSARouter* lsa_router : lsa_routers) {
        if (lsa_router->lsaHeader.link_state_id == link_state_id
        && lsa_router->lsaHeader.advertising_router == advertise_router_id) {
            return lsa_router;
        }
    }
    return nullptr;
}

LSANetwork* LSDB::getNetworkLSA(uint32_t link_state_id, uint32_t advertise_router_id) {
    for (LSANetwork* lsa_network : lsa_networks) {
        if (lsa_network->lsaHeader.link_state_id == link_state_id
        && lsa_network->lsaHeader.advertising_router == advertise_router_id) {
            return lsa_network;
        }
    }
    return nullptr;
}

LSANetwork* LSDB::getNetworkLSA(uint32_t link_state_id) {
    for (LSANetwork* lsa_network : lsa_networks) {
        if (lsa_network->lsaHeader.link_state_id == link_state_id) {
            return lsa_network;
        }
    }
    return nullptr;
}

//将新的LSA添加到LSDB中
void LSDB::addLSA(char* lsu_lsa_pos) {
    LSAHeader* header = (LSAHeader*)lsu_lsa_pos;
    if (header->ls_type == ROUTER) {
        LSARouter* lsa_router = new LSARouter(lsu_lsa_pos);
        //查找是否存在相同的LSA
        LSARouter* lsa_check = getRouterLSA(lsa_router->lsaHeader.link_state_id, lsa_router->lsaHeader.advertising_router);
        pthread_mutex_lock(&router_lock);
        //删除原有的LSA，将新的加入列表
        if (lsa_check) {
            for (auto lsa_pos = lsa_routers.begin(); lsa_pos != lsa_routers.end(); lsa_pos++) {
                if (*lsa_pos == lsa_check) {
                    lsa_routers.erase(lsa_pos);
                    delete lsa_check;
                    break;   
                }
            }
        }
        lsa_routers.push_back(lsa_router);
        pthread_mutex_unlock(&router_lock);
    }
    else if (header->ls_type == NETWORK) {
        LSANetwork* lsa_network = new LSANetwork(lsu_lsa_pos);
        LSANetwork* lsa_check = getNetworkLSA(lsa_network->lsaHeader.link_state_id, lsa_network->lsaHeader.advertising_router);
        pthread_mutex_lock(&network_lock);
        if (lsa_check) {
            for (auto lsa_pos = lsa_networks.begin(); lsa_pos != lsa_networks.end(); lsa_pos++) {
                if (*lsa_pos == lsa_check) {
                    lsa_networks.erase(lsa_pos);
                    delete lsa_check;
                    break;
                }
            }
        }
        lsa_networks.push_back(lsa_network);
        pthread_mutex_unlock(&network_lock);
    }
}

//根据三要素从LSDB中删除某一LSA
void LSDB::deleteLSA(uint8_t type, uint32_t link_state_id, uint32_t advertise_router_id) {
    if (type == ROUTER) {
        pthread_mutex_lock(&router_lock);
        //lsa_routers里面存的本来就是LSARouter*，指针类型，所以迭代器是指针的指针
        for (auto lsa_router = lsa_routers.begin(); lsa_router != lsa_routers.end(); lsa_router++) {
            if ((*lsa_router)->lsaHeader.link_state_id == link_state_id
            && (*lsa_router)->lsaHeader.advertising_router == advertise_router_id) {
                lsa_routers.erase(lsa_router);
                break;
            }
        }
        pthread_mutex_unlock(&router_lock);
    }
    else if (type == NETWORK) {
        pthread_mutex_lock(&network_lock);
        for (auto lsa_network = lsa_networks.begin(); lsa_network != lsa_networks.end(); lsa_network++) {
            if ((*lsa_network)->lsaHeader.link_state_id == link_state_id
            && (*lsa_network)->lsaHeader.advertising_router == advertise_router_id) {
                lsa_networks.erase(lsa_network);
                break;
            }
        }
        pthread_mutex_unlock(&network_lock);
    }
}

//将LSA洪泛到指定的接口列表上
void LSDB::floodLSA(LSA* lsa, std::vector<Interface*> interfaces) {
    //利用LSU携带LSA数据包
    char* lsu = (char*)malloc(1024);
    OSPFLSU* lsu_body = (OSPFLSU*)lsu;
    memset(lsu_body, 0, sizeof(OSPFLSU));
    //设置LSA数目为1
    lsu_body->LSA_num = htonl(1);

    //附加在LSU后面的LSA
    char* lsu_lsa = lsu + sizeof(OSPFLSU);
    //将LSA转换为字节包
    char* lsa_packet;
    if (lsa->lsaHeader.ls_type == ROUTER) {
        lsa_packet = ((LSARouter*)lsa)->toRouterLSA();
    }
    else if (lsa->lsaHeader.ls_type == NETWORK) {
        lsa_packet = ((LSANetwork*)lsa)->toNetworkLSA();
    }
    memcpy(lsu_lsa, lsa_packet, lsa->size());
    delete[] lsa_packet;

    //发送LSU包
    for (Interface* interface : interfaces) {
        if (interface->interfaceState == InterfaceState::DROTHER
        || interface->interfaceState == InterfaceState::BDR
        || interface->interfaceState == InterfaceState::PTP) {
            sendPacket(lsu, sizeof(OSPFLSU) + lsa->size(), LSU, ntohl(inet_addr("224.0.0.5")), interface);
        }
    }

    free(lsu);
}

//将LSDB洪泛到指定的接口列表上
void LSDB::floodLSDB(std::vector<Interface*> interfaces) {
    char* lsu = (char*)malloc(2048);
    OSPFLSU* lsu_body = (OSPFLSU*)lsu;
    memset(lsu_body, 0, sizeof(OSPFLSU));
    //计算LSA数目
    printf("LSA's num: %d.\n", lsu_body->LSA_num);
    lsu_body->LSA_num = htonl(lsa_routers.size() + lsa_networks.size());

    char* lsu_lsa = lsu + sizeof(OSPFLSU);
    char* lsa_packet;
    for (LSARouter* lsa_router : lsa_routers) {
        lsa_packet = ((LSARouter*)lsa_router)->toRouterLSA();
        memcpy(lsu_lsa, lsa_packet, lsa_router->size());
        lsu_lsa += lsa_router->size();
    }
    for (LSANetwork* lsa_network : lsa_networks) {
        lsa_packet = ((LSANetwork*)lsa_network)->toNetworkLSA();
        memcpy(lsu_lsa, lsa_packet, lsa_network->size());
        lsu_lsa += lsa_network->size();
    }
    delete[] lsa_packet;

    //发送LSU包
    for (Interface* interface : interfaces) {
        if (interface->interfaceState == InterfaceState::DROTHER
        || interface->interfaceState == InterfaceState::BDR
        || interface->interfaceState == InterfaceState::PTP) {
            sendPacket(lsu, sizeof(OSPFLSU) + (lsu_lsa - lsu), LSU, ntohl(inet_addr("224.0.0.5")), interface);
        }
    }
    free(lsu);
}

LSDB LSDB::deepClone() {
    LSDB copy;
    //复制lsa_routers
    for (const auto& lsa_router : lsa_routers) {
        copy.lsa_routers.push_back(new LSARouter(*lsa_router));
    }
    //复制network_routers
    for (const auto& lsa_network : lsa_networks) {
        copy.lsa_networks.push_back(new LSANetwork(*lsa_network));
    }

    return copy;
}

//打印本路由器的lsdb
void LSDB::print() {
    for (LSARouter* lsa_router : lsa_routers) {
        lsa_router->print();
    }
    for (LSANetwork* lsa_network : lsa_networks) {
        lsa_network->print();
    }
}