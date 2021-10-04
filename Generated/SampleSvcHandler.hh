#ifndef SAMPLESVC_HANDLER__INCLUDED
#define SAMPLESVC_HANDLER__INCLUDED

#include "Framework/Handler.hh"
#include "Framework/RPCHandler.hh"
#include "proto/rpc.grpc.pb.h"

class SampleSvcHandler : public Handler {
 public:
  ~SampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<rpc::RPC1Request, rpc::RPC1Response, rpc::SampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::SampleSvc::AsyncService::RequestRPC_1, std::bind(
        &SampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<rpc::RPC2Request, rpc::RPC2Response, rpc::SampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::SampleSvc::AsyncService::RequestRPC_2, std::bind(
        &SampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

  rpc::SampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};
#endif //SAMPLESVC_HANDLER__INCLUDED