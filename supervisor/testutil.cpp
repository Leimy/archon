// -*- mode: c++; eval: (c-set-style "stroustrup"); eval: (c-set-style "archon-cc-mode"); -*-
#include "include/util.h"
#include <iostream>

using namespace archon;
using namespace std;

int a = 42;

void foo () {
    cout << "I die\n";
}

int main () {
    auto oesc = MakeOnExitScopeCall(&foo);
    cout << "A is currently: " << a << endl; // 42 
    {
	auto oesc = MakeOnExitScopeCall([](){ ++a; });
    }
    cout << "A is currently: " << a << endl; // 43

} // prints "I die\n".
	
