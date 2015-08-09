//
//  main.cpp
//  state-machine
//
//  Created by Thomas Wyatt on 28/06/2015.
//  Copyright (c) 2015 Thomas Wyatt. All rights reserved.
//

#include <iostream>
#include <cxxabi.h>  // demangle class names in logs

// Class Names
std::string typeName(const std::type_info& typeInfo) {
    int status;
    char* name = abi::__cxa_demangle(typeInfo.name(), NULL, NULL, &status);
    std::string result = name;
    free(name);
    return result;
}
template<typename C> std::string className() { return typeName(typeid(C)); }
template<typename C> std::string className(C& obj) { return typeName(typeid(obj)); }

class sm {
    
    template<typename P, typename C> class SubState_;

    // Temporary (const) event functors to pass around
    // (containing event name and arguement values)
    template<typename E>
    struct Event {
        const char *name;
        Event(const char *name) : name(name) {}
        virtual void sendTo(E* target) const = 0;
    };
    
    template<typename E, void (E::*eventHandler)()>
    struct EventWithoutArgs : Event<E> {
        EventWithoutArgs(const char *name) : Event<E>(name) { }
        void sendTo(E* target) const { (target->*eventHandler)(); }
    };
    
    template<typename E>
    struct EventWithoutArgs2 : Event<E> {
        void (E::*eventHandler)();
        EventWithoutArgs2(void (E::*event)(), const char *name)
        : Event<E>(name), eventHandler(event) { }
        void sendTo(E* target) const { (target->*eventHandler)(); }
    };
    
    template<typename E, typename A1, void (E::*eventHandler)(A1)>
    struct EventWith1Arg : Event<E> {
        A1 a1;
        EventWith1Arg(A1 a1, const char *name)
        : Event<E>(name), a1(a1) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1); }
    };
    
    template<typename E, typename A1>
    struct EventWith1Arg2 : Event<E> {
        A1 a1;
        void (E::*eventHandler)(A1);
        EventWith1Arg2(void (E::*event)(A1), A1 a1, const char *name)
        : Event<E>(name), eventHandler(event), a1(a1) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1); }
    };
    
    template<typename E, typename A1, typename A2, void (E::*eventHandler)(A1, A2)>
    struct EventWith2Args : Event<E> {
        A1 a1;
        A1 a2;
        EventWith2Args(A1 a1, A2 a2, const char *name)
        : Event<E>(name), a1(a1), a2(a2) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1, a2); }
    };
    
    template<typename E, typename A1, typename A2>
    struct EventWith2Args2 : Event<E> {
        A1 a1;
        A2 a2;
        void (E::*eventHandler)(A1, A2);
        EventWith2Args2(void (E::*event)(A1,A2), A1 a1, A2 a2, const char *name)
        : Event<E>(name), eventHandler(event), a1(a1), a2(a2) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1, a2); }
    };
    
    
    template<typename M, typename E>
    struct TopState_ : E {
        using MachineType = M;
        using State       = TopState_;
        using Event       = const Event<E>;

        State*       self = this;
        MachineType* machine;
        bool         eventWasIgnored;
        
        virtual void dispatch( Event& event ) {
            event.sendTo(self);
        }
        virtual void handle( Event& event ) {
            std::cout << "Ignore event " << event.name << std::endl;
        }
        TopState_(MachineType& m) : machine(&m) { }
        TopState_() { }
        
        template<typename DestinationState>
        void transitionTo() {
            leave(DestinationState(), true);
            DestinationState::enter(*machine, true);
        }
        static State*& currentState(MachineType& machine) {
            return machine.topLevelRegion.self;
        }
        static void enter(MachineType&, bool) { }
        virtual void leave(const State&, bool) { }
    };
    
    template<typename P, typename C, typename M, typename State, State M::* r>
    struct Region_ : State {
        template<typename GC>
        using SubState  = SubState_<C,GC>;
        using Event     = typename State::Event;
        
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
        virtual void handle( Event& event ) override {
            this->eventWasIgnored = true;
            std::cout << "Event " << event.name << "() "
            "ignored by region " << className<C>() << std::endl;
        }
    };
    
    template<typename P, typename C, typename... RR>
    struct ParallelState_ : P::template SubState<C> {
        using State  = typename P::State;
        using M      = typename P::MachineType;
        using Event  = typename P::Event;
        template<typename GC, State M::*r>
        using Region = sm::Region_<C,GC,M,State,r>;

        template<typename RegionBeingEntered>
        static void enterInnerRegions(M& m, bool deep) {
            C::enter(m,deep);
        }
        
        void dispatchToInnerRegions( Event& event ) {
            if (this->eventWasIgnored) {
                std::cout << "Event " << event.name << "() was ignored by "
                "all subregions of " << className<C>() << std::endl;
                P::dispatch(event);
            }
        }
    };
    
    template<typename P, typename C, typename R, typename... RR>
    struct ParallelState_<P,C,R,RR...> : ParallelState_<P,C,RR...> {
        using super = ParallelState_<P,C,RR...>;
        using M     = typename P::MachineType;
        using Event = typename P::Event;
        using State = typename P::State;
        
        template<typename RegionAlreadyEntering>
        static void enterInnerRegions(M& m, bool deep) {
            super::template enterInnerRegions<RegionAlreadyEntering>(m, deep);
            if ( ! std::is_same<R, RegionAlreadyEntering>() ) {
                std::cout << "Start region " << className<R>() << std::endl;
                R::InitialState::enter(m,false);
            }
        }
        virtual void leave(const State& s, bool deep) {
            std::cout << "Stop region1 " << className<R>() << std::endl;
            R::currentState(*this->machine)->leave(s, false);
            if (deep) {
                super::leave(s, deep);
            }
        }
        void dispatchToInnerRegions( Event& event ) {
            auto* subState = R::currentState(*this->machine);
            subState->eventWasIgnored = false;
            event.sendTo(subState->self);
            this->eventWasIgnored &= subState->eventWasIgnored;
            super::dispatchToInnerRegions(event);
        }
        virtual void dispatch( Event& event ) override {
            this->eventWasIgnored = true;
            dispatchToInnerRegions(event);
        }
    };
    
    template<typename Parent, typename Child>
    struct SubState_ : Parent {
        using State = typename Parent::State;
        
        SubState_() {
            static_assert( std::is_base_of<SubState_, Child>(),
                          "Correct Usage: struct MySubState : MySuperStateOrRegion::SubState<MySubState>"
                          );
        }
        
        virtual void leave(const State& target, bool deep) {
            if ( ! dynamic_cast<const Child*>(&target) ) {
                std::cout << "Exit " << className<Child>() << "\n";
                static_cast<Child*>(this)->exit();
                Parent* parent = new (this) Parent;
                this->self = parent;
                parent->leave(target, deep);
            }
        }
        
        static void enter(typename Parent::MachineType& m, bool deep) {
            State *&currentState = Parent::currentState(m);
            if ( typeid(*currentState) != typeid(Child) ) {
                Parent::enter(m, deep);
                std::cout << "Enter " << className<Child>() << "\n";
                Child* child = new (currentState) Child;
                currentState = child;
                child->entry();
            }
        }
        
        // Default entry/exit handers (do nothing unless non-virtually overridden)
        void entry() {}
        void exit() {}
        
        // The following Public Types are used to create decendents
        template<typename GrandChild>
        using SubState = SubState_<Child, GrandChild>;
        
        template<typename GrandChild, typename... Regions>
        using ParallelState = ParallelState_<Child, GrandChild, Regions...>;
    };
    
    template<typename M, typename E>
    struct Machine_ : E {
        using BaseState = TopState_<M,E>;
        using State     = BaseState;
        using Region    = State;
        
        Region  topLevelRegion { *this };
        
        void start() {
            M::InitialState::enter(*this, false);
        }
        void stop() {
            topLevelRegion.self->leave(BaseState(), true);
        }
        void handle( const Event<E>& f ) {
            topLevelRegion.self->dispatch(f);
        }
        operator M&() {
            return *static_cast<M*>(this);
        }
        
        template<typename S>
        using TopState = SubState_<BaseState,S>;
    };

