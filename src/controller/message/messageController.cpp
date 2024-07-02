//
// Created by LEGION on 2024/6/14.
//
#include "../../common/common.h"
#include "messageController.h"
#include "../../common/globalConfig.h"
#include "../../model/LSDB/LSDB.h"
#include "../retransmitter/retransmitter.h"

//接收缓冲区大小
#define RECV_BUFFER 1514

uint16_t calculate_checksum(const void* data, size_t length) {
    //累加数据块的值
    uint32_t sum = 0;//(不会是因为没有设定初始值...)
    const uint16_t* pos = static_cast<const uint16_t*>(data);

    //循环按16位块遍历数据，将每个块的值累加到sum中
    for (size_t i = 0; i < length / 2; i++) {
        sum += *pos++;
        // pos++;
    }

    //检查数据长度是否为奇数
    if (length % 2 == 1) {
        //是则将最后一个字节的值加到sum中
        sum += static_cast<const uint8_t*>(data)[length - 1];
    }

    //将累加结果的高低16位相加，处理进位
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

void* sendHelloPacket(void* itf) {
    Interface* intf = (Interface*) itf;

    //创建原始套接字
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_RAW, IP_PROTOCOL_OSPF)) < 0) {
        perror("socket creation failed");
    }

    //将套接字绑定到特定的网络接口
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, intf->interface_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("setsockopt SO_BINDTODEVICE failed");
    }

    //设定目标多播地址
    struct sockaddr_in dst_sockaddr;
    memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
    dst_sockaddr.sin_family = AF_INET;
    dst_sockaddr.sin_addr.s_addr = inet_addr("224.0.0.5");

    //循环发送Hello报文
    char* packet = (char*)malloc(65535);
    while (true) {
        if (exiting) {
            break;
        }

        //添加邻居的长度到OSPF报文总长度
        size_t packet_sum_length = OSPF_HEADER_LEN + OSPF_HELLO_LEN + 4 * intf->neighbors.size();

        //构建OSPF Header
        OSPFHeader* header = (OSPFHeader*)packet;
        header->version = 2;
        header->type = 1;//表示Hello
        header->packet_length = htons(packet_sum_length);
        header->router_id = htonl(WHYConfig::router_id);
        header->area_id = htonl(intf->area_id);
        header->checksum = 0;
        header->autype = 0;
        header->authentication[0] = 0;
        header->authentication[1] = 0;

        //构建Hello报文
        OSPFHello* hello = (OSPFHello*)(packet + OSPF_HEADER_LEN);
        hello->network_mask = htonl(intf->network_mask);
        hello->hello_interval = htons(intf->hello_interval);
        hello->options = 0x02;
        hello->rtr_pri = intf->router_priority;
        hello->router_dead_interval = htonl(intf->router_dead_interval);
        hello->designed_router = htonl(intf->designed_router);
        hello->backup_designed_router = htonl(intf->backup_designed_router);

        //将邻居列表附加到Hello数据包末尾
        uint32_t* neighbor_attach = (uint32_t*)(packet + OSPF_HEADER_LEN + OSPF_HELLO_LEN);
        for (auto& neighbor : intf->neighbors) {
            *neighbor_attach = htonl(neighbor->id);
            neighbor_attach++;
        }

        //计算数据包的校验和
        header->checksum = calculate_checksum(header, packet_sum_length);

        //发送数据包
        if (sendto(socket_fd, packet, packet_sum_length, 0, (struct sockaddr*)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) {
            perror("send hello packet failed");
        }
        else {
            printf("send hello packet success\n");
        }
        
        //休眠一段时间
        sleep(intf->hello_interval);
    }
    //终止线程
    pthread_exit(NULL);
}

