#include <iostream>
#include <utility>
#include <grpcpp/grpcpp.h>
#include "proto/rpc.grpc.pb.h"

class RPCHandler {
 public:
  virtual ~RPCHandler() = default;
  virtual void Proceed() = 0;
};

class RPC1Handler : public RPCHandler {
 public:
  RPC1Handler(rpc::SampleSvc::AsyncService* service, grpc::ServerCompletionQueue* cq) :
    service_(service), cq_(cq), responder_(&ctx_), done_(false) {
    Register();
  }

  ~RPC1Handler() override = default;

 public:
  void Proceed() override {
    std::cout << "Inside Proceed" << std::endl;
    if (!done_) {
      new RPC1Handler(this->service_, this->cq_);
      done_.store(true);
      responder_.Finish(response_, grpc::Status::OK, this);
    } else {
      Cleanup();
    }
  }
 private:
  void Register() {
    service_->RequestRPC_1(&ctx_, &request_, &responder_, cq_, cq_, this);
  }
  void Cleanup() {
    delete this;
  }
 private:
  rpc::SampleSvc::AsyncService* service_;
  grpc::ServerCompletionQueue* cq_;
  grpc::ServerContext ctx_;
  rpc::RPC1Request request_;
  rpc::RPC1Response response_;
  grpc::ServerAsyncResponseWriter<rpc::RPC1Response> responder_;
  std::atomic_bool done_;
};

class RPC2Handler : public RPCHandler {
 public:
  RPC2Handler(rpc::SampleSvc::AsyncService* service, grpc::ServerCompletionQueue* cq) :
    service_(service), cq_(cq), responder_(&ctx_), done_(false) {
    Register();
  }

  ~RPC2Handler() override = default;

 public:
  void Proceed() override {
    std::cout << "Inside Proceed" << std::endl;
    if (!done_) {
      new RPC2Handler(this->service_, this->cq_);
      done_.store(true);
      responder_.Finish(response_, grpc::Status::OK, this);
    } else {
      Cleanup();
    }
  }
 private:
  void Register() {
    service_->RequestRPC_2(&ctx_, &request_, &responder_, cq_, cq_, this);
  }
  void Cleanup() {
    delete this;
  }
 private:
  rpc::SampleSvc::AsyncService* service_;
  grpc::ServerCompletionQueue* cq_;
  grpc::ServerContext ctx_;
  rpc::RPC2Request request_;
  rpc::RPC2Response response_;
  grpc::ServerAsyncResponseWriter<rpc::RPC2Response> responder_;
  std::atomic_bool done_;
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
    Install();
    Loop();
  }

 private:
  void Install() {
    new RPC1Handler(&this->service_, this->cq_.get());
    new RPC2Handler(&this->service_, this->cq_.get());
  }
  void Loop() {
    void* tag;
    bool ok;
    while(true) {
      GPR_ASSERT(cq_->Next(&tag, &ok));
      GPR_ASSERT(ok);
      static_cast<RPCHandler*>(tag)->Proceed();
    }
  }

 private:
  std::string addr_;
  std::unique_ptr<grpc::Server> server_;
  rpc::SampleSvc::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
};

int main() {
  std::cout << "Hello, World!" << std::endl;

  Server server("0.0.0.0:12345");
  server.Start();
  return 0;
}
