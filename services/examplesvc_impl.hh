#ifndef EXAMPLESVC_IMPL__INCLUDED
#define EXAMPLESVC_IMPL__INCLUDED

#include "generated/examplesvc_handler.hh"

class ExampleSvcImpl : public ExampleSvcHandler {
 public:
  ~ExampleSvcImpl() override = default;

 public:
  void HandleRPC_1(
    const sample::GRPC1Request&, sample::GRPC1Response*, const std::function<void(grpc::Status)>&) override;

  void HandleRPC_2(
    const sample::GRPC2Request&, sample::GRPC2Response*, const std::function<void(grpc::Status)>&) override;
};
#endif //EXAMPLESVC_IMPL__INCLUDED