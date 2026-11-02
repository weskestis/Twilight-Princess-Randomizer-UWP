#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

struct OSMutex { std::recursive_mutex native; };
struct OSCond { std::condition_variable_any native; };
struct OSThread {};
thread_local std::function<void(OSMutex*)> afterUnlock;
void OSLockMutex(OSMutex* m) { m->native.lock(); }
void OSUnlockMutex(OSMutex* m) {
    m->native.unlock();
    if (afterUnlock) {
        auto hook = std::move(afterUnlock);
        afterUnlock = {};
        hook(m);
    }
}
bool OSTryLockMutex(OSMutex* m) { return m->native.try_lock(); }
void OSWaitCond(OSCond* c, OSMutex* m) {
    std::unique_lock lock(m->native, std::adopt_lock);
    c->native.wait(lock);
    lock.release();
}
void OSSignalCond(OSCond* c) { c->native.notify_all(); }
void OSInitMutex(OSMutex*) {}
void OSInitCond(OSCond*) {}
