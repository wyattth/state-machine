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

    template<class R>
    struct Region {

        struct State : public E {
            using Events = E;
            using RegionType = R;
            
            Region* region;
            
            void* operator new (size_t size, void *addr) {
//                std::cout << "New " << size << " at " << addr << std::endl;
                return addr;
            }
            
            void handle(void (Events::*event)(), const char *name) {
                region->handled = false;
                std::cout << "    (" << className<R>()
                    << " ignores event " << name << ")\n";
            }
            virtual void dispatch(void (Events::*event)(), const char *name) {
                (this->*event)();
            }
            template<class D>
            void transitionTo() {
                region->machine->template transitionTo<D>();
            }
            virtual void exitTo(State* s) { }
            virtual void enterFrom(State* s) { }
        };

        S      *machine;
        bool    handled;
        State   stateStorage;
        State  *currentState = &stateStorage;
        
        Region() {
            currentState->region = this;
        }
        
        template<class D>
        D* setStateTo() {
            currentState = new (&stateStorage) D;
            currentState->region = this;
            return (D*) currentState;
        }
        
        void start(S* m) {
            std::cout << " Starting " << className<R>() << std::endl;
            machine = m;
            currentState->region = this;
            machine->template transitionTo<typename R::InitialState>();
        }
        void end() {
            std::cout << "  Transitioning out of "
            << className<Region>() << std::endl;
            machine->template doTransitionTo<State>();
            std::cout << " Ended " << className<R>() << std::endl;
        }
        void handle(void (Events::*event)(), const char *name) {
            std::cout << "  " << className<R>() << "." << name << "()" << std::endl;
            currentState->dispatch(event, name);
        }
    };

    template<class D>
    void doTransitionTo() {
        auto newStateRegion = &(typename D::RegionType&)(*(S*)this);
        auto oldCurrentState = newStateRegion->currentState;
        D newCurrentState;
        newCurrentState.region = newStateRegion;
        oldCurrentState->exitTo(&newCurrentState);
        newCurrentState.enterFrom(oldCurrentState);
    }

    template<class D>
    void transitionTo() {
        std::cout
        << "  Transitioning " << className<typename D::RegionType>()
        << " to state " << className<D>() << std::endl;
        doTransitionTo<D>();
    }
    
    void start() {
        std::cout << "Starting " << className<S>() << std::endl;
        ((S*)this)->topLevel.start((S*)this);
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
    using State = typename Outer::State;
    
    void exitTo(State* s) {
        if (! dynamic_cast<Inner*>(s)) {
            ((Inner*)this)->exitInnerRegions();
            ((Inner*)this)->exit();
            std::cout << "   Exited " << className<Inner>() << "\n";
            Outer::region->template setStateTo<Outer>();
            Outer::exitTo(s);
        }
    }
    
    void enterFrom(State* s) {
        if (! dynamic_cast<Inner*>(s)) {
            Outer::enterFrom(s);
            std::cout << "   Entering " << className<Inner>() << "\n";
            Inner* i = Outer::region->template setStateTo<Inner>();
            i->entry();
            i->enterInnerRegions();
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

    template<class R>
    R& innerRegion() const { return (R&)(*Outer::region->machine); }
    
    virtual void dispatch(void (Outer::Events::*event)(), const char *name) {
        auto& r1 = innerRegion<R1&>();
        auto& r2 = innerRegion<R2&>();
        r1.handled = true;
        r2.handled = true;
        r1.handle(event,name);
        r2.handle(event,name);
        if ( !r1.handled && !r2.handled ) {
            (this->*event)();
        }
    }
    
    void exitInnerRegions() {
        innerRegion<R1&>().end();
        innerRegion<R2&>().end();
    }
    
    void enterInnerRegions() {
        innerRegion<R1&>().start(Outer::region->machine);
        innerRegion<R2&>().start(Outer::region->machine);
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
        using InitialState = B;
    } topLevel;
    
    struct Region21 : public Region<Region21> {
        using InitialState = V;
    } r21;
    
    struct Region22 : public Region<Region22> {
        using InitialState = X;
    } r22;
    
    struct Region31 : public Region<Region31> {
    } r31;
    
    struct Region32 : public Region<Region32> {
    } r32;
    
    operator Region1&() { return topLevel; }
    operator Region21&() { return r21; }
    operator Region22&() { return r22; }
    operator Region31&() { return r31; }
    operator Region32&() { return r32; }
    
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
            transitionTo<W>();
        }
    };
    
    struct D : public SubMachines<C,D,Region21,Region22> {
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
    
    struct W : public SubState<U,W> {
        void g() {
            std::cout << "W.g()\n";
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
    m.g();
    
    m.end();
    
    return 0;
}
