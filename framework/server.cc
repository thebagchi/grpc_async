#include "server.hh"
#include "framework/rpc_handler.hh"

Server::~Server() {
  server_->Shutdown();
  cq_->Shutdown();
}

void Server::AddHandler(Handler *handler) {
  handlers_.push_back(handler);
}

void Server::Install() {
  for (const auto &item : handlers_) {
    item->Install(this->cq_.get());
  }
}

[[noreturn]]
void Server::Loop() {
  void *tag;
  bool ok;
  while (true) {
    if (GPR_UNLIKELY(!(cq_->Next(&tag, &ok)))) {
      continue;
    }
    if (GPR_UNLIKELY(!(ok))) {
      continue;
    }
    static_cast<RPCHandler *>(tag)->Proceed();
  }
}

void Server::Start() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr_, grpc::InsecureServerCredentials());
  for (const auto &item : handlers_) {
    builder.RegisterService(item->Service());
  }
  builder.SetResourceQuota(grpc::ResourceQuota().SetMaxThreads(1));
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  if (nullptr == server_) {
    std::cout << "Failed to start server on: " << addr_ << std::endl;
    return;
  }
  Install();
  Loop();
}