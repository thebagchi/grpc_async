#ifndef ANOTHERSAMPLESVC_HANDLER__INCLUDED
#define ANOTHERSAMPLESVC_HANDLER__INCLUDED

#include "framework/handler.hh"
#include "framework/rpc_handler.hh"
#include "proto/rpc.grpc.pb.h"

class AnotherSampleSvcHandler : public Handler {
 public:
  ~AnotherSampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<rpc::RPC1Request, rpc::RPC1Response, rpc::AnotherSampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::AnotherSampleSvc::AsyncService::RequestRPC_1, std::bind(
        &AnotherSampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<rpc::RPC2Request, rpc::RPC2Response, rpc::AnotherSampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::AnotherSampleSvc::AsyncService::RequestRPC_2, std::bind(
        &AnotherSampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  grpc::Service* Service() final {
    return &service_;
  }

  virtual void HandleRPC_1(
    const rpc::RPC1Request& request, rpc::RPC1Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  virtual void HandleRPC_2(
    const rpc::RPC2Request& request, rpc::RPC2Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  rpc::AnotherSampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};

#endif //ANOTHERSAMPLESVC_HANDLER__INCLUDED