//发送OSPF数据包
void sendPacket(const char* data, size_t length, uint8_t type, uint32_t target_ip, Interface* interface) {
    //TEST
    printf("Sending packet, length: %d, type: %d...\n", length, type);
    
    //创建原始套接字
    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_RAW, IP_PROTOCOL_OSPF)) < 0) {
        perror("socket creation failed");
    }

    //将套接字绑定到特定的网络接口
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface->interface_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
        perror("setsockopt SO_BINDTODEVICE failed");
    }

    //设定目标多播地址
    struct sockaddr_in dst_sockaddr;
    memset(&dst_sockaddr, 0, sizeof(dst_sockaddr));
    dst_sockaddr.sin_family = AF_INET;
    dst_sockaddr.sin_addr.s_addr = htonl(target_ip);

    //OSPF报头
    char* packet = (char*)malloc(1500);
    size_t packet_sum_length = OSPF_HEADER_LEN + length;
    OSPFHeader* header = (OSPFHeader*)packet;
    header->version = 2;
    header->type = type;
    header->packet_length = htons(packet_sum_length);
    header->router_id = htonl(WHYConfig::router_id);
    header->area_id = htonl(interface->area_id);
    header->checksum = 0;
    header->autype = 0;
    header->authentication[0] = 0;
    header->authentication[1] = 0;

    //OSPF数据部分
    memcpy(packet + OSPF_HEADER_LEN, data, length);

    //计算校验和
    header->checksum = calculate_checksum(header, packet_sum_length);

    //发送OSPF报文
    if (sendto(socket_fd, packet, packet_sum_length, 0, (struct sockaddr*)&dst_sockaddr, sizeof(dst_sockaddr)) < 0) {
        perror("send packet failed");
    }
    else {
        printf("send packet success: type %d, length %d\n", type, packet_sum_length);
    }

    //释放分配的内存
    free(packet);
}   

void* receivePacket(void* itf) {
    Interface* intf = (Interface*) itf;

    //创建接收套接字
    int socket_fd;
    if ((socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        perror("Receive socket creation failed");
    }

    //将套接字绑定到特定的网络接口
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, intf->interface_name);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr) < 0)) {
        perror("Receive setsockopt SO_BINDTODEVICE failed");
    }

    //打印提示
    // printf("Receive Packet has init.\n");
    // printf("Interface's name: %s.\n", intf->interface_name);
    
    //循环接收网络数据包并处理
    struct iphdr* ip_header;//IP头部信息
    struct in_addr source, target;//两个地址

    //1514字节，用于接收数据frame
    char* frame_receive = (char*)malloc(RECV_BUFFER);
    //跳过以太网头部
    char* packet_receive = frame_receive + sizeof(struct ethhdr);

    //循环接收数据包
    while (true) {
        // printf("while true.\n");
        if (exiting) {
            printf("exiting.\n");
            break;
        }
        //将接收缓冲区的数据清零
        memset(frame_receive, 0, RECV_BUFFER);
        //从套接字接收数据并存入frame_receive
        int receive_size = recv(socket_fd, frame_receive, RECV_BUFFER, 0);

        //检查IP数据包头部
        ip_header = (struct iphdr*)packet_receive;
        //若不是OSPF协议
        // printf("%d\n", ip_header->protocol);
        if (ip_header->protocol != 89) {
            // printf("Not OSPF protocol.\n");
            continue;
        }

        //检查地址是否合法
        in_addr_t source_ip = ntohl(*(uint32_t*)(packet_receive + IP_HEADER_SOURCE_IP));
        in_addr_t target_ip = ntohl(*(uint32_t*)(packet_receive + IP_HEADER_TARGET_IP));
        //这的逻辑加一条:目标地址为多播需判断源是否处于同一网段
        if ((target_ip != intf->ip && target_ip != ntohl(inet_addr("224.0.0.5")))
         || (source_ip == intf->ip) || 
         (target_ip == ntohl(inet_addr("224.0.0.5")) && ((source_ip & 0xffffff00) != (intf->ip & 0xffffff00)))) {
            printf("Invalid address, continue.\n");
            continue;
        }

        //TEST 打印测试
        // printf("Receive one packet.\n");
        source.s_addr = htonl(source_ip);
        printf("source: %s, ", inet_ntoa(source));
        target.s_addr = htonl(target_ip);
        printf("target: %s\n", inet_ntoa(target));

        //报文头部
        OSPFHeader* header = (OSPFHeader*)(packet_receive + IP_HEADER_LEN);
        header->packet_length = ntohs(header->packet_length);
        header->router_id = ntohl(header->router_id);
        header->area_id = ntohl(header->area_id);
        header->checksum = ntohl(header->checksum);

        //核心处理逻辑
        if (header->type == HELLO) {
            printf("Receive Hello packet.\n");
            printf("Interface's name: %s.\n", intf->interface_name);
            receiveHelloPacket(packet_receive, source_ip, intf, header);
            // printf("ReceiveHellopacket finished.\n");
        }
        else if (header->type == DD) {
            printf("Receive DD packet.\n");
            printf("Interface's name: %s.\n", intf->interface_name);
            receiveDDPacket(packet_receive, source_ip, intf, header);
            // printf("ReceiveDDpacket finished.\n");
        }
        else if (header->type == LSR) {
            printf("Receive LSR packet.\n");
            printf("Interface's name: %s.\n", intf->interface_name);
            receiveLSRPacket(packet_receive, source_ip, intf, header);
            // printf("ReceiveLSRpacket finished.\n");
        }
        else if (header->type == LSU) {
            printf("Receive LSU packet.\n");
            printf("Interface's name: %s.\n", intf->interface_name);
            receiveLSUPacket(packet_receive, source_ip, target_ip, intf);
            // printf("ReceiveLSUpacket finished.\n");
        }
        // printf("Next circle.\n");
    }
    // printf("Receive packet finished.\n");

    //释放空间
    free(packet_receive);
    pthread_exit(NULL);
}

void receiveHelloPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header) {
    OSPFHello* hello = (OSPFHello*)(packet_receive + IP_HEADER_LEN + OSPF_HEADER_LEN);
    //从接口的邻居列表中获取源IP地址对应的邻居
    Neighbor* neighbor;
    if ((neighbor = intf->getNeighbor(source_ip)) == nullptr) {
        printf("Doesn't have neighbor: %x, add it into list.\n", source_ip);
        //若为空则加入列表
        neighbor = intf->addNeighbor(source_ip);
    }

    //设置邻居信息，保存之前的DR和BDR
    neighbor->id = header->router_id;
    uint32_t prev_neighbor_dr = neighbor->designed_router;
    uint32_t prev_neighbor_bdr = neighbor->backup_designed_router;
    neighbor->designed_router = ntohl(hello->designed_router);
    neighbor->backup_designed_router = ntohl(hello->backup_designed_router);
    neighbor->priority = ntohl(hello->rtr_pri);

    //处理Hello报文接收事件
    neighbor->receiveHelloEvent();
    // printf("ReceiveHelloEvent finished.\n");

    //TEST
    printf("neighbor's prev_dr: %x.\n", prev_neighbor_dr);
    printf("neighbor's prev_bdr: %x.\n", prev_neighbor_bdr);
    printf("neighbor's dr: %x.\n", neighbor->designed_router);
    printf("neighbor's bdr: %x.\n", neighbor->backup_designed_router);
    printf("neighbor's ip: %x.\n", neighbor->ip);
    printf("neighbor's id: %x.\n", neighbor->id);

    //检查Hello报文中是否包含本路由器的ID以确定邻居关系是否为2-Way状态
    uint32_t* neighbor_attach = (uint32_t*)(packet_receive + IP_HEADER_LEN + OSPF_HELLO_LEN);
    uint32_t* end = (uint32_t*)(packet_receive + IP_HEADER_LEN + header->packet_length);
    bool is2Way = false;//判定是否为2Way
    while (neighbor_attach != end) {
        if (*neighbor_attach == htonl(WHYConfig::router_id)) {
            is2Way = true;
            neighbor->receive2WayEvent(intf);
            break;
        }
        neighbor_attach++;
    }
    if (!is2Way) {
        neighbor->receive1WayEvent();
        //可以结束方法
        //TEST
        return;
    }

    //检查DR的变化情况
    if (neighbor->designed_router == neighbor->ip && neighbor->backup_designed_router == 0
    && intf->interfaceState == InterfaceState::WAITING) {
        //TODO 这里的方法不返回
        // printf("DR's BackupSeenEvent started.\n");
        intf->backupSeenEvent();
        // printf("DR's BackupSeenEvent finished.\n");
    }
    else if ((prev_neighbor_dr == neighbor->ip) ^ (neighbor->designed_router == neighbor->ip)) {
        //如果neighbor所宣称的DR发生了变化
        // printf("DR's NeighborChangeEvent started.\n");
        intf->neighborChangeEvent();
        // printf("DR's NeighborChangeEvent finished.\n");
    }

    //检查BDR的变化情况
    if (neighbor->backup_designed_router == neighbor->ip
    && intf->interfaceState == InterfaceState::WAITING) {
        // printf("BDR's BackupSeenEvent started.\n");
        intf->backupSeenEvent();
        // printf("BDR's BackupSeenEvent finished.\n");
    }
    else if ((prev_neighbor_bdr == neighbor->ip) ^ (neighbor->backup_designed_router == neighbor->ip)) {
        // printf("BDR's NeighborChangeEvent started.\n");
        intf->neighborChangeEvent();
        // printf("BDR's NeighborChangeEvent finished.\n");
    }
}

void receiveDDPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header) {
    OSPFDD* dd = (OSPFDD*)(packet_receive + IP_HEADER_LEN + OSPF_HEADER_LEN);
    Neighbor* neighbor = intf->getNeighbor(source_ip);

    //处理序列号
    bool is_accepted = false;//报文是否接受
    bool is_duplicate = false;//报文是否重复
    //这是收到的DD报文的序列号
    uint32_t sequence_number = ntohl(dd->sequence_number);
    printf("DD packet's sequence_number: %d.\n", sequence_number);
    printf("Last DD sequence_number: %d.\n", neighbor->last_dd_seq_num);
    //若序列号和上次收到的相同
    if (neighbor->last_dd_seq_num == sequence_number) {
        is_duplicate = true;
        printf("Received duplicate DD packets.\n");
    }
    //不同就更新
    else {
        neighbor->last_dd_seq_num = sequence_number;
    }

    //TEST
    printf("Neighbor's state: %d.\n", neighbor->neighborState);

    //根据邻居状态处理DD报文
    start_switch:
    switch (neighbor->neighborState) {
        case NeighborState::INIT: {
            //将建立2WAY或EXSTART关系
            neighbor->receive2WayEvent(intf);
            goto start_switch;
            break;
        }
        //本地路由器与邻居路由器进行主从角色协商(空DD报文)
        case NeighborState::EXSTART: {
            //邻居宣称自己是master
            if (neighbor->id > WHYConfig::router_id && dd->b_I && dd->b_M && dd->b_MS) {
                printf("neighbor %x declared to be master, ", neighbor->id);
                printf("local interface %x agreed, ", intf->ip);
                printf("neighbor became master.\n");

                neighbor->is_master = true;
                //将邻居的DD序号设定为主机所提出的号码(因为还没有正式发报文)
                neighbor->dd_seq_num = sequence_number;
                //Master每发送一个DD报文，seq便+1
                sequence_number += 1;
            }
            //邻居宣称自己是slave
            else if (WHYConfig::router_id > neighbor->id && dd->sequence_number == neighbor->dd_seq_num
            && !dd->b_I && !dd->b_MS) {
                printf("neighbor %x declared to be slave, ", neighbor->id);
                printf("local interface %x confirmed, ", intf->ip);
                printf("self became master.\n");

                neighbor->is_master = false;
            }
            //其他情况，忽略
            else {
                printf("dd packet was ignored.\n");
                //立即结束当前DD报文的处理，准备下一次
                return;
            }
            //协商完成，跳转回去重新检查邻居状态
            neighbor->negotiationDoneEvent();
            goto start_switch;
            break;
        }
        //向邻居发送DD包来描述链路状态数据库
        case NeighborState::EXCHANGE: {
            //主机收到重复DD包时直接丢弃，从机收到重复DD包时重发前一个DD包
            if (is_duplicate) {
                printf("Received duplicate dd packet and ignored.\n");
                //说明本地为从机
                if (neighbor->is_master) {
                    sendPacket(neighbor->last_dd_data, neighbor->last_dd_data_len, DD, neighbor->ip, neighbor->local_interface);
                }
                return;
            }
            //检查报文一致性
            if ((neighbor->is_master ^ dd->b_MS)) {
                printf("DD packet mismatched: b_I or b_MS not fit negotiation result.\n");
                neighbor->seqNumberMismatchEvent();
                return;
            }
            //检查序列号
            //这彻底没问题了，dd_seq_num表示上一次给邻居发的DD报文的序号
            if ((neighbor->is_master && sequence_number == neighbor->dd_seq_num + 1)
            || (!neighbor->is_master && sequence_number == neighbor->dd_seq_num)) {
                //accepted表示收到的DD是携带LSA header的
                printf("DD packet was accepted.\n");
                is_accepted = true;
            }
            else {
                printf("DD packet mismatched: sequence_number illegal.\n");
                neighbor->seqNumberMismatchEvent();
                return;
            }
            //TEST
            printf("Break from state exchange.\n");
            break;
        }
        //只需要处理重复报文
        case NeighborState::LOADING:
        case NeighborState::FULL: {
            if (is_duplicate) {
                printf("Received duplicate dd packet and ignored.\n");
                if (neighbor->is_master) {
                    sendPacket(neighbor->last_dd_data, neighbor->last_dd_data_len, DD, neighbor->ip, neighbor->local_interface);
                }
                return;
            }
            if ((neighbor->is_master ^ dd->b_MS)) {
                printf("DD packet mismatched: b_I or b_MS not fit negotiation result.\n");
                neighbor->seqNumberMismatchEvent();
                return;
            }
            break;
        }
        default: ;
    }

    //TEST
    printf("Out of switch and reply DD packet.\n");
    printf("Neighbor's db_summary_list has %d member.\n", neighbor->db_summary_list.size());
    if (is_accepted) {
        //获取接收到的DD报文中第一个LSA的起始位置和结束位置
        LSAHeader* receive_lsa_pos = (LSAHeader*)(packet_receive + IP_HEADER_LEN + OSPF_HEADER_LEN + OSPF_DD_LEN);
        LSAHeader* receive_lsa_end = (LSAHeader*)(packet_receive + IP_HEADER_LEN + header->packet_length);

        bool is_empty_dd = false;
        //TESST 特判是不是空DD报文
        if (receive_lsa_end == receive_lsa_pos) {
            is_empty_dd = true;
        }

        //计算需要从邻居获取的LSA并放入lsr_list
        while (receive_lsa_pos != receive_lsa_end) {
            LSAHeader lsa_header;
            //发布LSA的路由器
            lsa_header.advertising_router = ntohl(receive_lsa_pos->advertising_router);
            //网络环境信息
            lsa_header.link_state_id = ntohl(receive_lsa_pos->link_state_id);
            //识别LSA是否是最新的，路由器每生成一个新的LSA便+1
            lsa_header.ls_sequence_number = ntohl(receive_lsa_pos->ls_sequence_number);
            //LSA的类型(1-5)
            lsa_header.ls_type = receive_lsa_pos->ls_type;

            //将LSA添加到neighbor的lsr_list中(需要从邻居接收以同步LSDB)
            if (lsa_header.ls_type == ROUTER) {
                if (!lsdb.getRouterLSA(lsa_header.link_state_id, lsa_header.advertising_router)) {
                    neighbor->lsr_list.push_back(lsa_header);
                }
            }
            else if (lsa_header.ls_type == NETWORK) {
                if (!lsdb.getNetworkLSA(lsa_header.link_state_id, lsa_header.advertising_router)) {
                    neighbor->lsr_list.push_back(lsa_header);
                }
            }

            //循环位置++
            receive_lsa_pos++;
        }

        //TEST 现在本地是master
        printf("Num of LSR: : %d.\n", neighbor->lsr_list.size());

        //对收到的DD报文进行应答
        // printf("start replying to DD packet.\n");
        char* reply_data = (char*)malloc(1024);
        int reply_len = 0;

        OSPFDD* DD_ack = (OSPFDD*)reply_data;
        memset(DD_ack, 0, sizeof(OSPFDD));
        reply_len += sizeof(OSPFDD);
        DD_ack->options = 0x02;
        DD_ack->interface_MTU = htons(neighbor->local_interface->mtu);
        //这是给邻居回应的报文，MS表示自己是不是主机
        if (neighbor->is_master) {
            DD_ack->b_MS = 0;
        }
        else {
            DD_ack->b_MS = 1;
        }

        //根据主从关系讨论(本地为从机)
        if (neighbor->is_master) {
            //TEST 特判
            if (!is_empty_dd) {
                neighbor->dd_seq_num = sequence_number;
                printf("DD_ack's seq_num: %d.\n", sequence_number);
                DD_ack->sequence_number = htonl(sequence_number);
            }
            else {
                printf("DD_ack's seq_num: %d.\n", neighbor->dd_seq_num); 
                DD_ack->sequence_number = htonl(neighbor->dd_seq_num);
            }

            //本机接口是slave(不能这样设置，本地是slave，只要重复master的seq_num)
            // neighbor->dd_seq_num = sequence_number;
            // printf("neighbor's dd_seq_num: %d.\n", neighbor->dd_seq_num);
            // DD_ack->sequence_number = htonl(sequence_number);
            // DD_ack->sequence_number = htonl(neighbor->dd_seq_num);

            //添加LSA头部到应答报文中
            LSAHeader* lsa_header = (LSAHeader*)(reply_data + sizeof(OSPFDD));
            int lsa_count = 0;
            while (neighbor->db_summary_list.size() > 0) {
                //限制LSA的最大数目(暂定)
                if (lsa_count >= 20) {
                    break;
                }
                //front函数返回的是对象引用
                LSAHeader& lsa_h = neighbor->db_summary_list.front();
                lsa_h.hton();//需要转换为网络字节序
                memcpy(lsa_header, &lsa_h, LSA_HEADER_LEN);
                lsa_header++;
                lsa_count++;
                reply_len += LSA_HEADER_LEN;
                neighbor->db_summary_list.pop_front();
            }
            //设置b_M位
            //TODO 我不明白为什么上面while循环过后，这里还会不等于0
            //哦明白了，可能会因为LSA太多break掉
            if (neighbor->db_summary_list.size() == 0) {
                DD_ack->b_M = 0;
            }
            else {
                DD_ack->b_M = 1;
            }
            
            //发送报文
            sendPacket(reply_data, reply_len, DD, neighbor->ip, neighbor->local_interface);
            memcpy(neighbor->last_dd_data, reply_data, reply_len);
            neighbor->last_dd_data_len = reply_len;
            //如果没有其他的DD报文
            if (DD_ack->b_M == 0 && dd->b_M == 0) {
                neighbor->exchangeDoneEvent();
            }
            free(reply_data);
        }
        //自己的接口为主机
        else {
            //收到对自己发送的上一个DD报文的确认
            if (neighbor->link_state_rxmt_list.count(neighbor->dd_seq_num) > 0) {
                printf("received ack of last dd packet, delete rxmt.\n");
                //停止重传
                intf->rxmtter.delRxmtData(neighbor->link_state_rxmt_list[neighbor->dd_seq_num]);
            }
            //增加邻居数据结构中的DD序列号
            neighbor->dd_seq_num++;
            DD_ack->sequence_number = htonl(neighbor->dd_seq_num);

            //检查是否还有更多的DD报文需要发送
            //邻居的db_summary_list中存放着需要从我们这儿获取的DD包
            if (neighbor->db_summary_list.size() == 0 && dd->b_M == 0) {
                neighbor->exchangeDoneEvent();
                free(reply_data);
                return;//结束交换过程，继续处理下一个报文
            }
            
            //否则，向从机发送新的DD包
            //添加LSA头部到应答报文
            LSAHeader* lsa_header_pos = (LSAHeader*)(reply_data + sizeof(OSPFDD));
            int lsa_count = 0;
            while (neighbor->db_summary_list.size() > 0) {
                if (lsa_count >= 20) {
                    break;
                }
                LSAHeader& lsa_h = neighbor->db_summary_list.front();
                memcpy(lsa_header_pos, &lsa_h, LSA_HEADER_LEN);

                //TEST
                lsa_h.print();  
                
                lsa_header_pos++;
                lsa_count++;
                reply_len += LSA_HEADER_LEN;
                //向邻居发送本地LSDB的内容，同时从db_summary_list中删除
                neighbor->db_summary_list.pop_front();
            }
            //设置b_M位
            if (neighbor->db_summary_list.size() == 0) {
                DD_ack->b_M = 0;
            }
            else {
                DD_ack->b_M = 1;
            }

            //发送报文
            sendPacket(reply_data, reply_len, DD, neighbor->ip, neighbor->local_interface);
            uint32_t rxmt_data_id = intf->rxmtter.addRxmtData(
                RxmtData(reply_data, reply_len, DD, neighbor->ip, intf->rxmt_interval)
            );
            neighbor->link_state_rxmt_list[neighbor->dd_seq_num] = rxmt_data_id;
        }
    }
}

void receiveLSRPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header) {
    //解析收到的LSR报文
    OSPFLSR* lsr_pos = (OSPFLSR*)(packet_receive + IP_HEADER_LEN + OSPF_HEADER_LEN);
    OSPFLSR* lsr_end = (OSPFLSR*)(packet_receive + IP_HEADER_LEN + header->packet_length);
    Neighbor* neighbor = intf->getNeighbor(source_ip);

    //检查邻居状态(仅Exchange Loading Full状态可以接收)
    if (neighbor->neighborState < NeighborState::EXCHANGE) {
        printf("Neighbor state error: couldn't receive LSR packet.\n");
        return;
    }

    //分配内存用于LSU应答数据
    char* lsu_ack = (char*)malloc(2048);//包含LSA
    OSPFLSU* lsu = (OSPFLSU*)lsu_ack;//不包含LSA
    memset(lsu, 0, sizeof(OSPFLSU));
    lsu->LSA_num = 0;

    //指向LSU附加部分
    char *lsu_lsa = lsu_ack + sizeof(OSPFLSU);
    int lsa_count = 0;

    //逐个处理LSR请求(在lsdb中检索每个LSA并复制到LSU中发往邻居)
    while (lsr_pos != lsr_end) {
        //网络转为主机
        lsr_pos->LS_type = ntohl(lsr_pos->LS_type);
        lsr_pos->LS_state_id = ntohl(lsr_pos->LS_state_id);
        lsr_pos->advertising_router = ntohl(lsr_pos->advertising_router);

        //处理Router LSA
        if (lsr_pos->LS_type == ROUTER) {
            LSARouter* lsa_router = lsdb.getRouterLSA(lsr_pos->LS_state_id, lsr_pos->advertising_router);
            if (lsa_router == nullptr) {
                neighbor->badLSReqEvent();
                free(lsu_ack);
                //TODO 这里的return待定
                return;
            }
            else {
                char* lsa = lsa_router->toRouterLSA();
                memcpy(lsu_lsa, lsa, lsa_router->size());
                //释放toRouterLSA分配的内存
                delete[] lsa;
                lsu_lsa += lsa_router->size();
            }
        }
        //处理Network LSA
        else if (lsr_pos->LS_type == NETWORK) {
            LSANetwork* lsa_network = lsdb.getNetworkLSA(lsr_pos->LS_state_id, lsr_pos->advertising_router);
            if (lsa_network == nullptr) {
                neighbor->badLSReqEvent();
                free(lsu_ack);
                return;
            }
            else {
                char* lsa = lsa_network->toNetworkLSA();
                memcpy(lsu_lsa, lsa, lsa_network->size());
                delete[] lsa;
                lsu_lsa += lsa_network->size();
            }
        }
        lsr_pos++;
        lsa_count++;
    }
    lsu->LSA_num = htonl(lsa_count);
    sendPacket(lsu_ack, (lsu_lsa - lsu_ack), LSU, source_ip, intf);
    free(lsu_ack);//TODO 释放内存
}

