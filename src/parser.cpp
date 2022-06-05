#include "parser.h"
#include <string.h>
#include <unistd.h>
#include <fstream>

using namespace std;



string ArgParser::getProgName() {
    return _progName;
}

bool ArgParser::fileExist() {
    fstream fileStream;
    fileStream.open(_progName);
    if (fileStream.fail()) {
        cout << "File does not exist" << endl;
        return false;
    }
    return true;
}

bool ArgParser::parse() {
    int opt = 0;
    string ProgName = string(_argv[1]);
    if (ProgName.find("-") != 0) {
        _progName = ProgName;
    }
    while ((opt = getopt(_argc, _argv, opts)) != -1) {
        switch (opt) {
            case 'h':
                help();
                return false;
            case 'p':
                if (!_progName.empty()) {
                    cout << "Incorrect args" << endl;
                    return false;
                }
                _progName = string(optarg);
                break;
            default:
                help();
                return false;
        }
    }
    if (_progName.empty() || !fileExist()) {
        return false;
    }
    return true;
}

void ArgParser::help() {
    cout << "This is the debugger.  Usage:" << endl << endl <<
            "    ./my_app [options] [executable-file]" << endl << endl <<
            "Selection of debuggee:" << endl << endl <<
            "   -h             Print this message and then exit." << endl <<
            "   -p             Option requires an argument"<< endl;
}


