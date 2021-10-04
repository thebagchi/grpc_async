#include "services/anothersamplesvc_impl.hh"
#include <absl/strings/str_join.h>

void AnotherSampleSvcImpl::HandleRPC_1(
  const rpc::RPC1Request &request,
  rpc::RPC1Response * response,
  const std::function<void(grpc::Status)> & callback) {
  std::vector<std::string> items = {"Hello", request.name()};
  response->set_message(absl::StrJoin(items, " "));
  callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
}

void AnotherSampleSvcImpl::HandleRPC_2(
  const rpc::RPC2Request &request,
  rpc::RPC2Response * response,
  const std::function<void(grpc::Status)> & callback) {
  std::vector<std::string> items = {"Hello", request.name()};
  response->set_message(absl::StrJoin(items, " "));
  callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
}