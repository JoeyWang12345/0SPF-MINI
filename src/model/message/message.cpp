//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "message.h"

//计算数据块的fletcher校验和，确认数据在传输过程中的完整性
uint16_t calculate_fletcher_checksum(const void* lsa_data, size_t length, size_t checksum_offset) {
    const uint8_t* pos = static_cast<const uint8_t*>(lsa_data);
    
    //中间计算值
    int32_t x, y;
    //乘法运算中间变量
    uint32_t mul;
    //fletcher校验和的两个累加和
    uint32_t c0 = 0, c1 = 0;
    //最终校验和结果
    uint16_t checksum = 0;
    
    for (int i = 0; i < length; i++) {
        if (i == checksum_offset || i == checksum_offset + 1) {
            c1 += c0;
            pos++;
        }
        else {
            c0 += *(pos++);
            c1 += c0;
        }
    }

    c0 %= 255;
    c1 %= 255;
    mul = c0 * (length - checksum_offset);
    x = mul - c1 - c0;
    y = c1 - 1 - mul;
    if (x < 0) {
        x--;
    }
    if (y >= 0) {
        y++;
    }
    x %= 255;
    y %= 255;
    if (!x) {
        x = 255;
    }
    if (!y) {
        y = 255;
    }
    y &= 0x00ff;

    return y | (x << 8);
}   

/*
 * LSA Header
 */
LSAHeader::LSAHeader() {
    ls_age = 50;//暂定
    options = 0x02;
    ls_sequence_number = 0x80000000;//默认保留值
}

void LSAHeader::hton() {
    ls_age = htons(ls_age);
    link_state_id = htonl(link_state_id);
    advertising_router = htonl(advertising_router);
    ls_sequence_number = htonl(ls_sequence_number);
    ls_checksum = htons(ls_checksum);
    length = htons(length);
}   

void LSAHeader::ntoh() {
    ls_age = ntohs(ls_age);
    link_state_id = ntohl(link_state_id);
    advertising_router = ntohl(advertising_router);
    ls_sequence_number = ntohl(ls_sequence_number);
    ls_checksum = ntohs(ls_checksum);
    length = ntohs(length);
}

void LSAHeader::print() {
    printf("------------LSA Header Info------------\n");
    printf("ls_age: %d\n", ls_age);
    printf("ls_type: %d\n", ls_type);
    ip_addr.s_addr = htonl(link_state_id);
    printf("link_state_id: %s\n", inet_ntoa(ip_addr));
    ip_addr.s_addr = htonl(advertising_router);
    printf("advertising_router: %s\n", inet_ntoa(ip_addr));
    printf("ls_sequence_number: %d\n", ls_sequence_number);
    printf("---------------------------------------\n");
}

/*
 * LSA 
 */
LSA::LSA() {
    lsaHeader = LSAHeader();
}

//bigger: newer，比较两个LSA实例
bool LSA::operator>(const LSA& other) {
    //优先检查序列号
    if (this->lsaHeader.ls_sequence_number > other.lsaHeader.ls_sequence_number) {
        return true;
    }
    //TODO 或许还会检查其他的
    return false;
}

/*
 * LSA Router
 */
LSARouter::LSARouter() {
    lsaHeader = LSAHeader();
    lsaHeader.ls_type = 1;
    zero1  = 0;
    b_B = 0;
    b_E = 0;
    b_V = 0;
    zero2 = 0;
    link_num = 0;
}

LSARouter::LSARouter(char* lsu_lsa_pos) {
    //LSA头部
    lsaHeader = *((LSAHeader*)lsu_lsa_pos);
    lsaHeader.ntoh();
    //LSA数据部分
    char* lsa_data = lsu_lsa_pos + LSA_HEADER_LEN;
    zero1 = 0;
    b_B = b_E = b_V = 0;
    zero2 = 0;
    link_num = ntohs(*(uint16_t*)(lsa_data + 2));
    //填充RouterLink
    char* lsa_link = lsa_data + 4;
    for (int i = 0; i < link_num; i++) {
        LSARouterLinks.push_back(LSARouterLink(lsa_link));
        lsa_link += sizeof(LSARouterLink);
    }
}

