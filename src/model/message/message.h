//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_MESSAGE_H
#define WHY_OSPF_MESSAGE_H
#include <cstdint>
#include <vector>

#define IP_PROTOCOL_OSPF (89)

#define IP_HEADER_LEN (20)
#define IP_HEADER_SOURCE_IP (12)
#define IP_HEADER_TARGET_IP (16)

#define OSPF_HEADER_LEN (sizeof(OSPFHeader))
#define OSPF_HELLO_LEN (sizeof(OSPFHello))
#define OSPF_DD_LEN (sizeof(OSPFDD))

#define LSA_HEADER_LEN (sizeof(LSAHeader))

uint16_t calculate_fletcher_checksum(const void* lsa_data, size_t length, size_t checksum_offset);

enum OSPFType: uint8_t {
    HELLO = 1,
    DD,
    LSR,
    LSU,
    LSAck
};

/**
 * OSPF Header: 报文头部
 * version: 版本号，IPv4为2
 * type: 报文类型，1-5
 * packet_length: 报文总长度，包括报头
 * router_id: 报文通告路由器(生成该包的路由器)的ID
 * area_id: 标识报文属于某个区域
 * checksum: 报文内容校验和
 * autype: 认证类型
 * authentication: 与认证类型相关
 */
struct OSPFHeader {
    uint8_t version = 2;
    uint8_t type;
    uint16_t packet_length;
    uint32_t router_id;
    uint32_t area_id;
    uint16_t checksum;
    uint16_t autype;
    uint32_t authentication[2];
};

/**
 * Packet Data: Hello
 * network_mask: 网络掩码
 * hello_interval: 发送Hello包的间隔
 * options: 路由器支持的选项
 * rtr_pri: 路由器优先级(用于DR和BDR选举)
 * router_dead_interval: 宣告断开前需等待的秒数
 * designed_router: 发送路由器的视角上看，网络中的DR
 * backup_designed_router: BDR
 */
struct OSPFHello {
    uint32_t network_mask;
    uint16_t hello_interval;
    uint8_t options;
    uint8_t rtr_pri;
    uint32_t router_dead_interval;
    uint32_t designed_router;
    uint32_t backup_designed_router;
    //maybe have neighbors
};

/**
 * Packet Data: DD
 * interface_MTU: 从所关联的接口不分片而能发送的最大IP包字节数
 * options: 路由器支持的选项
 * b_MS: 主从位，数据库交换时主机设定为1，否则为从机
 * b_M: 还有更多位时设为1
 * b_I: 初始位，在第一个DD包中设为1
 * sequence_number: 描述DD包的序号
 * 一共只有8个字节，尽管看起来很多
 */
struct OSPFDD {
    uint16_t interface_MTU;
    uint8_t options;
    uint8_t b_MS: 1;
    uint8_t b_M: 1;
    uint8_t b_I: 1;//这里限制了b_I是1位的    
    uint8_t b_other: 5;
    uint32_t sequence_number;
    //maybe have lsa headers(DD报文分为空和带有摘要信息两种)
};

/**
 * Packet Data: LSR
 * 三要素可唯一表示一个LSA
 */
struct OSPFLSR {
    uint32_t LS_type;
    uint32_t LS_state_id;
    uint32_t advertising_router;
};

/**
 * Packet Data: LSU
 */
struct OSPFLSU {
    uint32_t LSA_num;
    //maybe have LSAs
};

struct OSPFLSAck {
    //maybe have LSA headers
};

enum LSAType : uint8_t {
    ROUTER = 1,
    NETWORK,
    SUMNET,
    SUMASB,
    ASE
};

/*
 * LSA Header
 * ls_age: 从LSA生成开始的时间
 * options: 路由域中支持的可选项
 * ls_type: LSA的类型，1-5
 * link_state_id: 描述由LSA描述的网络部件
 * advertising_router: 生成LSA的路由器标识
 * ls_sequence_number: 判定旧的或重复的LSA
 * ls_checksum: 整个LSA的Fletcher校验和
 * length: LSA的字节长度
 * PS: LSA头部长度为20字节
 */
struct LSAHeader {
    uint16_t ls_age;
    uint8_t options;
    uint8_t ls_type;
    uint32_t link_state_id;
    uint32_t advertising_router;
    uint32_t ls_sequence_number;
    uint16_t ls_checksum;
    uint16_t length;//整个LSA的length，包括头部

    LSAHeader();
    void hton();
    void ntoh();
    void print();
};

//连接类型
enum LinkType : uint8_t {
    P2P = 1,
    TRANSIT,
    STUB,
    VIRTUAL
};

/*
 * LSA Data
 * 描述各个路由器连接(接口)，每个接口都有类型
 */
struct LSARouterLink {
    //连接标识(路由器连接所接入的目标)，取决于连接类型
    uint32_t link_id;
    //连接数据
    uint32_t link_data;
    //连接的基本描述
    uint8_t type;
    //服务类型(type of service)数量，默认为0
    uint8_t tos_num = 0;
    //路由器连接的距离
    uint16_t metric;

    LSARouterLink();
    LSARouterLink(char* lsa_link);
    
    //比较两实例是否相等
    bool operator==(const LSARouterLink& other);
    void print();
};

struct LSA {
    LSAHeader lsaHeader;

    LSA();
    //要使用override，必须在父类声明虚函数
    virtual size_t size() = 0;
    bool operator>(const LSA& other);
};

//lsaHeader从LSA继承得到
struct LSARouter : public LSA {
    uint8_t zero1 : 5;
    uint8_t b_V : 1;//虚拟通道，传输区域
    uint8_t b_E : 1;//ASBR
    uint8_t b_B : 1;//ABR
    uint8_t zero2 = 0;
    //LSA描述的路由器连接数量(必须是该区域路由器连接的总和)
    uint16_t link_num;
    std::vector<LSARouterLink> LSARouterLinks;

    LSARouter();
    LSARouter(char* lsu_lsa_pos);
    char* toRouterLSA();
    size_t size() override;
    void print();
    bool operator==(const LSARouter& other);
};

struct LSANetwork : public LSA {
    //网络mask(ABC类网络)
    uint32_t network_mask = 0xffffff00;
    //接入该网络的各个路由器的标识
    std::vector<uint32_t> attached_routers;

    LSANetwork();
    LSANetwork(char* lsu_lsa_pos);
    char* toNetworkLSA();
    size_t size() override;
    void print();
    bool operator==(const LSANetwork& other);
};

#endif //WHY_OSPF_MESSAGE_H