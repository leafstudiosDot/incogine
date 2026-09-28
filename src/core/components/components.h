#include <string>
#include <memory>
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
        virtual ~Component();

        void setComponentName(string name);
        string getComponentName();
        Object* getLinkedObject() const;

        virtual void Start();
        virtual void Update();
        virtual void OnDestroy();
};

#endif
