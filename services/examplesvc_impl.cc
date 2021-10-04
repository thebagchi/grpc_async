#include "services/examplesvc_impl.hh"
#include <absl/strings/str_join.h>

void ExampleSvcImpl::HandleRPC_1(
  const sample::GRPC1Request &request,
  sample::GRPC1Response * response,
  const std::function<void(grpc::Status)> & callback) {
  std::vector<std::string> items = {"Hello", request.name()};
  response->set_message(absl::StrJoin(items, " "));
  callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
}

void ExampleSvcImpl::HandleRPC_2(
  const sample::GRPC2Request &request,
  sample::GRPC2Response * response,
  const std::function<void(grpc::Status)> & callback) {
  std::vector<std::string> items = {"Hello", request.name()};
  response->set_message(absl::StrJoin(items, " "));
  callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
}