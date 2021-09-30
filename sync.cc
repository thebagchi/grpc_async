#include <iostream>
#include <memory>
#include "proto/rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class SampleSvcImpl final : public rpc::SampleSvc::Service {
  grpc::Status RPC_1(::grpc::ServerContext* context, const ::rpc::RPC1Request* request, ::rpc::RPC1Response* response) {
    std::cout << "Handling RPC_1" << std::endl;
    return grpc::Status::OK;
  }
  grpc::Status RPC_2(::grpc::ServerContext* context, const ::rpc::RPC2Request* request, ::rpc::RPC2Response* response) {
    std::cout << "Handling RPC_2" << std::endl;
    return grpc::Status::OK;
  }
};

int main() {
  std::cout << "Hello, World!" << std::endl;

  std::string addr = "0.0.0.0:12345";
  std::unique_ptr<grpc::Service> service = std::unique_ptr<grpc::Service>(new SampleSvcImpl());

  auto server = grpc::ServerBuilder()
    .AddListeningPort(addr, grpc::InsecureServerCredentials())
    .SetResourceQuota(grpc::ResourceQuota().SetMaxThreads(2))
    .RegisterService(service.get())
    .BuildAndStart();

  server->Wait();
  return 0;
}