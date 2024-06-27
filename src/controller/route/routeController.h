//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_ROUTECONTROLLER_H
#define WHY_OSPF_ROUTECONTROLLER_H

#include <map>
#include "../../model/interface/interface.h"
#include <net/route.h>

#define INFINITY (0x7fffffff)

//路由表项
struct RoutingTableItem {
    uint32_t destination;//目的IP
    uint32_t address_mask;//子网mask
    uint8_t type;//路由类型
    uint32_t metric;//路由成本
    uint32_t next_hop;//下一跳IP

    RoutingTableItem();
    RoutingTableItem(uint32_t destination, uint32_t next_hop, uint32_t metric);
    void print();
};

struct Edge {
    uint32_t source_ip;
    uint32_t metric;
    uint32_t target_id;

    Edge();
    Edge(uint32_t source_ip, uint32_t metric, uint32_t target_id);
};

//顶点和与之相邻的边
struct Vertex {
    uint32_t router_id;
    std::map<uint32_t, Edge> adjacencies;

    Vertex();
    Vertex(uint32_t router_id);
    void print();
};

//到目标顶点的路径信息
struct toTargetVertex {
    uint32_t target_vertex_id;//目标顶点ID
    uint32_t total_metric;//路径总成本
    uint32_t next_hop;//下一跳
    Interface* out_interface;//出接口

    toTargetVertex();
    toTargetVertex(uint32_t target_vertex_id, uint32_t total_metric);
    void print();
};

class RoutingTable {
public:
    //路由器ID和对应顶点
    std::map<uint32_t, Vertex> vertexes;
    //路由器ID和到达目标的路径
    std::map<uint32_t, toTargetVertex> paths;
    //路由表项，存放目标地址和路由的对应
    std::map<uint32_t, RoutingTableItem> routings;

    //与内核态交互
    int router_fd;//写入内核路由的套接字
    //已被写入内核的路由
    std::vector<struct rtentry> rtentries;

    //更新路由表
    void update();
    void buildTopo();
    void printTopo();
    void calRouting();
    void printPaths();
    void generateRouting();
    void printRoutingTable();

    RoutingTable();
    ~RoutingTable();
    //与内核交互
    void resetRoute();//重置内核路由
    void writeKernelRoute();//写入内核路由
};

extern RoutingTable routing_table;

#endif //WHY_OSPF_ROUTECONTROLLER_H