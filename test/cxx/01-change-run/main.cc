#include "myclass.h"
#include <zygote.h>
#include <iostream>

int main(int argc, char* argv[]) {
    MyClass a;
    a.loadFile(argv[1]);

    return zygote("zygote.socket", &a, NULL);
}

int run(int objc, void* objv[],  int argc, char* argv[]) {
    MyClass& a = *(MyClass*)objv[0];

    cout << "Computing..." << endl;

    int result = a.compute(atoi(argv[1]));

    cout << "Computed: " << result << endl;

    return (result % 2);
}
