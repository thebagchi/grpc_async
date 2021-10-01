#include <iostream>
#include <grpcpp/grpcpp.h>
#include "proto/rpc.grpc.pb.h"

const std::uint64_t loop = 1;

int main() {
  std::cout << "Hello, World!" << std::endl;

  auto channel = grpc::CreateChannel("0.0.0.0:12345", grpc::InsecureChannelCredentials());
  auto stub = rpc::SampleSvc::NewStub(channel);
  for (auto i = 0; i < loop; i++) {
    {
      grpc::ClientContext context;
      auto request = rpc::RPC1Request();
      auto response = rpc::RPC1Response().New();
      auto status = stub->RPC_1(&context, request, response);
      if (status.ok()) {
        std::cout << "Successfully called RPC_1" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
    {
      grpc::ClientContext context;
      auto request = rpc::RPC2Request();
      auto response = rpc::RPC2Response().New();
      auto status = stub->RPC_2(&context, request, response);
      if (status.ok()) {
        std::cout << "Successfully called RPC_2" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
  }
  return 0;
}