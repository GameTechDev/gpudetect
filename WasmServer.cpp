#include "WasmServer.h"
#include <iostream>

void WasmServer::start() {
    std::cout << "Wasm Server started." << std::endl;
}

void WasmServer::stop() {
    std::cout << "Wasm Server stopped." << std::endl;
}

void WasmServer::restart() {
    stop();
    start();
}
