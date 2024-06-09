#include <string>
using namespace std;

#ifndef COMPONENTS_H
#define COMPONENTS_H

class Object;

class Component {
    private:
        Object* linkedobj;
        string component_name;
    public:
        Component(Object* linkedobj);
};

#endif
