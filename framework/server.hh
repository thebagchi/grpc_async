#ifndef FRAMEWORK_SERVER__INCLUDED
#define FRAMEWORK_SERVER__INCLUDED

#include "framework/macros.hh"
#include "framework/handler.hh"
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>

class Server {
 public:
  explicit Server(std::string addr) : addr_(std::move(addr)) {
    // Empty
  }
  virtual ~Server();

 public:
  void AddHandler(Handler* handler);
  void Start();

 private:
  void Install();

  [[noreturn]] void Loop();

 private:
  std::string addr_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::vector<Handler*> handlers_;
};

#endif  //FRAMEWORK_SERVER__INCLUDED