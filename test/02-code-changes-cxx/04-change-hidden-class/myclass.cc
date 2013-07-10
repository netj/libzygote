#include "myclass.h"
#include <iostream>
#include <fstream>

using namespace std;

void MyClass::loadFile(const char* fileName) {
    ifstream ifs;
    ifs.open(fileName);

    int x;
    while (ifs.good()) {
        ifs >> x;
        //cout << x << endl;
        data.push_back(x);
    }

    ifs.close();

    cout << "loaded " << data.size() << " entries" << endl;
}

int MyClass::compute(int x) {
    cout << "(compute modified)" << endl;
    return hiddenMemberFunction(x);
}

int MyClass::hiddenMemberFunction(int x) {
    cout << "(and computing via hidden member function)" << endl;
    return x * 2;
}
