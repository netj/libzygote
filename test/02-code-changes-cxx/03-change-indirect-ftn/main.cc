#include "myclass.h"
#include <zygote.h>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        cerr << "Usage: " << argv[0] << " FILENAME" << endl;
        return 1;
    }
    MyClass a;
    a.loadFile(argv[1]);

    return zygote(const_cast<char *>("zygote.socket"), &a, NULL);
}

int indirectFunction(MyClass& a, int x) {
    cout << "from INDIRECTLY called function" << endl;
    return a.compute(x);
}

int directFunction(MyClass& a, int x) {
    cout << "from directly called function" << endl;
    return indirectFunction(a, x);
}

int run(int objc, void* objv[],  int argc, char* argv[]) {
    MyClass& a = *(MyClass*)objv[0];

    cout << "Computed: " << directFunction(a, atoi(argv[1])) << endl;

    return 0;
}
