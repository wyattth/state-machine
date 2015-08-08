//
//  main.cpp
//  state-machine
//
//  Created by Thomas Wyatt on 28/06/2015.
//  Copyright (c) 2015 Thomas Wyatt. All rights reserved.
//

#include <iostream>
#include <cxxabi.h>  // demangle class names in logs

std::string typeName(const std::type_info& typeInfo) {
    int status;
    char* name = abi::__cxa_demangle(typeInfo.name(), NULL, NULL, &status);
    std::string result = name;
    free(name);
    return result;
}
template<typename C> std::string className() { return typeName(typeid(C)); }
template<typename C> std::string className(C& obj) { return typeName(typeid(obj)); }

namespace sm {
    
    template<typename P, typename C> struct SubState_;
    
    template<typename M, typename E>
    struct TopState_ : E {
        using MachineType = M;
        using Events      = E;
        using State       = TopState_;
        
        State* self = this;
        M*     machine;
        bool   ignored;
        
        virtual void dispatch( void (E::*event)(), const char *eventName ) {
            (this->*event)();
        }
        virtual void handle( void (Events::*event)(), const char *eventName ) {
            std::cout << "Ignore event " << eventName << std::endl;
        }
        TopState_(M& m) : machine(&m) { }
        TopState_() { }
        
        template<typename D>
        void transitionTo() {
            leave(D(), true);
            D::enter(*machine, true);
        }
        static State*& currentState(M& m) {
            return m.topState.self;
        }
        static void enter(M&, bool) { }
        virtual void leave(const State&, bool) { }
    };
    
    template<typename P, typename C, typename M, typename State, State M::* r>
    struct Region_ : State {
        using E = typename State::Events;
        template<typename GC> using SubState = SubState_<C,GC>;
        
        static void enter(M& m, bool deep) {
            if (deep) {
                P::template enterInnerRegions<C>(m, deep);
            }
        }
        virtual void leave(const State& s, bool deep) override {
            if (deep) {
                std::cout << "Leave all regions parallel to " << className<C>() <<"\n";
                ((P*)P::currentState(*this->machine))->leave(s, deep);
            }
        }
        static State*& currentState(M& m) {
            return (m.*r).self;
        }
        virtual void handle( void (E::*event)(), const char *eventName ) override {
            this->ignored = true;
            std::cout << "Event " << eventName << "() "
            "ignored by region " << className<C>() << std::endl;
        }
    };
    
    template<typename P, typename C, typename... RR>
    struct ParallelState_ : P::template SubState<C> {
        using State  = typename P::State;
        using M      = typename P::MachineType;
        using E      = typename P::Events;
        template<typename GC, State M::*r>
        using Region = sm::Region_<C,GC,M,State,r>;
        
        template<typename RegionBeingEntered>
        static void enterInnerRegions(M& m, bool deep) {
            C::enter(m,deep);
        }
        
        void dispatchToRegions( void (E::*event)(), const char *eventName ) {
            if (this->ignored) {
                std::cout << "Event " << eventName << "() was ignored by "
                "all subregions of " << className<C>() << std::endl;
                P::dispatch(event, eventName);
            }
        }
    };
    
    template<typename P, typename C, typename R, typename... RR>
    struct ParallelState_<P,C,R,RR...> : ParallelState_<P,C,RR...> {
        using super = ParallelState_<P,C,RR...>;
        using M     = typename P::MachineType;
        using E     = typename P::Events;
        using State = typename P::State;
        
        template<typename RegionBeingEntered>
        static void enterInnerRegions(M& m, bool deep) {
            super::template enterInnerRegions<RegionBeingEntered>(m, deep);
            if ( ! std::is_same<R, RegionBeingEntered>() ) {
                std::cout << "Start region " << className<R>() << std::endl;
                R::InitialState::enter(m,false);
            }
        }
        virtual void leave(const State& s, bool deep) override {
            std::cout << "Stop region1 " << className<R>() << std::endl;
            R::currentState(*this->machine)->leave(s, false);
            if (deep)
                super::leave(s, deep);
        }
        void dispatchToRegions( void (E::*event)(), const char *eventName ) {
            auto* subState = R::currentState(*this->machine);
            subState->ignored = false;
            (subState->*event)();
            this->ignored &= subState->ignored;
            super::dispatchToRegions(event, eventName);
        }
        virtual void dispatch( void (E::*event)(), const char *eventName ) override {
            this->ignored = true;
            dispatchToRegions(event,eventName);
        }
    };
    