public:
    template<typename E>
    class EventList {
    protected:
        template<void (E::*event)()>
        void handle(const char *name) {
            handle(EventWithoutArgs<E,event>(name));
        }
        void handle(void (E::*event)(), const char *name) {
            handle(EventWithoutArgs2<E>(event, name));
        }
        template<typename A1, void (E::*event)(A1)>
        void handle(A1 a1, const char *name) {
            handle(EventWith1Arg<E,A1,event>(a1,name));
        }
        template<typename A1>
        void handle(void (E::*event)(A1), A1 a1, const char *name) {
            handle(EventWith1Arg2<E,A1>(event, a1, name));
        }
        template<typename A1, typename A2, void (E::*event)(A1)>
        void handle(A1 a1, A2 a2, const char *name) {
            handle(EventWith2Args<E,A1,A2,event>(a1,a2,name));
        }
        template<typename A1, typename A2>
        void handle(void (E::*event)(A1,A2), A1 a1, A2 a2, const char *name) {
            handle(EventWith2Args2<E,A1,A2>(event, a1, a2, name));
        }
        virtual void handle( const Event<E>& f ) = 0;
    public:
        template<typename M>
        using Machine = Machine_<M,E>;
    };
};

struct MyEvents : sm::EventList<MyEvents> {
    virtual void f() { handle(&MyEvents::f, __func__); }
    virtual void g() { handle<&MyEvents::g>(__func__); }
    virtual void h(int x) { handle(&MyEvents::h, x, __func__); }
    virtual void j(int x) { handle<int,&MyEvents::j>(x, __func__); }
};

struct MyMachine : MyEvents::Machine<MyMachine> {
    Region s1 { *this };
    Region s2 { *this };
    
    struct A : TopState<A> {
        void f() override {
            std::cout << "A::f()\n";
            transitionTo<D>();
        }
    };
    struct B : A::SubState<B> {
        void g() override {
            std::cout << "B::g()\n";
        }
    };
    struct C : A::SubState<C> {
    };
    struct D : C::SubState<D> {
        void f() override {
            std::cout << "D::f()\n";
            transitionTo<A>();
        }
        void h(int x) override {
            std::cout << "Got h(" << x << ")\n";
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
        void f() override {
            std::cout << "G::f()\n";
        }
        void g() override {
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
    m.h(2);
    m.f();
    m.h(3);
    m.g();
    m.h(4);
    std::cout << "Stop\n";
    m.stop();
    
    return 0;
}
