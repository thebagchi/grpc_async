#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

#include "sample.framework.pb.h"

using sample::Chunk;
using sample::Summary;

// Concrete service implementation.  The generated SampleServiceBase provides
// Install(), Svc(), and the four pure-virtual handlers below; no framework
// template plumbing is needed here.
class SampleServiceImpl final : public sample::SampleServiceBase {
public:
  // Unary: echo the payload back inside a Summary.
  void OnUnary(grpc::ServerContext* context,
   const Chunk& request,
   const framework::Ptr<UnaryCall>& call) override {
    Summary summary;
    summary.set_count(1);
    summary.set_note("unary: " + request.payload());
    call->Finish(summary);
  }

  // Server-streaming: emit three derived chunks then close.
  void OnServerStream(grpc::ServerContext* context,
   const Chunk& request,
   const framework::Ptr<ServerStreamCall>& call) override {
    for (std::uint64_t i = 0; i < 3; ++i) {
      Chunk chunk;
      chunk.set_payload(request.payload() + "-" + std::to_string(i));
      chunk.set_seq(i);
      call->Write(chunk);
    }
    call->Finish(grpc::Status::OK);
  }

  // Client-streaming: count incoming chunks and reply once with a Summary.
  // Called per message; Read() returns (message, Status::OK) while data flows
  // and (nullptr, OUT_OF_RANGE) at half-close.
  void OnClientStream(
   grpc::ServerContext* context, const framework::Ptr<ClientStreamCall>& call) override {
    auto [msg, status] = call->Read();
    if (!status.ok()) {
      Summary summary;
      summary.set_count(counts_[call.get()]);
      summary.set_note("received");
      counts_.erase(call.get());
      call->Finish(summary);
      return;
    }
    counts_[call.get()]++;
  }

  // Bidi: echo every incoming chunk, close when the client half-closes.
  // Called per message; Read() returns (message, Status::OK) while data flows
  // and (nullptr, OUT_OF_RANGE) at half-close.
  void OnBidiStream(
   grpc::ServerContext* context, const framework::Ptr<BidiStreamCall>& call) override {
    auto [msg, status] = call->Read();
    if (!status.ok()) {
      call->Finish(grpc::Status::OK);
      return;
    }
    Chunk reply;
    reply.set_payload("echo: " + msg->payload());
    reply.set_seq(msg->seq());
    call->Write(reply);
  }
private:
  // Per-call message counter for client-streaming RPCs, keyed by call pointer.
  std::unordered_map<void*, std::uint64_t> counts_;
};

// Builds the server, registers the demo service, and blocks serving.
int main() {
  const std::string address = "0.0.0.0:50051";
  framework::Server server(address);
  server.AddHandler(std::make_shared<SampleServiceImpl>());
  std::cout << "Server listening on " << address << std::endl;
  server.Start();
  server.Wait();
  return 0;
}
