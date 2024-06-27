//
// Created by LEGION on 2024/6/14.
//

#ifndef WHY_OSPF_LSACONTROLLER_H
#define WHY_OSPF_LSACONTROLLER_H

#include "../../model/interface/interface.h"

void generateRouterLSA();
void generateNetworkLSA(Interface* interface);

LSARouter* genRouterLSA(std::vector<Interface*> interfaces);
LSANetwork* genNetworkLSA(Interface* interface);

#endif //WHY_OSPF_LSACONTROLLER_H
