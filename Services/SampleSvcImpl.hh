#ifndef SAMPLESVC_IMPL__INCLUDED
#define SAMPLESVC_IMPL__INCLUDED

#include "Generated/SampleSvcHandler.hh"

class SampleSvcImpl : public SampleSvcHandler {
 public:
  ~SampleSvcImpl() override = default;

 public:
  void HandleRPC_1(
    const rpc::RPC1Request&, rpc::RPC1Response*, const std::function<void(grpc::Status)>&) override;

  void HandleRPC_2(
    const rpc::RPC2Request&, rpc::RPC2Response*, const std::function<void(grpc::Status)>&) override;
};
#endif //SAMPLESVC_IMPL__INCLUDED