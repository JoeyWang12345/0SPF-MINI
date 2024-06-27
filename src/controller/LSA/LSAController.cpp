//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "LSAController.h"
#include "../../model/interface/interface.h"
#include "../../model/LSDB/LSDB.h"

void generateRouterLSA() {
    printf("Generate a Router LSA.\n");
    //生成新的RouterLSA，利用配置中的接口信息
    LSARouter* lsa_router = genRouterLSA(WHYConfig::interfaces);
    //获取当前路由器的Router LSA
    LSARouter* lsa_check = lsdb.getRouterLSA(WHYConfig::router_id, WHYConfig::router_id);
    //标志是否插入到LSDB中
    bool flag = false;
    //LSDB中不存在
    if (!lsa_check) {
        printf("new Router LSA, insert into lsdb.\n");
        pthread_mutex_lock(&lsdb.router_lock);
        lsdb.lsa_routers.push_back(lsa_router);
        pthread_mutex_unlock(&lsdb.router_lock);
    }
    //LSDB中存在，比较谁更新
    else if (*lsa_router > * lsa_check) {
        printf("newer Router LSA, insert into lsdb and delete the former.\n");
        lsdb.deleteLSA(ROUTER, lsa_check->lsaHeader.link_state_id, lsa_check->lsaHeader.advertising_router);
        pthread_mutex_lock(&lsdb.router_lock);
        lsdb.lsa_routers.push_back(lsa_router);
        pthread_mutex_unlock(&lsdb.router_lock);
        flag = true;
    }

    //若添加了新的LSA，就进行洪泛
    if (flag) { 
        printf("Flood LSA.\n");
        lsdb.floodLSA(lsa_router, WHYConfig::interfaces);
    }
}

LSARouter* genRouterLSA(std::vector<Interface*> interfaces) {
    LSARouter* lsa_router = new LSARouter();
    lsa_router->lsaHeader.ls_type = 1;
    lsa_router->lsaHeader.advertising_router = WHYConfig::router_id;
    lsa_router->lsaHeader.link_state_id = WHYConfig::router_id;
    pthread_mutex_lock(&LSA_seq_lock);
    //利用序号计数器完成标号并自增
    lsa_router->lsaHeader.ls_sequence_number += LSA_seq_num;
    LSA_seq_num++;
    pthread_mutex_unlock(&LSA_seq_lock);

    //遍历每个接口
    for (Interface* interface : interfaces) {
        //跳过未启动的
        if (interface->interfaceState == InterfaceState::DOWN) {
            continue;
        }

        //设置路由器连接
        LSARouterLink lsa_router_link;
        lsa_router_link.metric = interface->cost;
        lsa_router_link.tos_num = 0;

        //检查接口状态和DR信息
        //接口状态不能是WAITING(在等待选举DR和BDR，不能生成Transit链路)
        //DR是自己-可以生成Transit链路
        //邻居DR状态为FULL，表示邻居完全同步了LSDB-可以生成Transit链路
        if ((interface->interfaceState != InterfaceState::WAITING)
        && (interface->designed_router == interface->ip
        || interface->getNeighbor(interface->designed_router)->neighborState == NeighborState::FULL)) {
            lsa_router_link.type = TRANSIT;
            lsa_router_link.link_id = interface->designed_router;
            lsa_router_link.link_data = interface->ip;
        }
        else {
            lsa_router_link.type = STUB;
            lsa_router_link.link_id = interface->ip & interface->network_mask;
            lsa_router_link.link_data = interface->network_mask;
        }
        lsa_router->LSARouterLinks.push_back(lsa_router_link);
    }
    lsa_router->link_num = lsa_router->LSARouterLinks.size();
    return lsa_router;
}

void generateNetworkLSA(Interface* interface) {
    printf("Generate a Network LSA.\n");
    LSANetwork* lsa_network = genNetworkLSA(interface);
    LSANetwork* lsa_check = lsdb.getNetworkLSA(WHYConfig::router_id, interface->ip);
    bool flag = false;
    
    if (!lsa_check) {
        printf("new Network LSA, insert into lsdb.\n");
        pthread_mutex_lock(&lsdb.network_lock);
        lsdb.lsa_networks.push_back(lsa_network);
        pthread_mutex_unlock(&lsdb.network_lock);
        flag = true;
    }
    else if (*lsa_network > *lsa_check) {
        printf("newer Network LSA, insert into lsdb and delete the former.\n");
        lsdb.deleteLSA(NETWORK, lsa_check->lsaHeader.link_state_id, lsa_check->lsaHeader.advertising_router);
        pthread_mutex_lock(&lsdb.network_lock);
        lsdb.lsa_networks.push_back(lsa_network);
        pthread_mutex_unlock(&lsdb.network_lock);
        flag = true;
    }

    if (flag) {
        printf("Flood LSA.\n");
        lsdb.floodLSA(lsa_network, WHYConfig::interfaces);
    }
}

LSANetwork* genNetworkLSA(Interface* interface) {
    LSANetwork* lsa_network = new LSANetwork();
    lsa_network->lsaHeader.ls_type = 2;
    lsa_network->lsaHeader.advertising_router = WHYConfig::router_id;
    lsa_network->lsaHeader.link_state_id = interface->ip;
    pthread_mutex_lock(&LSA_seq_lock);
    lsa_network->lsaHeader.ls_sequence_number += LSA_seq_num;
    LSA_seq_num++;
    pthread_mutex_unlock(&LSA_seq_lock);

    //只有和DR达到完全临接(FULL)的路由器才被放入
    for (Neighbor* neighbor : interface->neighbors) {
        if (neighbor->neighborState == NeighborState::FULL) {
            lsa_network->attached_routers.push_back(neighbor->ip);
        }
    }
    return lsa_network;
}