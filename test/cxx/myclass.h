#ifndef _MYCLASS_H_
#define _MYCLASS_H_

#include <list>

using namespace std;

class MyClass {
    private:
        list<int> data;
    public:
        void loadFile(const char* fileName);
        int compute(int x);
};

#endif // _MYCLASS_H_
