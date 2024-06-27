//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_MESSAGECONTROLLER_H
#define WHY_OSPF_MESSAGECONTROLLER_H
#include <cstdint>
#include "../../model/interface/interface.h"

//计算校验和
uint16_t calculate_checksum(const void* data, size_t length);

//发送数据包
void sendPacket(const char* data, size_t length, uint8_t type, uint32_t target_ip, Interface* interface);
void* sendHelloPacket(void* itf);
void* sendEmptyDDPacket(void* neighborPtr);
void* sendLSRPacket(void* neighborPtr);

//接收数据包
void* receivePacket(void* itf);
void receiveHelloPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header);
void receiveDDPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header);
void receiveLSRPacket(char* packet_receive, in_addr_t source_ip, Interface* intf, OSPFHeader* header);
void receiveLSUPacket(char* packet_receive, in_addr_t source_ip, in_addr_t target_ip, Interface* intf);

#endif //WHY_OSPF_MESSAGECONTROLLER_H