#include <iostream>
#include <random>
#include <grpcpp/grpcpp.h>
#include "proto/rpc.grpc.pb.h"
#include "proto/sample.grpc.pb.h"


const std::uint64_t loop = 1;

std::string gen_random() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::string temp;
  static const char characters[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

  int length = std::uniform_int_distribution<int>(0,100)(rng);
  temp.reserve(length);
  for (int i = 0; i < length; ++i) {
    int position = std::uniform_int_distribution<int>(0, sizeof(characters))(rng);
    temp += characters[position];
  }
  return temp;

}

int main() {
  std::cout << "Hello, World!" << std::endl;

  auto channel = grpc::CreateChannel("0.0.0.0:12345", grpc::InsecureChannelCredentials());
  auto stubSampleSvc = rpc::SampleSvc::NewStub(channel);
  auto stubAnotherSampleSvc = rpc::AnotherSampleSvc::NewStub(channel);
  auto stubExampleSvc = sample::ExampleSvc::NewStub(channel);
  for (auto i = 0; i < loop; i++) {
#if 1
    {
      grpc::ClientContext context;
      auto request = rpc::RPC1Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = rpc::RPC1Response().New();
      auto status = stubSampleSvc->RPC_1(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_1" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
    {
      grpc::ClientContext context;
      auto request = rpc::RPC2Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = rpc::RPC2Response().New();
      auto status = stubSampleSvc->RPC_2(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_2" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
    {
      grpc::ClientContext context;
      auto request = rpc::RPC1Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = rpc::RPC1Response().New();
      auto status = stubAnotherSampleSvc->RPC_1(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_1" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
    {
      grpc::ClientContext context;
      auto request = rpc::RPC2Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = rpc::RPC2Response().New();
      auto status = stubAnotherSampleSvc->RPC_2(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_2" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
#endif
    {
      grpc::ClientContext context;
      auto request = sample::GRPC1Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = sample::GRPC1Response().New();
      auto status = stubExampleSvc->RPC_1(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_1" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
#if 1
    {
      grpc::ClientContext context;
      auto request = sample::GRPC2Request();
      request.set_name(gen_random());
      std::cout << request.DebugString() << std::endl;
      auto response = sample::GRPC2Response().New();
      auto status = stubExampleSvc->RPC_2(&context, request, response);
      if (status.ok()) {
        std::cout << response->DebugString() << std::endl;
        std::cout << "Successfully called RPC_2" << std::endl;
      } else {
        std::cout << "Response status: " << status.error_details() << " code: " << status.error_code() << std::endl;
      }
    }
#endif
  }
  return 0;
}