//将路由器LSA转换为网络传输的字节数据
char* LSARouter::toRouterLSA() {
    //LSA的总大小
    char* lsa_router = new char[size()];
    //初始化动态值
    lsaHeader.length = size();//整个LSA的长度
    lsaHeader.ls_checksum = 0;
    link_num = LSARouterLinks.size();
    //填充LSA头部
    LSAHeader lsa_header = lsaHeader;
    lsa_header.hton();//复制一份出来，用作转换为网络号
    memcpy(lsa_router, &lsa_header, LSA_HEADER_LEN);
    //填充标志位
    uint16_t* flags = (uint16_t*)(lsa_router + LSA_HEADER_LEN);
    *flags = 0;
    //填充链路数量
    uint16_t* num = flags + 1;
    *num = htons(link_num);
    //填充链路信息
    LSARouterLink* lsa_router_link = (LSARouterLink*)(num + 1);
    for (LSARouterLink link : LSARouterLinks) {
        lsa_router_link->link_id = htonl(link.link_id);
        lsa_router_link->link_data = htonl(link.link_data);
        lsa_router_link->type = link.type;
        lsa_router_link->tos_num = 0;
        lsa_router_link->metric = htons(link.metric);
        lsa_router_link++;
    }
    //计算并填充校验和
    //TODO 所做的一切都是为了得到网络字节序的版本
    //lsa_router+2是为了跳过LS age，从这里开始到LS checksum正好是14的偏移量
    lsaHeader.ls_checksum = calculate_fletcher_checksum(lsa_router + 2, lsaHeader.length - 2, 14);
    LSAHeader* header = (LSAHeader*)lsa_router;
    header->ls_checksum = htons(lsaHeader.ls_checksum);

    return lsa_router;
}

size_t LSARouter::size() {
    //整个router_lsa的大小
    return LSA_HEADER_LEN + 4 + LSARouterLinks.size() * sizeof(LSARouterLink);
}

/*
 * LSA Router Link
 */
LSARouterLink::LSARouterLink() {
    tos_num = 0;
    metric = 1;
}

LSARouterLink::LSARouterLink(char* lsa_link) {
    link_id = ntohl(*(uint32_t*)lsa_link);
    link_data = ntohl(*(uint32_t*)(lsa_link + 4));
    type = ntohl(*(uint8_t*)(lsa_link + 8));
    tos_num = 0;
    metric = ntohs(*(uint16_t*)(lsa_link + 10));
}

/*
 * LSA Network
 */
LSANetwork::LSANetwork() {
    lsaHeader = LSAHeader();
    lsaHeader.ls_type = 2;
    //TODO 暂定是这样
    network_mask = 0xffffff00;
}

LSANetwork::LSANetwork(char* lsu_lsa_pos) {
    //初始化LSA头部
    lsaHeader = *((LSAHeader*)lsu_lsa_pos);
    lsaHeader.ntoh();
    //初始化LSA数据部分
    char* lsa_data = lsu_lsa_pos + LSA_HEADER_LEN;
    network_mask = ntohl(*(uint32_t*)lsa_data);
    //填充路由列表
    int router_num = (lsaHeader.length - LSA_HEADER_LEN - 4) / 4;
    uint32_t* router_pos = (uint32_t*)(lsa_data + 4);
    for (int i = 0; i < router_num; i++) {
        attached_routers.push_back(ntohl(*router_pos));
        router_pos++;
    }
}

//将网络LSA转换为网络传输的字节数据
char* LSANetwork::toNetworkLSA() {
    //LSA总大小
    char* lsa_network = new char[size()];
    //初始化动态值
    lsaHeader.length = size();
    lsaHeader.ls_checksum = 0;
    //填充LSA头部
    LSAHeader lsa_header = lsaHeader;
    lsa_header.hton();
    memcpy(lsa_network, &lsa_header, LSA_HEADER_LEN);
    //填充网络mask和attached_routers
    uint32_t* pos = (uint32_t*)(lsa_network + LSA_HEADER_LEN);
    pos[0] = htonl(network_mask);
    pos++;
    for (uint32_t router_id : attached_routers) {
        *pos = htonl(router_id);
        pos++;
    }
    //填充校验和
    lsaHeader.ls_checksum = calculate_fletcher_checksum(lsa_network + 2, lsaHeader.length - 2, 14);
    LSAHeader* header = (LSAHeader*)lsa_network;
    header->ls_checksum = htons(lsaHeader.ls_checksum);

    return lsa_network;
}

size_t LSANetwork::size() {
    //TODO LSANetworkLink大小为什么是32？
    return LSA_HEADER_LEN + 4 + attached_routers.size() * sizeof(uint32_t);
}