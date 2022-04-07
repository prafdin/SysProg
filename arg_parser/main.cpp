#include <iostream>
#include "Parser.h"
int main (int argc, char* argv[]) {
    ArgParser args(argc, argv);
    auto prog = args.getProgName();
    if (!args.Pars(argc, argv)){
        return 0;
    }
    std::cout<<prog;
}
