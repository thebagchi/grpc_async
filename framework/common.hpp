#ifndef FRAMEWORK_COMMON_HPP_INCLUDED
#define FRAMEWORK_COMMON_HPP_INCLUDED

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

namespace framework {

  // Lifecycle state of a streaming call.
  enum class StreamState {
    E_INITIALIZING, // StartCall in flight (client side only)
    E_ACTIVE,       // stream open
    E_DRAINING,     // half-closed; Finish in flight
    E_CLOSED,       // final status available
  };

  // Event delivered to a client-side stream callback.
  enum class StreamEvent {
    E_READY,    // StartCall completed, stream writable
    E_DATA,     // a response message arrived
    E_FINISHED, // final status available (check Status())
  };

  // Shorthand for shared ownership, used throughout the framework.
  template <typename T> using Ptr = std::shared_ptr<T>;

  // Base type for every object submitted to a completion queue as a void* tag.
  // The event loop casts the raw tag back to Handler* and calls Proceed (ok) or
  // Errored (not ok) without knowing the concrete type.
  class Handler {
  public:
    virtual ~Handler() = default;

    // Invoked when the completion queue delivers this tag with ok == true.
    virtual void Proceed() = 0;

    // Invoked when the completion queue delivers this tag with ok == false.
    virtual void Errored() = 0;
  };

  // One-shot completion-queue tag. Holds a callback that receives the ok flag,
  // keeps itself alive while the operation is outstanding, and releases that
  // self-reference once the queue delivers it.
  class CallbackTag final : public Handler, public std::enable_shared_from_this<CallbackTag> {
  public:
    static Handler* New(std::function<void(bool)> callback) {
      // Allocates a tag, seeds self-ownership, and returns the raw Handler pointer.
      auto instance = std::make_shared<CallbackTag>();
      instance->callback_ = std::move(callback);
      instance->self_ = instance;
      return static_cast<Handler*>(instance.get());
    }

    void Proceed() override {
      // Runs the callback with ok == true, then drops self-ownership.
      auto self = std::move(self_);
      if (callback_) {
        callback_(true);
      }
    }

    void Errored() override {
      // Runs the callback with ok == false, then drops self-ownership.
      auto self = std::move(self_);
      if (callback_) {
        callback_(false);
      }
    }
  private:
    std::function<void(bool)> callback_;
    Ptr<CallbackTag> self_;
  };

  template <typename T> Ptr<T> MakeMessage() {
    // Allocates a default-constructed protobuf message.
    return std::make_shared<T>();
  }

  inline Ptr<grpc::ServerContext> MakeServerContext() {
    // Allocates a server context.
    return std::make_shared<grpc::ServerContext>();
  }

  inline Ptr<grpc::ClientContext> MakeClientContext() {
    // Allocates a client context.
    return std::make_shared<grpc::ClientContext>();
  }

  inline void Drain(grpc::CompletionQueue* cq) {
    // Pulls tags off a completion queue and dispatches them on the calling
    // thread until the queue is shut down and fully drained. This is the single
    // polling loop shared by both Server and Client.
    void* tag = nullptr;
    bool ok = false;
    while (cq->Next(&tag, &ok)) {
      auto* handler = static_cast<Handler*>(tag);
      if (ok) {
        handler->Proceed();
      } else {
        handler->Errored();
      }
    }
  }

  // Guards the single drain loop for a completion queue so the queue is polled by
  // at most one thread. The first caller of Run drains the queue; while that loop
  // is active any other caller returns immediately instead of starting a second,
  // racing Next loop. This lets a process that hosts both a Server and a Client
  // share one queue (and one polling thread) safely.
  class Reactor {
  public:
    explicit Reactor(grpc::CompletionQueue* cq) : cq_(cq) {
      // Do Nothing
    }

    grpc::CompletionQueue* Queue() const {
      // Returns the completion queue this reactor drains.
      return cq_;
    }

    void Run() {
      // Drains the queue on the calling thread, unless another caller already
      // holds the drain, in which case it returns without polling.
      if (running_.exchange(true)) {
        return;
      }
      Drain(cq_);
      running_.store(false);
    }
  private:
    grpc::CompletionQueue* cq_;
    std::atomic_bool running_{ false };
  };

} // namespace framework

#endif // FRAMEWORK_COMMON_HPP_INCLUDED
