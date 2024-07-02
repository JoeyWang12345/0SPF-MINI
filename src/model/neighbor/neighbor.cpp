//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "../../controller/message/messageController.h"
#include "../LSDB/LSDB.h"
#include "../../controller/LSA/LSAController.h"
#include "../../controller/route/routeController.h"

//构造函数
Neighbor::Neighbor(in_addr_t ip, Interface* interface):ip(ip) {
    neighborState = NeighborState::DOWN;
    is_master = false;
    dd_seq_num = 0;
    last_dd_seq_num = 0;
    last_dd_data_len = 0;
    priority = 1;
    local_interface = interface;
    pthread_mutex_init(&lsr_list_lock, NULL);
}

//析构函数
Neighbor::~Neighbor() {
    // lsr_list.shrink_to_fit();
    // db_summary_list.shrink_to_fit();

    for (auto& lsr : lsr_list) {
        delete &lsr;
    }
    for (auto& db_summary : db_summary_list) {
        delete &db_summary;
    }
    for (auto& rxmt : link_state_rxmt_list) {
        delete &rxmt;
    }
}

//初始化邻居的数据库摘要列表
void Neighbor::initDBSummaryList() {
    printf("Init db_summary_list.\n");
    printf("LSDB's lsa_routers size: %d.\n", lsdb.lsa_routers.size());
    printf("LSDB's lsa_networks size: %d.\n", lsdb.lsa_networks.size());
    //确保在访问时线程安全
    pthread_mutex_lock(&lsdb.router_lock);
    for (auto& lsa_router : lsdb.lsa_routers) {
        LSAHeader lsa_header = *((LSAHeader*)lsa_router->toRouterLSA());
        db_summary_list.push_back(lsa_header);
    }
    pthread_mutex_unlock(&lsdb.router_lock);

    //network
    pthread_mutex_lock(&lsdb.network_lock);
    for (auto& lsa_network : lsdb.lsa_networks) {
        LSAHeader lsa_header = *((LSAHeader*)lsa_network->toNetworkLSA());
        db_summary_list.push_back(lsa_header);
    }
    pthread_mutex_unlock(&lsdb.network_lock);
}

void Neighbor::removefromLSRList(uint32_t link_state_id, uint32_t advertise_router_id) {
    pthread_mutex_lock(&lsr_list_lock);
    //迭代找到指向要删除的LSA的迭代器
    for (auto lsa = lsr_list.begin(); lsa != lsr_list.end(); lsa++) {
        if (lsa->link_state_id == link_state_id && lsa->advertising_router == advertise_router_id) {
            lsr_list.erase(lsa);
            printf("Removed one lsa from lsr_list.\n");
            //这里需要break，因为一次只会移除一条LSR
            break;
        }
    }
    //将容量缩减为当前大小，减少内存占用
    lsr_list.shrink_to_fit();
    pthread_mutex_unlock(&lsr_list_lock);
}

//收到Hello报文，触发邻居状态转移
void Neighbor::receiveHelloEvent() {
    printf("Neighbor %x touched receiveHelloEvent ", this->id);
    if (neighborState == NeighborState::DOWN) {
        neighborState = NeighborState::INIT;
        printf("then change from DOWN to INIT.\n");
    }
    else if (neighborState >= NeighborState::INIT) {
        printf("then updated.\n");
    }
    else {
        printf("but rejected.\n");
    }
}

//neighbor发送给interface的Hello报文中没有interface的router_id
//导致状态降级，邻居关系不再有效，需要重新建立
void Neighbor::receive1WayEvent() {
    printf("Neighbor %x touched receive1WayEvent ", this->id);
    if (neighborState >= NeighborState::TWO_WAY) {
        neighborState = NeighborState::INIT;
        printf("then change from TWO-WAY to INIT.\n");
    }
    else {
        printf("but rejected.\n");
    }
}

