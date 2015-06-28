//
//  main.cpp
//  state-machine
//
//  Created by Thomas Wyatt on 28/06/2015.
//  Copyright (c) 2015 Thomas Wyatt. All rights reserved.
//

#include <iostream>
#include <cxxabi.h>  // demangle class names in logs

template<class Class>
std::string className() {
    int status;
    char *name = abi::__cxa_demangle(typeid(Class).name(), NULL, NULL, &status);
    std::string result = name;
    free(name);
    return result;
}

template<class E>
struct EventList {
    virtual void handle(void (E::*event)(), const char *name) = 0;
};

template<class S, class E>
struct System : public E {
    using Events = E;

    int accepted;

    template<class R>
    struct Region {
        S *machine;
        
        Region(S *m) : machine(m), currentState(new State) {
            currentState->region = this;
        }
        
        struct State : public Events {
            using EventTypes = Events;
            using RegionType = Region;
            Region* region;
            void handle(void (Events::*event)(), const char *name) {
                --region->machine->accepted;
                std::cout << className<R>() << " ignores event " << name << "\n";
            }
            virtual void dispatch(void (Events::*event)(), const char *name) {
                (this->*event)();
            }
            template<class D>
            void transitionTo() {
                region->template transitionTo<D>();
            }
            virtual void exitTo(State* s) { }
            virtual void enterFrom(State* s) { }
        };
        State *currentState = new State(this);
        void start() {
            transitionTo<typename R::InitialState>();
        }
        void end() {
            transitionTo<State>();
        }
        void handle(void (Events::*event)(), const char *name) {
            currentState->dispatch(event, name);
        }
        template<class D>
        void transitionTo() {
            auto oldCurrentState = currentState;
            std::cout << "Transitioning " << className<R>()
            << " to state " << className<D>() << std::endl;
            currentState = new D;
            currentState->region = this;
            oldCurrentState->template exitTo(currentState);
            currentState->enterFrom(oldCurrentState);
        }
    };

    void start() {
        std::cout << "Starting " << className<S>() << std::endl;
        ((S*)this)->topLevel.start();
    }
    void end() {
        ((S*)this)->topLevel.end();
        std::cout << "Ended " << className<S>() << std::endl;
    }
    void handle(void (E::*event)(), const char *name) {
        ((S*)this)->topLevel.handle(event, name);
    }
};

template<class Outer, class Inner>
struct SubState : public Outer {
    
    void exitTo(typename Outer::State* s) {
        if (! dynamic_cast<Inner*>(s)) {
            ((Inner*)this)->exitInnerRegions();
            ((Inner*)this)->exit();
            std::cout << "Exit " << className<Inner>() << "\n";
            Outer::exitTo(s);
        }
    }
    
    void enterFrom(typename Outer::State* s) {
        if (! dynamic_cast<Inner*>(s)) {
            Outer::enterFrom(s);
            std::cout << "Enter " << className<Inner>() << "\n";
            ((Inner*)this)->entry();
            ((Inner*)this)->enterInnerRegions();
        }
    }
    
    // A normal substate has no inner regions, so nothing to do unless overridden
    void exitInnerRegions() { }
    void enterInnerRegions() { }
    
    // Default entry/exit handers (do nothing unless non-virtually overridden)
    void entry() { }
    void exit() { }
};

template<class Outer, class Inner, class R1, class R2>
struct SubMachines : public SubState<Outer,Inner> {
    R1* r1;
    R2* r2;
    
    virtual void dispatch(void (Outer::EventTypes::*event)(), const char *name) {
        Outer::region->machine->accepted=2;
        r1->handle(event,name);
        r2->handle(event,name);
        if (Outer::region->machine->accepted == 0) {
            (this->*event)();
        }
    }
    
    void exitInnerRegions() {
        r1->end();
        r2->end();
    }
    
    void enterInnerRegions() {
        ((Inner*)this)->getMachines(&r1,&r2);
        r1->start();
        r2->start();
    }
};

// -------------------------------------------------------------------------

struct MyEvents : public EventList<MyEvents> {
    virtual void f() { handle(&MyEvents::f, __FUNCTION__); }
    virtual void g() { handle(&MyEvents::g, __FUNCTION__); }
    virtual void h() { handle(&MyEvents::h, __FUNCTION__); }
    virtual void k() { handle(&MyEvents::k, __FUNCTION__); }
};

struct MySystem : public System<MySystem, MyEvents> {
    
    // Forward declarations for initial states
    struct B;
    struct V;
    struct X;
    
    struct Region1 : public Region<Region1> {
        using Region::Region;
        using InitialState = B;
    } topLevel = this;
    
    struct Region21 : public Region<Region21> {
        using Region::Region;
        using InitialState = V;
    } r21 = this;
    
    struct Region22 : public Region<Region22> {
        using Region::Region;
        using InitialState = X;
    } r22 = this;
    
    struct Region31 : public Region<Region31> {
        using Region::Region;
    } r31 = this;
    
    struct Region32 : public Region<Region32> {
        using Region::Region;
    } r32 = this;

    struct A : public SubState<Region1::State,A> {
        void entry() {
            std::cout << "A.entry()\n";
        }
        void f() {
            std::cout << "A.f()\n";
        }
    };
    
    struct B : public SubState<A,B> {
        void g() {
            std::cout << "B.g()\n";
            transitionTo<D>();
        }
    };
    
    struct C : public SubState<Region1::State,C> {
        void k() {
            std::cout << "C.k()\n";
        }
    };
    
    struct D : public SubMachines<C,D,Region21,Region22> {
        void getMachines(Region21**a21, Region22**a22) {
            *a21 = &region->machine->r21;
            *a22 = &region->machine->r22;
        }
        void entry() {
            std::cout << "D.entry()\n";
        }
        void exit() {
            std::cout << "D.exit()\n";
        }
        void h() {
            std::cout << "D.h()\n";
        }
    };
    
    struct U : public SubState<Region21::State,U> {
        void f() {
            std::cout << "U.f()\n";
        }
    };
    
    struct V : public SubState<U,V> {
        void g() {
            std::cout << "V.g()\n";
        }
    };
    
    struct X : public SubState<Region22::State,X> {
    };
};

int main(int argc, const char * argv[]) {
    MySystem  m;
    
    m.start();
    
    m.f();
    m.g();
    m.f();
    m.g();
    m.h();
    m.k();
    
    m.end();
    
    return 0;
}
