#include "nutmeg/runner/cli.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    return nutmeg::runner::runCli(args, std::cout, std::cerr);
}