    template<typename Parent, typename Child>
    struct SubState_ : Parent {
        using State = typename Parent::State;
        
        virtual void leave(const State& target, bool deep) {
            if ( ! dynamic_cast<const Child*>(&target) ) {
                std::cout << "Exit " << className<Child>() << "\n";
                static_cast<Child*>(this)->exit();
                (new (this) Parent)->leave(target, deep);
            }
        }
        
        static void enter(typename Parent::MachineType& m, bool deep) {
            State *&currentState = Parent::currentState(m);
            if ( typeid(*currentState) != typeid(Child) ) {
                Parent::enter(m, deep);
                std::cout << "Enter " << className<Child>() << "\n";
                (new (currentState) Child)->entry();
            }
        }
        
        // Default entry/exit handers (do nothing unless non-virtually overridden)
        void entry() {}
        void exit() {}
        
        // The following types are usinged to create decendents
        template<typename GrandChild>
        using SubState = SubState_<Child, GrandChild>;
        
        template<typename GrandChild, typename... Regions>
        using ParallelState = ParallelState_<Child, GrandChild, Regions...>;
    };
    
    template<typename M, typename E>
    struct Machine_ : E {
        using BaseState = TopState_<M,E>;
        using State     = BaseState;
        
        State  topState { *this };
        
        void start() {
            M::InitialState::enter(*this, false);
        }
        void stop() {
            topState.self->leave(BaseState(), true);
        }
        void handle( void (E::*event)(), const char *eventName ) {
            topState.self->dispatch(event, eventName);
        }
        operator M&() {
            return *static_cast<M*>(this);
        }
        
        template<typename S>
        using TopState = SubState_<BaseState,S>;
    };
    
    template<typename E>
    struct EventList {
        virtual void handle( void (E::*event)(), const char *eventName ) = 0;
        
        template<typename M>
        using Machine = Machine_<M,E>;
    };
}

struct MyEvents : sm::EventList<MyEvents> {
    virtual void f() { handle(&MyEvents::f, __func__); }
    virtual void g() { handle(&MyEvents::g, __func__); }
    virtual void h() { handle(&MyEvents::h, __func__); }
};

struct MyMachine : MyEvents::Machine<MyMachine> {
    State s1 { *this };
    State s2 { *this };
    
    struct A : TopState<A> {
        void f() {
            std::cout << "A::f()\n";
            transitionTo<D>();
        }
    };
    struct B : A::SubState<B> {
        void g() { std::cout << "B::g()\n"; }
    };
    struct C : A::SubState<C> {
    };
    struct D : C::SubState<D> {
        void f() {
            std::cout << "D::f()\n";
            transitionTo<A>();
        }
        void h() {
            transitionTo<G>();
        }
        void entry() { std::cout << "D::in()\n"; }
        void exit() { std::cout << "D::out()\n"; }
    };
    struct R1;
    struct R2;
    struct G;
    struct H;
    struct EE : C::ParallelState<EE,R1,R2> {
    };
    struct R1 : EE::Region<R1,&MyMachine::s1> {
        using InitialState = G;
    };
    struct R2 : EE::Region<R2,&MyMachine::s2> {
        using InitialState = H;
    };
    struct G : R1::SubState<G> {
        void f() {
            std::cout << "G::f()\n";
        }
        void g() {
            transitionTo<D>();
        }
    };
    struct H : R2::SubState<H> {
    };
    using InitialState = A;
};

int main(int argc, const char * argv[]) {
    MyMachine m;
    
    m.start();
    m.f();
    m.g();
    m.h();
    m.f();
    m.h();
    m.g();
    m.h();
    std::cout << "Stop\n";
    m.stop();
    
    return 0;
}
