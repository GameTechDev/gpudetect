#include "VirtualMachine.h"
#include "WasmServer.h"

int main() {
    VirtualMachine vm;
    vm.start();
    vm.stop();
    vm.restart();

    WasmServer ws;
    ws.start();
    ws.stop();
    ws.restart();

    return 0;
}
