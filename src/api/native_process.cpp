#include "host_apps_internal.h"
#include "host_class.h"
#include "arg_reader.h"
#include "object_builder.h"

namespace broapps::api {

namespace {

HostClass g_processClass;

struct HostProcessHandle {
    static constexpr uint64_t kMagic = 0x42524f4150505301ULL; // "BROAPPS\x01"
    uint64_t magic = kMagic;
    std::shared_ptr<ProcessHandle> ptr;
};

void hostProcessHandleDtor(void* p) {
    auto* h = static_cast<HostProcessHandle*>(p);
    delete h;
}

HostProcessHandle* processHandleOf(Value v) {
    if (!ev::isObject(v)) return nullptr;
    auto* p = static_cast<HostProcessHandle*>(g_processClass.unwrap(v));
    if (!p || p->magic != HostProcessHandle::kMagic) return nullptr;
    return p;
}

void decorateProcessProto(ObjectBuilder& proto) {
    // pid() method & aliases
    proto.def("pid", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.pid called on invalid object");
        return ev::fromDouble(static_cast<double>(h->ptr->pid()));
    });

    proto.def("getPid", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.getPid called on invalid object");
        return ev::fromDouble(static_cast<double>(h->ptr->pid()));
    });

    proto.accessor("processId", [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.processId called on invalid object");
        return ev::fromDouble(static_cast<double>(h->ptr->pid()));
    });

    // isRunning() method & is_running alias
    proto.def("isRunning", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.isRunning called on invalid object");
        return ev::fromBool(h->ptr->is_running());
    });

    proto.def("is_running", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.is_running called on invalid object");
        return ev::fromBool(h->ptr->is_running());
    });

    // terminate() method
    proto.def("terminate", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.terminate called on invalid object");
        return ev::fromBool(h->ptr->terminate());
    });

    // kill() method
    proto.def("kill", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.kill called on invalid object");
        return ev::fromBool(h->ptr->kill());
    });

    // wait([timeoutMs]) method
    proto.def("wait", 1, [](Value self, std::span<const Value> args) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.wait called on invalid object");
        ArgReader r(args);
        int timeoutMs = r.getInt(0, 5000);
        return ev::fromBool(h->ptr->wait_for_exit(std::chrono::milliseconds(timeoutMs)));
    });

    // exitCode() method
    proto.def("exitCode", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.exitCode called on invalid object");
        auto code = h->ptr->exit_code();
        if (code) return ev::fromDouble(static_cast<double>(*code));
        return ev::null();
    });

    // scopeId() method
    proto.def("scopeId", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.scopeId called on invalid object");
        return ev::fromUtf8(h->ptr->scope_id());
    });

    // launchId() method
    proto.def("launchId", 0, [](Value self, std::span<const Value>) -> Value {
        auto* h = processHandleOf(self);
        if (!h || !h->ptr) return ev::throwTypeError("ProcessHandle.launchId called on invalid object");
        return ev::fromDouble(static_cast<double>(h->ptr->launch_id()));
    });
}

} // namespace

Value wrapProcessHandle(std::shared_ptr<ProcessHandle> handle) {
    if (!handle) return ev::null();
    auto* wrapper = new HostProcessHandle();
    wrapper->ptr = std::move(handle);
    return g_processClass.make(wrapper, hostProcessHandleDtor);
}

void installProcessOnto(Value appsObj) {
    ev::Persistent appsP(appsObj);
    g_processClass.install("ProcessHandle", 0, nullptr, decorateProcessProto);
    ObjectBuilder apps(appsP.get());
    apps.set("ProcessHandle", g_processClass.constructor());
}

} // namespace broapps::api
