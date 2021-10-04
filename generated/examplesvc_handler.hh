#ifndef EXAMPLESVC_HANDLER__INCLUDED
#define EXAMPLESVC_HANDLER__INCLUDED

#include "framework/handler.hh"
#include "framework/rpc_handler.hh"
#include "proto/sample.grpc.pb.h"

class ExampleSvcHandler : public Handler {
 public:
  ~ExampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<sample::GRPC1Request, sample::GRPC1Response, sample::ExampleSvc::AsyncService>(
      &this->service_, this->cq_, &sample::ExampleSvc::AsyncService::RequestRPC_1, std::bind(
        &ExampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<sample::GRPC2Request, sample::GRPC2Response, sample::ExampleSvc::AsyncService>(
      &this->service_, this->cq_, &sample::ExampleSvc::AsyncService::RequestRPC_2, std::bind(
        &ExampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  grpc::Service* Service() final {
    return &service_;
  }

  virtual void HandleRPC_1(
    const sample::GRPC1Request& request, sample::GRPC1Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  virtual void HandleRPC_2(
    const sample::GRPC2Request& request, sample::GRPC2Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  sample::ExampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};

#endif //EXAMPLESVC_HANDLER__INCLUDED