#ifndef queue_hh
#define queue_hh

#include "Function.h"

using Action = vl::Func<void ()>;

template <int cap>
class Queue {

    struct Item {
        unsigned long when;
        Action action;
    };

    int sze = 0;

    Item v[cap];

public:

    void in(unsigned long time, Action action) {
        if (sze == cap) for (;;);
        v[sze++] = {millis()+time, action};
    }

    void drain() {
        unsigned long now = millis();
        for (int i = 0; i < sze; ++i) {
            if (now >= v[i].when) {
                Action action = v[i].action;
                v[i] = v[--sze];
                action();
                return;
            }
        }
    }

};


#endif
