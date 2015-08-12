//
//  main.cpp
//  state-machine
//
//  Created by Thomas Wyatt on 28/06/2015.
//  Copyright (c) 2015 Thomas Wyatt. All rights reserved.
//

#include <iostream>
#include <functional> // for std::function (used in transaction actions)
#include <cxxabi.h>   // demangle class names in logs

// Class Names
std::string typeName(const std::type_info& typeInfo) {
    int status;
    char* name = abi::__cxa_demangle(typeInfo.name(), NULL, NULL, &status);
    std::string result = name;
    free(name);
    return result;
}
template<typename C> std::string className() {
    return typeName(typeid(C));
}
template<typename C> std::string className(C& obj) {
    return typeName(typeid(obj));
}

class sm {
public:
    template<typename P, typename C> class SubState_;
    class BaseEventInterface;
    
    // Temporary (const) event functors to pass around
    // (containing event name and arguement values)
    struct GenericEvent {
        const char *name;
        GenericEvent(const char *name) : name(name) {}
        virtual void sendTo(BaseEventInterface* target) const = 0;
    };

    typedef const GenericEvent& Event;

    struct BaseEventInterface {
        virtual void handle(Event event) = 0;
    };
    
    template<typename I, void (I::*eventHandler)()>
    struct EventWithoutArgs : GenericEvent {
        EventWithoutArgs(const char *name) : GenericEvent(name) { }
        void sendTo(BaseEventInterface* target) const {
            (dynamic_cast<I*>(target)->*eventHandler)();
        }
    };
    
    template<typename I>
    struct EventWithoutArgs2 : GenericEvent {
        void (I::*eventHandler)();
        EventWithoutArgs2(void (I::*event)(), const char *name)
            : GenericEvent(name), eventHandler(event) { }
        void sendTo(BaseEventInterface* target) const {
            (dynamic_cast<I*>(target)->*eventHandler)();
        }
    };
    
    template<typename I, typename A1, void (I::*eventHandler)(A1)>
    struct EventWith1Arg : GenericEvent {
        A1 a1;
        EventWith1Arg(A1 a1, const char *name)
            : GenericEvent(name), a1(a1) { }
        void sendTo(BaseEventInterface* target) const {
            (dynamic_cast<I*>(target)->*eventHandler)(a1);
        }
    };
    
    template<typename I, typename A1>
    struct EventWith1Arg2 : GenericEvent {
        A1 a1;
        void (I::*eventHandler)(A1);
        EventWith1Arg2(void (I::*event)(A1), A1 a1, const char *name)
            : GenericEvent(name), a1(a1), eventHandler(event) { }
        void sendTo(BaseEventInterface* target) const {
            (dynamic_cast<I*>(target)->*eventHandler)(a1);
        }
    };
    
