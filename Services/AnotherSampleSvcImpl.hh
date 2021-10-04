#ifndef ANOTHERSAMPLESVC_IMPL__INCLUDED
#define ANOTHERSAMPLESVC_IMPL__INCLUDED

#include "Generated/AnotherSampleSvcHandler.hh"

class AnotherSampleSvcImpl : public AnotherSampleSvcHandler {
 public:
  ~AnotherSampleSvcImpl() override = default;

 public:
  void HandleRPC_1(
    const rpc::RPC1Request&, rpc::RPC1Response*, const std::function<void(grpc::Status)>&) override;

  void HandleRPC_2(
    const rpc::RPC2Request&, rpc::RPC2Response*, const std::function<void(grpc::Status)>&) override;
};
#endif //ANOTHERSAMPLESVC_IMPL__INCLUDED