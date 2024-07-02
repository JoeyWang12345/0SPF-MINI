#include "common/common.h"
#include "./model/interface/interface.h"
#include "./controller/message/messageController.h"
#include "./model/LSDB/LSDB.h"
#include "./controller/route/routeController.h"

LSDB lsdb;//链路状态数据库
RoutingTable routing_table;

int main() {
    //创建收发线程(一个)
    pthread_t send_hello_thread;
    pthread_t receive_message_thread;

    //创建收发线程(两个)
    // pthread_t send_hello_thread_1;
    // pthread_t receive_message_thread_1;
    // pthread_t send_hello_thread_2;
    // pthread_t receive_message_thread_2;

    //初始化第一个接口
    Interface interface1;
    interface1.interface_name = "ens37";
    interface1.ip = ntohl(inet_addr("30.1.1.2"));
    interface1.cost = 200;
    WHYConfig::interfaces.push_back(&interface1);
    WHYConfig::iptointerface[interface1.ip] = &interface1;

    //启动第一个接口
    interface1.upEvent();

    //初始化第二个接口
    Interface interface2;
    interface2.interface_name = "ens38";
    interface2.cost = 100;
    interface2.ip = ntohl(inet_addr("40.1.1.2"));
    WHYConfig::interfaces.push_back(&interface2);
    WHYConfig::iptointerface[interface2.ip] = &interface2;

    //启动第二个接口
    interface2.upEvent();

    //初始化线程属性
    pthread_attr_init(&WHYConfig::thread_attr);
    pthread_attr_setdetachstate(&WHYConfig::thread_attr, PTHREAD_CREATE_DETACHED);

    //初始化LSA互斥锁
    pthread_mutex_init(&LSA_seq_lock, NULL);

    //启动线程
    pthread_create(&send_hello_thread, NULL, sendHelloPacket, &interface1);
    pthread_create(&receive_message_thread, NULL, receivePacket, &interface1);

    pthread_create(&send_hello_thread, NULL, sendHelloPacket, &interface2);
    pthread_create(&receive_message_thread, NULL, receivePacket, &interface2);
    
    //等待处理用户输入(TODO 或许换种结束方式？)
    while (true) {
        std::cout << "WHY-OSPF>";
        std::string command;
        std::getline(std::cin, command);

        if (command == "display ospf lsdb") {
            lsdb.print();
        }
        else if (command == "display ospf path") {
            routing_table.update();
            routing_table.printPaths();
        }
        else if (command == "display ip routing-table") {
            routing_table.update();
            routing_table.printRoutingTable();
        }
        else if (command == "display ospf peer") {
            //TODO 打印邻居信息
            continue;
        }
        else if (command == "reset route") {
            routing_table.resetRoute();
        }
        //判断是否要退出
        else if (command == "exit") {
            std::cout << "Killing OSPF...Please wait for a while" << std::endl;
            exiting = true;
            routing_table.resetRoute();
            break;
        }
        else {
            std::cout << "Unknown command" << std::endl;
        }
    }

    //等待线程结束(两个)
    // pthread_join(send_hello_thread_1, NULL);
    // pthread_join(receive_message_thread_1, NULL);
    // pthread_join(send_hello_thread_2, NULL);
    // pthread_join(receive_message_thread_2, NULL);

    //等待线程结束(一个)
    pthread_join(send_hello_thread, NULL);
    pthread_join(receive_message_thread, NULL);

    std::cout << "OSPF has closed." << std::endl;
    return 0;
}