    template<typename E, typename A1, typename A2,
             void (E::*eventHandler)(A1, A2)>
    struct EventWith2Args : GenericEvent {
        A1 a1;
        A1 a2;
        EventWith2Args(A1 a1, A2 a2, const char *name)
            : GenericEvent(name), a1(a1), a2(a2) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1, a2); }
    };
    
    template<typename E, typename A1, typename A2>
    struct EventWith2Args2 : GenericEvent {
        A1 a1;
        A2 a2;
        void (E::*eventHandler)(A1, A2);
        EventWith2Args2(void (E::*event)(A1,A2), A1 a1, A2 a2, const char *name)
            : GenericEvent(name), eventHandler(event), a1(a1), a2(a2) { }
        void sendTo(E* target) const { (target->*eventHandler)(a1, a2); }
    };
    
    
    template < typename    M,    // type of state (M)achine
               typename... E  >  // list of (E)vents supported by state machine
    struct TopState_ : E... {
        
        // Clever Trick
        //
        // Every state-machine state (e.g. "class MySubState") that is
        // a sub-state of (e.g. "class MySuperState") has its own
        // sub-TYPE called "class MySubState::HierarchyPos"
        // that inherits from "class MySuperState::HierarchyPos"
        // EVEN IF "class MySubState" DOESN'T INHERIT FROM "class MySuperState"
        //
        // This parallel hierarchy is used to test state inheritance across
        // subRegions, where there is no class inheritance between the states
        // themselves - i.e...
        //  - subRegion does NOT inherit from its enclosing superState
        //  - subRegion::HierarchyPos DOES inherit from superState::HierarchyPos
        
        struct StatePos {
            virtual void dummy() {}  // makes polymorphic, allowing dynamic_cast
        };
        
        using MachineType  = M;
        using State        = TopState_;
//        using Event        = const struct sm::Event<E>;
        using HierarchyPos = StatePos;
        
        State*       self = this;      // avoids compiler optimization errors
        MachineType* machine;          // for data members and region-from-type
        bool         eventWasIgnored;  // controls propogation in sub-regions
        
        virtual void dispatch( Event event ) {
            event.sendTo(self);        // by default send event to current state
        }
        virtual void handle( Event event ) {
            std::cout << className<M>() << "." << event.name << "() ignored(!)\n";
        }
        TopState_(MachineType& m) : machine(&m) { }
        TopState_() { }
        
        template<typename DestinationState>
        void transitionTo( std::function<void()> action = []{} ) {
            std::cout << " " << className(*this) << " -> "
                << className<DestinationState>() << std::endl;
            leave(typename DestinationState::HierarchyPos(), true);
            action();
            DestinationState::enter(*machine, true);
        }
        static State*& currentState(MachineType& machine) {
            return machine.topLevelRegion.self;
        }
        static void enterAncestors(MachineType&, bool) { }
        virtual void leave(const StatePos&, bool) { }
    };
    
    template < typename P,   // (P)arent state (i.e. outer state)
               typename C,   // (C)hild state
               typename M,   // type of state (M)achine
               typename S,   // generic (S)tate for this type of state machine
               S M::*   r >  // pointer to member where (R)egion state is stored
    struct Region_ : S {
        template<typename GC>
        using SubState  = SubState_<C,GC>;
//        using Event     = typename S::Event;
        struct HierarchyPos : P::HierarchyPos {};
        
        static void enterAncestors(M& m, bool deep) {
            if (deep) {
                P::template enterInnerRegions<C>(m, deep);
            }
        }
        virtual void leave(const typename P::StatePos& s, bool deep) override {
            if (deep) {
                std::cout << className<C>() << " leave all sibling regions\n";
                ((P*)P::currentState(*this->machine))->leave(s, deep);
            }
        }
        static S*& currentState(M& m) {
            return (m.*r).self;
        }
        virtual void handle( Event event ) override {
            this->eventWasIgnored = true;
            std::cout << className<C>() << "." << event.name << "() "
                "ignored by region\n";
        }
    };
    
    template < typename    P,    // (P)arent state
               typename    C,    // (C)hild state
               typename... RR >  // list of (RR)egions
    struct ParallelState_ : P::template SubState<C> {
        using State  = typename P::State;
        using M      = typename P::MachineType;
//        using Event  = typename P::Event;
        template<typename GC, State M::*r>
        using Region = sm::Region_<C,GC,M,State,r>;

        template<typename RegionBeingEntered>
        static void enterInnerRegions(M& m, bool deep) {
            C::enterAncestors(m,deep);
        }
        
        void dispatchToInnerRegions( Event event ) {
            if (this->eventWasIgnored) {
                std::cout << className<C>() << "." << event.name << "() "
                    "was ignored by all subregions, propagating up...\n";
                P::dispatch(event);
            }
        }
    };
    
    template < typename    P,    // (P)arent state
               typename    C,    // (C)hild state
               typename    R,    // this (R)egion
               typename... RR >  // other (RR)egions
    struct ParallelState_<P,C,R,RR...> : ParallelState_<P,C,RR...> {
        using super = ParallelState_<P,C,RR...>;
        using M     = typename P::MachineType;
//        using Event = typename P::Event;
        using State = typename P::State;
        
        template<typename RegionAlreadyEntering>
        static void enterInnerRegions(M& m, bool deep) {
            super::template enterInnerRegions<RegionAlreadyEntering>(m, deep);
            if ( ! std::is_same<R, RegionAlreadyEntering>() ) {
                std::cout << " " << className<R>() << ".startRegion()...\n";
                R::InitialState::enter(m,false);
                std::cout << " " << className<R>() << ".startRegion() done.\n";
            }
        }
        virtual void leave(const typename P::StatePos& s, bool deep) {
            std::cout << " " << className<R>() << ".stopRegion()...\n";
            R::currentState(*this->machine)->leave(s, false);
            std::cout << " " << className<R>() << ".stopRegion() done.\n";
            if (deep) {
                super::leave(s, deep);
            }
        }
        void dispatchToInnerRegions( Event event ) {
            auto* subState = R::currentState(*this->machine);
            subState->eventWasIgnored = false;
            event.sendTo(subState->self);
            this->eventWasIgnored =
                this->eventWasIgnored && subState->eventWasIgnored;
            super::dispatchToInnerRegions(event);
        }
        virtual void dispatch( Event event ) override {
            this->eventWasIgnored = true;
            dispatchToInnerRegions(event);
        }
    };
    
    template < typename P,   // (P)arent state
               typename C >  // (C)hild state
    struct SubState_ : P {
        using Parent       = P;
        using State        = typename Parent::State;
        using InitialState = C;
        struct HierarchyPos : P::HierarchyPos {};
        SubState_() {
            static_assert( std::is_same<Parent, typename C::Parent>(),
                          "Correct Usage: struct MySubState : MySuperStateOrRegion::SubState<MySubState>"
                          );
        }
        
        virtual void leave(const typename P::StatePos& target, bool deep) {
            if ( ! dynamic_cast<const typename C::HierarchyPos*>(&target) ) {
                std::cout << "  " << className<C>() << ".exit()\n";
                static_cast<C*>(this)->exit();       // call any exit method for child state
                Parent* parent = new (this) Parent;  // new parent state overwrites child state
                this->self = parent;                 // avoids mistaken compiler optimizations
                parent->leave(target, deep);         // (recursively) leave (new) parent state
            }
        }
        
        static void enterAncestors(typename Parent::MachineType& m, bool deep) {
            State *&currentState = Parent::currentState(m);
            if ( typeid(*currentState) != typeid(C) ) {
                Parent::enterAncestors(m, deep);  // (recursively) enter parent state first
                std::cout << "  " << className<C>() << ".entry()\n";
                C* child = new (currentState) C;  // new child state overwrites parent state
                currentState = child;             // avoids mistaken compiler optimizations
                child->entry();                   // call any entry method for child state
            }
        }
        
        static void enter(typename Parent::MachineType& m, bool deep) {
            enterAncestors(m,deep);
            if ( ! std::is_same<C, typename C::InitialState>() ) {
                std::cout << "  " << className<C>() << " has an initial transition...\n";
                Parent::currentState(m)->template transitionTo<typename C::InitialState>();
            }
        }
        
        // Default entry/exit handlers (does nothing unless non-virtually overridden)
        void entry() {}
        void exit() {}
        
        // The following Public Types are used to create decendents
        template<typename GC>                       // < GC=GrandChild state >
        using SubState = SubState_<C, GC>;
        
        template<typename GC, typename... Regions>  // < GC=GrandChild state >
        using ParallelState = ParallelState_<C, GC, Regions...>;
    };
    
    template < typename    M,   // type of state (M)achine
               typename... E >  // list of (E)vents handled by state machine
    struct Machine : E... {
        using BaseState = TopState_<M,E...>;
        using State     = BaseState;
        using Region    = State;
        
        Region  topLevelRegion { *this };
        
        void start() {
            std::cout << className<M>() << ".start()...\n";
            M::InitialState::enter(*this, false);
            std::cout << className<M>() << ".start() done.\n";
        }
        void stop() {
            std::cout << className<M>() << ".stop()...\n";
            topLevelRegion.self->leave(typename BaseState::HierarchyPos(), true);
            std::cout << className<M>() << ".stop() done.\n";
        }
        void handle( Event event ) {
            std::string originalState = className(*topLevelRegion.self);
            std::cout << originalState << "." << event.name << "()...\n";
            topLevelRegion.self->dispatch(event);
            std::cout << originalState << "." << event.name << "() done.\n";
        }
        operator M&() {
            return *static_cast<M*>(this);
        }
        
        template<typename S>
        using TopState = SubState_<BaseState,S>;
    };