void receiveLSUPacket(char* packet_receive, in_addr_t source_ip, in_addr_t target_ip, Interface* intf) {
    //收到LSU报文时，会将报文中每个LSA更新到lsdb，并生成LSAck应答报文
    Neighbor* neighbor = nullptr;
    bool target_is_local_interface = false;
    //检查目标地址是否等于本地接口地址
    if (target_ip == intf->ip) {
        target_is_local_interface = true;
        neighbor = intf->getNeighbor(source_ip);
    }

    //LSU报文起始位置
    OSPFLSU* lsu = (OSPFLSU*)(packet_receive + IP_HEADER_LEN + OSPF_HEADER_LEN);
    int lsa_count = ntohl(lsu->LSA_num);//LSA数量
    //TEST
    printf("LSU has %d lsa.\n", lsa_count);

    char* lsack = (char*)malloc(1024);
    LSAHeader* lsack_header = (LSAHeader*)lsack;

    //指向LSU中第一个LSA的起始位置
    char* lsu_lsa_pos = (char*)(lsu) + sizeof(OSPFLSU);

    //循环处理每个LSA
    for (int i = 0; i < lsa_count; i++) {
        lsdb.addLSA(lsu_lsa_pos);
        //从LSU中提取出LSA
        LSAHeader* lsa_header = (LSAHeader*)lsu_lsa_pos;
        //从邻居的req_list中移除该LSA
        if (target_is_local_interface) {
            neighbor->removefromLSRList(ntohl(lsa_header->link_state_id), ntohl(lsa_header->advertising_router));
        }
        //将当前LSA头部复制到LSAck数据中
        memcpy(lsack_header, lsa_header, sizeof(LSAHeader));
        lsack_header++;
        lsu_lsa_pos += ntohs(lsa_header->length);
    }
    //发送LSAck报文
    sendPacket(lsack, LSA_HEADER_LEN * lsa_count, LSAck, ntohl(inet_addr("224.0.0.5")), intf);
    free(lsack);
}

