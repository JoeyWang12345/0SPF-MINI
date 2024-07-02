//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "interface.h"
#include "../../controller/LSA/LSAController.h"

#include <algorithm>
#include <list>

//析构函数
Interface::~Interface() {
    for (Neighbor* neighbor : neighbors) {
        delete neighbor;
    }
}

void* wait(void* interface) {
    Interface* intf = (Interface*)interface;
    // sleep(40);
    sleep(12);
    intf->waitTimerEvent();
}

//启动接口
void Interface::upEvent() {
    printf("Interface %x touched upEvent ", this->ip);
    if (interfaceState == InterfaceState::DOWN) {
        //创建线程属性对象并设置其分离态属性，用于创建后续线程
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        //创建定时器线程和重发器线程
        pthread_t timer_thread;
        pthread_t rxmt_thread;
        pthread_create(&timer_thread, &attr, wait, this);
        pthread_create(&rxmt_thread, &attr, Retransmitter::run, (void*)&(this->rxmtter));

        //修改状态为WAITING
        interfaceState = InterfaceState::WAITING;
        printf("then change from DOWN to WAITING.\n");
    }
    else {
        printf("but rejected.\n");
    }
}

//等待计时器到期(选举DR和BDR之前的等待状态结束)
void Interface::waitTimerEvent() {
    printf("Interface %x touched WaitTimerEvent ", this->ip);
    if (interfaceState == InterfaceState::WAITING) {
        electDR();
        if (ip == designed_router) {
            interfaceState = InterfaceState::DR;
            printf("then change from WAITTING to DR.\n");
        }
        else if (ip == backup_designed_router) {
            interfaceState = InterfaceState::BDR;
            printf("then change from WAITTING to BDR.\n");
        }
        else {
            interfaceState = InterfaceState::DROTHER;
            printf("then change from WAITTING to DROTHER.\n");
        }

        generateRouterLSA();
    }
    else {
        printf("but rejected.\n");
    }
}

//路由器已经探知网络上是否存在BDR，会结束等待状态
void Interface::backupSeenEvent() {
    printf("Interface %x touched backupSeenEvent ", this->ip);
    printf("=================================================================\n");
    printf("=======================Backup Has Seen Event=====================\n");
    printf("=================================================================\n");
    if (interfaceState == InterfaceState::WAITING) {
        //此时需要选举DR
        electDR();
        if (ip == designed_router) {
            interfaceState = InterfaceState::DR;
            printf("then change from WAITING to DR.\n");
        }
        else if (ip == backup_designed_router) {
            interfaceState = InterfaceState::BDR;
            printf("then change from WAITING to BDR.\n");
        }
        else {
            interfaceState = InterfaceState::DROTHER;
            printf("then change from WAITING to DROTHER.\n");
        }

        //生成路由器LSA
        generateRouterLSA();
    }
    else {
        printf("but rejected.\n");
    }
}

//邻居发生改变
void Interface::neighborChangeEvent() {
    printf("Interface %x touched neighborChange ", this->ip);
    printf("\n=================================================================\n");
    printf("=======================Neighbor Change Event=====================\n");
    printf("=================================================================\n");
    //只有当接口状态为DR/BDR/DROTHER时才行
    if (interfaceState == InterfaceState::DR || interfaceState == InterfaceState::BDR || interfaceState == InterfaceState::DROTHER) {
        //重新选举DR
        electDR();
        if (ip == designed_router) {
            interfaceState = InterfaceState::DR;
            printf("then change from WAITING to DR.\n");
        }
        else if (ip == backup_designed_router) {
            interfaceState = InterfaceState::BDR;
            printf("then change from WAITING to BDR.\n");
        }
        else {
            interfaceState = InterfaceState::DROTHER;
            printf("then change from WAITING to DROTHER.\n");
        }

        //生成路由器LSA
        generateRouterLSA();
    }
    else {
        printf("but rejected.\n");
    }
}

