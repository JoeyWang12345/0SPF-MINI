//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_INTERFACE_H
#define WHY_OSPF_INTERFACE_H
#include "../neighbor/neighbor.h"
#include "../../controller/retransmitter/retransmitter.h"

#include <cstdint>
#include <list>

//接口的类型：广播，NBMA，点对点，点对多点，虚拟通道
enum struct NetworkType : uint8_t {
    //别耍小聪明
    PTP = 1,
    BROADCAST,
    NBMA,
    PTMP,
    VIRTUAL
};

//接口状态机
enum struct InterfaceState : uint8_t {
    DOWN = 0,
    LOOPBACK,
    WAITING,
    PTP,
    DR,
    BDR,
    DROTHER
};

class Interface {
public:
    NetworkType networkType = NetworkType::BROADCAST;
    InterfaceState interfaceState = InterfaceState::DOWN;
    //接口名称
    const char* interface_name;
    //in_addr_t就是uint32_t
    in_addr_t ip;
    uint32_t network_mask = 0xffffff00;

    //默认为0号区域
    uint32_t area_id = 0;
    uint32_t hello_interval = 3;
    //不再收到路由器的Hello后40s宣告邻居断开
    uint32_t router_dead_interval = 12;
    //重传的时间戳
    uint32_t rxmt_interval = 5;
    uint32_t router_priority = 1;

    //重传器
    Retransmitter rxmtter = Retransmitter(this);
    
    //默认值
    // uint32_t cost = 1;
    uint32_t cost;
    uint32_t mtu = 1500;

    uint32_t designed_router = 0;
    uint32_t backup_designed_router = 0;

    //邻居列表
    std::list<Neighbor*> neighbors;

    //自身构造函数
    Interface()=default;
    ~Interface();

    //接口事件回调函数
    void upEvent();
    void backupSeenEvent();
    void neighborChangeEvent();
    void waitTimerEvent();
    Neighbor* getNeighbor(in_addr_t source_ip);
    Neighbor* addNeighbor(in_addr_t source_ip);

    //比较邻居
    static bool cmp(Neighbor *n1, Neighbor *n2);
    //选举DR的函数
    void electDR();
};

#endif //WHY_OSPF_INTERFACE_H
