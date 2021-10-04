#ifndef APP_MSG_H
#define APP_MSG_H

#include <functional>
#include <string>
#include <memory>

enum EventType {
    E_RPC1 = 1
};

struct EventBase {
    EventType event_type;
    EventBase(EventType _type) : event_type(_type) {}
    virtual ~EventBase() {}
};

/* RPC 1 related structures */
struct RPC1AppMsgRequest {
    std::string value;
};
struct RPC1AppMsgResponse {
    std::string value;
};

using RPC1ResponseFn = std::function<void(std::shared_ptr<RPC1AppMsgResponse>, bool)>;

struct RPC1Event : public EventBase {
    std::shared_ptr<RPC1AppMsgRequest> request;
    RPC1ResponseFn response_fn;
    RPC1Event(std::shared_ptr<RPC1AppMsgRequest> _req, RPC1ResponseFn fn) : EventBase(E_RPC1), request(std::move(_req)), response_fn(std::move(fn)) {}
    ~RPC1Event() {
    }
};

#endif
