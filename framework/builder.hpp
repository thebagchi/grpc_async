#ifndef FRAMEWORK_BUILDER_HPP_INCLUDED
#define FRAMEWORK_BUILDER_HPP_INCLUDED

#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "framework/client.hpp"
#include "framework/server.hpp"

namespace framework {

  // Singleton that builds one gRPC server for all registered addresses, backed
  // by a single shared completion queue.  All servers and clients post events
  // to the same queue; a single thread drains it with no background threads.
  //
  // Typical usage:
  //
  //   auto& b = framework::Builder::Instance();
  //   auto  s = b.AddServer("0.0.0.0:50051");
  //   s->AddHandler(std::make_shared<MyService>());
  //   auto  c = b.AddClient();
  //   b.Start();                        // blocks until Stop()
  //
  // Inside any handler callback:
  //
  //   framework::Builder::Instance().DoAfter(500ms, [](bool ok) { ... });
  //
  class Builder {
  public:
    static Builder& Instance() {
      static Builder instance;
      return instance;
    }

    // Registers a server at the given address.
    // Call server->AddHandler(...) before Start().
    Ptr<Server> AddServer(const std::string& address) {
      auto s = std::make_shared<Server>(address);
      servers_.push_back(s);
      return s;
    }

    // Creates a Client that shares the single completion queue once Start()
    // is called.  Streams may only be opened from inside a Start() callback.
    Ptr<Client> AddClient() {
      auto c = std::make_shared<Client>();
      clients_.push_back(c);
      return c;
    }

    // Schedules a lambda to run after the given delay.  Must be called from
    // inside a callback (i.e. after Start() has begun running the reactor).
    template <typename Duration>
    Ptr<Alarm> DoAfter(const Duration& delay, std::function<void(bool)> callback) {
      assert(reactor_ && "DoAfter must be called after Start()");
      return Alarm::New(
       reactor_->Queue(), std::chrono::system_clock::now() + delay, std::move(callback));
    }

    // Builds one gRPC server for all registered addresses with one shared
    // completion queue, rebinds all clients to it, then blocks running the
    // reactor until Stop() is called.
    void Start() {
      if (!servers_.empty()) {
        grpc::ServerBuilder grpc_builder;
        for (auto& s : servers_) {
          s->Register(grpc_builder);
        }
        cq_ = grpc_builder.AddCompletionQueue();
        server_ = grpc_builder.BuildAndStart();
        if (server_) {
          reactor_ = std::make_shared<Reactor>(cq_.get());
          for (auto& s : servers_) {
            s->Install(cq_.get());
          }
        }
      }

      if (reactor_) {
        for (auto& c : clients_) {
          c->SetReactor(reactor_);
        }
      } else if (!clients_.empty()) {
        reactor_ = clients_.front()->GetReactor();
      }

      if (reactor_) {
        reactor_->Run();
      }
    }

    // Shuts down the gRPC server and completion queue, unblocking Start().
    void Stop() {
      if (server_) {
        server_->Shutdown();
      }
      if (cq_) {
        cq_->Shutdown();
      }
      for (auto& c : clients_) {
        c->Shutdown();
      }
    }
  private:
    Builder() = default;
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    std::vector<Ptr<Server>> servers_;
    std::vector<Ptr<Client>> clients_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    Ptr<Reactor> reactor_;
  };

} // namespace framework

#endif // FRAMEWORK_BUILDER_HPP_INCLUDED
