#include <iostream>
#include <thread>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    //std::string who_to_greet;
    //std::getline(std::cin, who_to_greet);
    //std::cout << "Hello " << who_to_greet << "!" << std::endl;

    const InstancePtr instance = Instance("");
    //const auto instance = daq::Instance();

    return 0;
}
