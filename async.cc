#include <iostream>
#include <utility>
#include <grpcpp/grpcpp.h>
#include "proto/rpc.grpc.pb.h"

class RPCHandler {
 public:
  RPCHandler(rpc::SampleSvc::Service* service, grpc::ServerCompletionQueue* cq) :  {

  }

 private:
  rpc::SampleSvc::Service* service_;
  grpc::ServerCompletionQueue* cq_;
};

class Server {
 public:
  explicit Server(std::string addr) : addr_(std::move(addr)) {
    // Empty
  }

  ~Server() {
    server_->Shutdown();
    cq_->Shutdown();
  }

  void Start() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr_, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
    Loop();
  }

 private:
  void Loop() {

  }

 private:
  std::string addr_;
  std::unique_ptr<grpc::Server> server_;
  rpc::SampleSvc::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
};

int main() {
  std::cout << "Hello, World!" << std::endl;
  return 0;
}
