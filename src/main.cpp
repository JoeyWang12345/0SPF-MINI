#include "common/common.h"
#include "./model/interface/interface.h"
#include "./controller/message/messageController.h"
#include "./model/LSDB/LSDB.h"
#include "./controller/route/routeController.h"

LSDB lsdb;//链路状态数据库
RoutingTable routing_table;

int main() {
    //创建收发线程
    pthread_t send_hello_thread;
    pthread_t receive_message_thread;

    //初始化接口
    Interface interface;
    interface.ip = ntohl(inet_addr("192.168.206.4"));
    WHYConfig::interfaces.push_back(&interface);
    WHYConfig::iptointerface[interface.ip] = &interface;

    //启动接口
    // printf("start interface.\n");
    interface.upEvent();

    //初始化线程属性
    pthread_attr_init(&WHYConfig::thread_attr);
    pthread_attr_setdetachstate(&WHYConfig::thread_attr, PTHREAD_CREATE_DETACHED);

    //初始化LSA互斥锁
    pthread_mutex_init(&LSA_seq_lock, NULL);

    //启动线程
    pthread_create(&send_hello_thread, NULL, sendHelloPacket, &interface);
    pthread_create(&receive_message_thread, NULL, receivePacket, &interface);
    
    //等待处理用户输入(TODO 或许换种结束方式？)
    while (true) {
        std::string op;
        std::cin >> op;
        std::cout << op << std::endl;

        //判断是否要退出
        if (op == "exit") {
            std::cout << "Killing OSPF...Please wait for a while" << std::endl;
            exiting = true;
            break;
        }
    }

    //等待线程结束
    pthread_join(send_hello_thread, NULL);
    pthread_join(receive_message_thread, NULL);

    std::cout << "OSPF has closed." << std::endl;
    return 0;
}