//发送空的DD报文
void* sendEmptyDDPacket(void* neighborPtr) {
    Neighbor* neighbor = (Neighbor*)neighborPtr;

    //TEST 只发一次DD报文
    // int count = 0;
    while (true) {
        // if (count) {
        //     break;
        // }
        //如果邻居状态不是EXSTART(交换初始状态)
        if (neighbor->neighborState != NeighborState::EXSTART) {
            break;
        }
        OSPFDD dd;
        memset(&dd, 0, sizeof(dd));
        //填充DD报文
        dd.interface_MTU = htons(neighbor->local_interface->mtu);
        //说明是外部路由器(E)
        dd.options = 0x02;
        dd.sequence_number = neighbor->dd_seq_num;
        //I-init M-more MS-master
        dd.b_I = dd.b_M = dd.b_MS = 1;

        sendPacket((char*)&dd, sizeof(dd), DD, neighbor->ip, neighbor->local_interface);
        printf("send empty DD packet success.\n");
        sleep(neighbor->local_interface->rxmt_interval);
        // count++;
    }

    pthread_exit(NULL);
}

void* sendLSRPacket(void* neighborPtr) {
    Neighbor* neighbor = (Neighbor*)neighborPtr;

    while (true) {
        //所有的循环进程都要判断是否exit
        if (exiting) {
            break;
        }
        //循环发送LSR报文，列表为空就结束
        pthread_mutex_lock(&neighbor->lsr_list_lock);
        if (neighbor->lsr_list.size() == 0) {
            neighbor->loadDoneEvent();
            break;
        }
        //初始化LSR报文
        char* lsr = (char*)malloc(1024);
        int count = 0;
        OSPFLSR* lsr_packet = (OSPFLSR*)lsr;

        //填充LSR报文
        for (LSAHeader lsr : neighbor->lsr_list) {
            lsr_packet->LS_state_id = htonl(lsr.link_state_id);
            lsr_packet->advertising_router = htonl(lsr.advertising_router);
            lsr_packet->LS_type = htonl(lsr.ls_type);

            lsr_packet++;
            count++;
        }
        //解锁并发送LSR报文
        pthread_mutex_unlock(&neighbor->lsr_list_lock);
        sendPacket(lsr, sizeof(OSPFLSR) * count, LSR, neighbor->ip, neighbor->local_interface);
        printf("send LSR packet success.\n");
        free(lsr);
        sleep(neighbor->local_interface->rxmt_interval);
    }
    pthread_exit(NULL);
}