//互相发送Hello后，进入2Way状态
void Neighbor::receive2WayEvent(Interface* intf) {
    printf("Neighbor %x touched receive2WayEvent ", this->id);
    if (neighborState == NeighborState::INIT) {
        //获取邻居所在接口及其网络类型
        Interface* local_interface = this->local_interface;
        NetworkType interface_type = local_interface->networkType;

        //根据网络类型讨论
        switch (interface_type) {
            //如果是广播或非广播多点可达
            case NetworkType::BROADCAST:
            case NetworkType::NBMA: {
                //检查邻居和自己是否都不是DR或BDR
                // printf("Neighbor's dr: %x.\n", this->designed_router);
                // printf("Neighbor's bdr: %x.\n", this->backup_designed_router);
                if (this->id != this->designed_router && this->id != this->backup_designed_router
                && WHYConfig::router_id != this->designed_router
                && WHYConfig::router_id != this->backup_designed_router) {
                    //将状态设为2WAY，不成临接关系
                    neighborState = NeighborState::TWO_WAY;
                    printf("then change from INIT to 2-WAY.\n");
                    break;
                }
                //最后没有break，会继续向下执行，称为fall through
            }

            default: {
                //形成临接关系
                neighborState = NeighborState::EXSTART;
                printf("then change from INIT to EXSTART.\n");
                //准备开始传送DD空报文
                dd_seq_num = 0;//初始化序列号
                is_master = true;//设置为主路由器
                //创建发送空DD报文的线程
                pthread_create(&send_empty_dd, &WHYConfig::thread_attr, sendEmptyDDPacket, (void*)this);
                break;
            }
        }
    }
    else {
        printf("but rejected.\n");
    }
}

void Neighbor::negotiationDoneEvent() {
    printf("Neighbor %x touched NegotiationDoneEvent ", this->id);
    if (neighborState == NeighborState::EXSTART) {
        neighborState = NeighborState::EXCHANGE;
        printf("then change from EXSTART -> EXCHANGE.\n");
        initDBSummaryList();
    }
    else {
        printf("but rejected.\n");
    }
}

//序列号不匹配事件: 临接被拆除并重新建立
void Neighbor::seqNumberMismatchEvent() {
    printf("Neighbor %x touched SeqNumberMismatchEvent ", this->id);
    if (neighborState >= NeighborState::EXCHANGE) {
        NeighborState prev_state = neighborState;
        //TODO 改变状态，清除列表，发送空DD报文
        neighborState = NeighborState::EXSTART;
        // lsr_list.clear();
        // db_summary_list.clear();
        // link_state_rxmt_list.clear();
        // sendEmptyDDPacket(this);
        printf("then change from EXCHANGE to EXSTART.\n");
    }
    else {
        printf("but rejected.\n");
    }
}

//交换了完整的DD包，每台路由器知道其连接状态数据库中过期的部分
void Neighbor::exchangeDoneEvent() {
    printf("Neighbor %x touched ExchangeDoneEvent ", this->id);
    if (neighborState == NeighborState::EXCHANGE) {
        //邻居的LSR列表为空(所有需要的LSA都已经接收到了)，直接进入FULL
        if (lsr_list.size() == 0) {
            neighborState = NeighborState::FULL;
            printf("then change from EXCHANGE to FULL.\n");
            //生成Router LSA
            generateRouterLSA();
            //如果接口的DR是本接口的IP地址，生成网络LSA
            if (local_interface->designed_router == local_interface->ip) {
                printf("=================================================================\n");
                printf("=======================Generate Network LSA======================\n");
                printf("=================================================================\n");
                generateNetworkLSA(local_interface);
            }

            //洪泛整个LSDB
            printf("Flood LSDB.\n");
            lsdb.floodLSDB(WHYConfig::interfaces);
        }
        //需要从邻居获取LSA，将邻居状态改为LOADING
        else {
            neighborState = NeighborState::LOADING;
            printf("then change from EXCHANGE to LOADING.\n");
            pthread_create(&lsr_send, &WHYConfig::thread_attr, sendLSRPacket, (void*)this);
        }
    }
    else {
            printf("but rejected.n");
    }
}

//接收到的LSR中，含有不在数据库中(无效)的LSA
void Neighbor::badLSReqEvent() {
    printf("Neighbor %x touched badLSReqEvent ", this->id);
    if (neighborState >= NeighborState::EXCHANGE) {
        NeighborState prev_state = neighborState;
        //TODO 改变状态，清除列表，发送空DD报文
        neighborState = NeighborState::EXSTART;
        // lsr_list.clear();
        // db_summary_list.clear();
        // link_state_rxmt_list.clear();
        // sendEmptyDDPacket(this);
        printf("then change from EXCHANGE to EXSTART.\n");
    }
    else {
        printf("but rejected.\n");
    }
}

void Neighbor::loadDoneEvent() {
    printf("Neighbor %x touched loadDoneEvent ", this->id);
    if (neighborState == NeighborState::LOADING) {
        neighborState = NeighborState::FULL;
        printf("then change from LOADING to FULL.\n");
        generateRouterLSA();

        //洪泛LSAB
        printf("Flood LSDB.\n");
        lsdb.floodLSDB(WHYConfig::interfaces);
        //TEST
        lsdb.print();

        //TODO 这里应该需要更新路由表
        routing_table.update();
    }   
    else {
        printf("but rejected.\n");
    }
}