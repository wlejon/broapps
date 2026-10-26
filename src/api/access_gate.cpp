// The process-wide access gates (api.h, "Access gates") and the helpers
// bro.apps's natives use to ask them.
//
// The gates are set before realms run, but every realm's thread reads them, and
// a host that revokes a grant (or a test) may replace one while they do. So each
// lives behind a shared_ptr swapped under a mutex: a check copies the pointer
// under the lock and calls the gate outside it, so a gate that blocks to ask the
// user holds no lock, and a replaced gate stays alive until the calls already in
// it return. The flag spares the common case, no gate at all, the lock.

#include "api.h"
#include "host_apps_internal.h"

#include <atomic>
#include <mutex>

namespace broapps::api {

namespace {

template <class Fn>
struct GateSlot {
    std::mutex mu;
    std::shared_ptr<const Fn> fn;
    std::atomic<bool> active{false};

    void set(Fn gate) {
        const bool any = static_cast<bool>(gate);
        auto next = any ? std::make_shared<const Fn>(std::move(gate)) : nullptr;
        std::lock_guard lock(mu);
        fn = std::move(next);
        active.store(any, std::memory_order_release);
    }

    std::string ask(const std::string& what) {
        if (!active.load(std::memory_order_acquire)) return {};
        std::shared_ptr<const Fn> gate;
        {
            std::lock_guard lock(mu);
            gate = fn;
        }
        return gate ? (*gate)(what) : std::string();
    }
};

GateSlot<SpawnGate> g_spawn_gate;
GateSlot<WriteGate> g_config_write_gate;

} // namespace

void setSpawnGate(SpawnGate gate) { g_spawn_gate.set(std::move(gate)); }
void setConfigWriteGate(WriteGate gate) { g_config_write_gate.set(std::move(gate)); }

std::string spawnRefusal(const std::string& what) { return g_spawn_gate.ask(what); }
std::string configWriteRefusal(const std::string& what) { return g_config_write_gate.ask(what); }

std::string launchDescription(const std::string& id, const std::vector<std::string>& args) {
    auto quoted = [](const std::string& s) {
        if (!s.empty() && s.find_first_of(" \t\n\"") == std::string::npos) return s;
        std::string q = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') q += '\\';
            q += c;
        }
        return q + "\"";
    };
    std::string line = "launch " + quoted(id);
    for (const auto& a : args) {
        line += ' ';
        line += quoted(a);
    }
    return line;
}

Value newAccessError(const std::string& refusal) {
    ev::Persistent err(makeError(refusal));
    if (ev::isObject(err.get())) {
        // The value is made in a statement of its own: built inside the call,
        // it could move the receiver after err.get() was read.
        ev::Persistent code(ev::fromUtf8("EACCES"));
        err.set(ev::setProperty(err.get(), "code", code.get()));
    }
    return err.get();
}

Value throwAccessError(const std::string& refusal) {
    return ev::throwValue(newAccessError(refusal));
}

} // namespace broapps::api
