#include <iostream>
#include <map>

using namespace std;
class ArgParser {
    map <string, string> args;
    string ParsName(char argv[]);
public:
    ArgParser(int argc, char **argv);
    bool Pars(int argc, char* argv[]);
    string getProgName();
};

