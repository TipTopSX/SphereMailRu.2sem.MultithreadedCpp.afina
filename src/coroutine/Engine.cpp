#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstring>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char currentStack;

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
    if (it && it == cur_routine) {
        it = it->next;
    }

    if (it) {
        sched(it);
    }
}

void Engine::sched(void *routine_) {
    if (!routine_ || routine_ == cur_routine) {
        return yield();
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

void Engine::block(void *routine_) {
    auto routine = cur_routine;
    if (routine_) {
        routine = (context *)routine_;
    }
    if (routine && !routine->blocked) {
        routine->blocked = true;
        // Remove from alive
        if (routine->prev) {
            routine->prev->next = routine->next;
        }
        if (routine->next) {
            routine->next->prev = routine->prev;
        }
        // Move to blocked
        if (blocked) {
            blocked->prev = routine;
        }
        routine->next = blocked;
        routine->prev = nullptr;
        blocked = routine;
        if (routine == cur_routine) {
            if (cur_routine && cur_routine != idle_ctx) {
                if (setjmp(cur_routine->Environment) > 0) {
                    return;
                }
                Store(*cur_routine);
            }
            cur_routine = nullptr;
            Restore(*idle_ctx);
        }
    }
}

void Engine::unblock(void *routine_) {
    auto routine = (context *)routine_;
    if (routine && routine->blocked) {
        // Remove from blocked
        if (routine->prev) {
            routine->prev->next = routine->next;
        }
        if (routine->next) {
            routine->next->prev = routine->prev;
        }
        // Move to alive
        if (alive) {
            alive->prev = routine;
        }
        routine->next = alive;
        routine->prev = nullptr;
        alive = routine;
    }
}

} // namespace Coroutine
} // namespace Afina