//根据IP地址获取邻居
Neighbor* Interface::getNeighbor(in_addr_t source_ip) {
    for (auto& neighbor : neighbors) {
        if (neighbor->ip == source_ip) {
            return neighbor;
        }
    }
    return nullptr;
}

//添加到邻居列表中
Neighbor* Interface::addNeighbor(in_addr_t source_ip) {
    Neighbor* neighbor = new Neighbor(source_ip, this);
    neighbors.push_back(neighbor);
    return neighbor;
}

bool Interface::cmp(Neighbor *n1, Neighbor *n2) {
    if (n1->priority == n2->priority) {
        return n1->id > n2->id;
    }
    return n1->priority > n2->priority;
}

//选举DR和BDR
void Interface::electDR() {
    printf("\nstart to elect DR and BDR...\n");

    //创建候选者列表，并把自身加进去
    std::list<Neighbor*> candidates;
    Neighbor self(this->ip, this);
    self.id = WHYConfig::router_id;
    self.designed_router = this->designed_router;
    self.backup_designed_router = this->backup_designed_router;
    self.priority = this->router_priority;
    candidates.push_back(&self);

    //将符合条件的邻居加入候选人列表
    for (auto& neighbor : neighbors) {
        //状态至少为2WAY，且优先级不为零
        if (static_cast<uint8_t>(neighbor->neighborState) >= static_cast<uint8_t>(NeighborState::TWO_WAY) && neighbor->priority != 0) {
            candidates.push_back(neighbor);
            //TEST
            printf("neighbor: %x\n", neighbor->id);
        }
    }
    //TEST
    printf("candidates's num: %d\n", candidates.size());
    printf("local interface's id: %x.\n", self.id);
    printf("local dr: %x.\n", self.designed_router);
    printf("local bdr: %x.\n", self.backup_designed_router);

    //初始化DR和BDR
    Neighbor* dr = nullptr;
    Neighbor* bdr = nullptr;

    //选举BDR
    std::vector<Neighbor*> candidates_bdr1;
    std::vector<Neighbor*> candidates_bdr2;
    for (auto& neighbor : candidates) {
        //只要没有声称自己为DR
        if (neighbor->designed_router != neighbor->id) {
            candidates_bdr2.push_back(neighbor);
            //还应该声称自己为BDR
            if (neighbor->backup_designed_router == neighbor->id) {
                candidates_bdr1.push_back(neighbor);
            } 
        }
    }
    //优先从这里选
    if (candidates_bdr1.size()) {
        std::sort(candidates_bdr1.begin(), candidates_bdr1.end(), cmp);
        bdr = candidates_bdr1[0];
    }
    else if (candidates_bdr2.size()) {
        std::sort(candidates_bdr2.begin(), candidates_bdr2.end(), cmp);
        bdr = candidates_bdr2[0];
    }
    
    //选举DR
    std::vector<Neighbor*> candidates_dr;
    for (auto& neighbor : candidates) {
        //需要宣称自己为DR
        if (neighbor->designed_router == neighbor->id) {
            candidates_dr.push_back(neighbor);
        }
    }
    if (candidates_dr.size()) {
        //按照规则排序，取第一个
        std::sort(candidates_dr.begin(), candidates_dr.end(), cmp);
        dr = candidates_dr[0];
    }
    else {
        dr = bdr;
    }

    //TODO?

    //检查当前接口是否成为了新的DR
    if (dr->ip == this->ip && this->designed_router != this->ip) {
        //更新网络拓扑信息
        printf("=================================================================\n");
        printf("=======================Generate Network LSA======================\n");
        printf("=================================================================\n");
        generateNetworkLSA(this);
    }

    //更新
    this->designed_router = dr->ip;
    this->backup_designed_router = bdr->ip;

    printf("designed_router is: %x now.\n", this->designed_router);
    printf("backup_designed_router is %x now.\n", this->backup_designed_router);
}