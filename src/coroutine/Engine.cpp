#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char currentStack;

    ctx.Hight = ctx.Low = StackBottom;
    if (&currentStack > StackBottom) {
        ctx.Hight = &currentStack;
    } else {
        ctx.Low = &currentStack;
    }

    auto stackSize = ctx.Hight - ctx.Low;
    if (stackSize > std::get<1>(ctx.Stack) || stackSize * 2 < std::get<1>(ctx.Stack)) {
        delete[] std::get<0>(ctx.Stack);
        std::get<0>(ctx.Stack) = new char[stackSize];
        std::get<1>(ctx.Stack) = stackSize;
    }

    std::memcpy(std::get<0>(ctx.Stack), ctx.Low, stackSize);
}

void Engine::Restore(context &ctx) {
    char currentStack;
    while (ctx.Low <= &currentStack && &currentStack <= ctx.Hight) {
        Restore(ctx);
    }

    std::memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.Hight - ctx.Low);
    std::longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    auto it = alive;
    while (it && it == cur_routine) {
        it = it->next;
    }

    if (it) {
        sched(it);
    }
}

void Engine::sched(void *routine_) {
    if (!routine_ || routine_ == cur_routine) {
        return;
    }

    if (cur_routine) {
        Store(*cur_routine);
        if (setjmp(cur_routine->Environment)) {
            return;
        }
    }
    cur_routine = (context *)routine_;
    Restore(*(context *)routine_);
}

} // namespace Coroutine
} // namespace Afina