public:
    template < typename E >  // list of (E)vents handled
    struct EventInterface : virtual BaseEventInterface {
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
        virtual void handle( Event f ) = 0;
    };
};

struct MyEvents1 : sm::EventInterface<MyEvents1> {
    virtual void f() { handle(&MyEvents1::f, __func__); }
    virtual void g() { handle<&MyEvents1::g>(__func__); }
};

struct MyEvents2 : sm::EventInterface<MyEvents2> {
    virtual void h(int x) { handle(&MyEvents2::h, x, __func__); }
    virtual void j(int x) { handle<int,&MyEvents2::j>(x, __func__); }
};

struct MyMachine : sm::Machine<MyMachine, MyEvents1, MyEvents2> {
    Region r1 { *this };
    Region r2 { *this };
    
    struct A : TopState<A> {
        void f() override {
            std::cout << "A::f()\n";
            transitionTo<D>( []{ std::cout << "Flying\n"; } );
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
        static void boo() {
            std::cout << "My Action\n";
        }
        void f() override {
            std::cout << "D::f()\n";
        }
        void h(int x) override {
            std::cout << "Got h(" << x << ")\n";
            transitionTo<G>( boo );
        }
        void entry() { std::cout << "D::in()\n"; }
        void exit() { std::cout << "D::out()\n"; }
    };
    struct R1;
    struct R2;
    struct G;
    struct H;
    struct HH;
    struct EE : C::ParallelState<EE,R1,R2> {
    };
    struct R1 : EE::Region<R1,&MyMachine::r1> {
        using InitialState = G;
    };
    struct R2 : EE::Region<R2,&MyMachine::r2> {
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
        using InitialState = HH;
    };
    struct HH : H::SubState<HH> {
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
