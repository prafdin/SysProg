#include "Parser.h"
#include <string.h>

using namespace std;
ArgParser::ArgParser(int argc, char* argv[]) {
    for(int i=1; i<argc; ++i){
        string str=string(argv[i]);
        if(str.find("-")!=0){
            args["program"]=argv[i];
            //run();
        }
    }
}

bool ArgParser::Pars(int argc, char* argv[]){
    for(int i=1; i<argc; ++i) {
        if(argc==2 && argv[i]==getProgName()){ //запуск, если первый аргумент имя
            //run();
            cout<<"Запуск программы"<<endl;
            return true;
        }
        if (!(strcmp(argv[i], "-h")) || !(strcmp(argv[i], "-help"))) {
            cout << "Help" << endl;
            return false;
        }
        if((!(strcmp(argv[i], "-p"))) && argc==2){ //если после -р ничего нет
            return false;
        }
        else if((!(strcmp(argv[i], "-p"))) && argv[i+1]!=getProgName()){ //если после -р идет не имя
            return false;
        }
        else {
            cout<<"Запуск программы"<<endl;
            return true;
        }
    }
    if(argc==1){ //если нет аргументов
        return false;
    }
    return true;
}

std::string ArgParser::getProgName() {
    return args["program"];
}

