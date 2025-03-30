#include "VirtualMachine.h"
#include <iostream>

void VirtualMachine::start() {
    std::cout << "Virtual Machine started." << std::endl;
}

void VirtualMachine::stop() {
    std::cout << "Virtual Machine stopped." << std::endl;
}

void VirtualMachine::restart() {
    stop();
    start();
}
