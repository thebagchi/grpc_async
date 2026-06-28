#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

#include "framework/builder.hpp"
#include "sample.framework.pb.h"

using sample::Chunk;
using sample::Summary;
using namespace std::chrono_literals;

class SampleServiceImpl final : public sample::SampleServiceBase {
public:
  // Unary: respond 100 ms after the request arrives.
  void OnUnary(grpc::ServerContext* context,
   const Chunk& request,
   const framework::Ptr<UnaryCall>& call) override {
    framework::Builder::Instance().DoAfter(100ms, [call, payload = request.payload()](bool ok) {
      Summary summary;
      summary.set_count(1);
      summary.set_note((ok ? "delayed: " : "cancelled: ") + payload);
      call->Finish(summary);
    });
  }

  // Server-streaming: emit one chunk per second for 1 minute then close.
  void OnServerStream(grpc::ServerContext* context,
   const Chunk& request,
   const framework::Ptr<ServerStreamCall>& call) override {
    struct State {
      std::uint64_t seq = 0;
      std::string payload;
      framework::Ptr<ServerStreamCall> call;
      bool closed = false;
      std::function<void(bool)> tick;
    };

    auto s = std::make_shared<State>();
    s->payload = request.payload();
    s->call = call;

    // Tick every second; stop once closed flag is set.
    s->tick = [s](bool ok) {
      if (!ok || s->closed)
        return;
      Chunk chunk;
      chunk.set_payload(s->payload + "-" + std::to_string(s->seq));
      chunk.set_seq(s->seq++);
      s->call->Write(chunk);
      framework::Builder::Instance().DoAfter(1s, s->tick);
    };

    framework::Builder::Instance().DoAfter(1s, s->tick);

    // After 1 minute, stop ticking and close the stream.
    framework::Builder::Instance().DoAfter(1min, [s](bool ok) {
      s->closed = true;
      s->call->Finish(grpc::Status::OK);
      s->tick = nullptr; // break the s→tick→s reference cycle
    });
  }

  // Client-streaming: count incoming chunks; finish after 10 or at half-close.
  void OnClientStream(
   grpc::ServerContext* context, const framework::Ptr<ClientStreamCall>& call) override {
    auto [msg, status] = call->Read();

    // Guard against the stale callback that arrives after we already finished.
    if (counts_.find(call.get()) == counts_.end() && !status.ok())
      return;

    auto& count = counts_[call.get()];

    if (!status.ok() || count >= 10) {
      Summary summary;
      summary.set_count(count);
      summary.set_note(count >= 10 ? "limit reached" : "received");
      counts_.erase(call.get());
      call->Finish(summary);
      return;
    }
    count++;
  }

  // Bidi: echo every message; idle watchdog of 1 minute.
  void OnBidiStream(
   grpc::ServerContext* context, const framework::Ptr<BidiStreamCall>& call) override {
    auto [msg, status] = call->Read();

    if (!status.ok()) {
      if (auto it = watchdogs_.find(call.get()); it != watchdogs_.end()) {
        it->second->Cancel();
        watchdogs_.erase(it);
      }
      call->Finish(grpc::Status::OK);
      return;
    }

    if (watchdogs_.find(call.get()) == watchdogs_.end()) {
      watchdogs_[call.get()] = framework::Builder::Instance().DoAfter(1min, [this, call](bool ok) {
        if (ok) {
          std::cout << "bidi stream timed out — closing\n";
          call->Finish(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "stream idle timeout"));
        }
        watchdogs_.erase(call.get());
      });
    }

    Chunk reply;
    reply.set_payload("echo: " + msg->payload());
    reply.set_seq(msg->seq());
    call->Write(reply);
  }
private:
  std::unordered_map<void*, std::uint64_t> counts_;
  std::unordered_map<void*, framework::Ptr<framework::Alarm>> watchdogs_;
};

int main() {
  const std::string address = "0.0.0.0:50051";
  auto& builder = framework::Builder::Instance();
  auto server = builder.AddServer(address);
  server->AddHandler(std::make_shared<SampleServiceImpl>());
  std::cout << "Server listening on " << address << std::endl;
  builder.Start();
  return 0;
}
