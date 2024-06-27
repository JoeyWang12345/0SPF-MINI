//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_NEIGHBOR_H
#define WHY_OSPF_NEIGHBOR_H
#include "../message/message.h"

#include <cstdint>
#include <deque>
#include <map>
#include <arpa/inet.h>

enum struct NeighborState : uint8_t {
    DOWN = 1,
    ATTEMPT,
    INIT,
    TWO_WAY,
    EXSTART,
    EXCHANGE,
    LOADING,
    FULL
};

class Interface;

class Neighbor {
public:
    NeighborState neighborState = NeighborState::DOWN;
    bool is_master;

    //当前被发往邻居的DD包序号
    uint32_t dd_seq_num;

    //neighbor's last DD packet
    uint32_t last_dd_seq_num;
    uint32_t last_dd_data_len;
    char last_dd_data[1024];

    //details of neighbor
    uint32_t id;
    uint32_t priority;
    uint32_t ip;
    uint32_t designed_router;
    uint32_t backup_designed_router;
    Interface* local_interface;

    //LSA连接状态请求列表(需要从邻居接受以同步LSDB的)
    std::deque<LSAHeader> lsr_list;
    //区域连接状态数据库中LSA的完整列表
    std::deque<LSAHeader> db_summary_list;
    //已经被洪泛但没有从邻接得到确认的LSA列表
    std::map<uint32_t, uint32_t> link_state_rxmt_list;

    //线程相关
    pthread_t send_empty_dd;//发送空DD报文以确定master/slave
    pthread_mutex_t lsr_list_lock;
    pthread_t lsr_send;//发送LSR

    //构造函数
    Neighbor(in_addr_t ip, Interface* interface);
    ~Neighbor();

    //从lsr_req)list中删除
    void removefromLSRList(uint32_t link_state_id, uint32_t advertise_router_id);

    //事件触发回调方法
    void receiveHelloEvent();
    void receive1WayEvent();
    void receive2WayEvent();
    void negotiationDoneEvent();
    void seqNumberMismatchEvent();
    void exchangeDoneEvent();
    void badLSReqEvent();
    void loadDoneEvent();

    //初始化数据库摘要列表
    void initDBSummaryList();
};

#endif //WHY_OSPF_NEIGHBOR_H
