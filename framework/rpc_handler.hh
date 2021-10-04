#ifndef FRAMEWORK_RPC_HANDLER__INCLUDED
#define FRAMEWORK_RPC_HANDLER__INCLUDED

#include <grpcpp/grpcpp.h>

class RPCHandler {
 public:
  virtual ~RPCHandler() = default;

  virtual void Proceed() = 0;
};

template<typename Request, typename Response, typename Service>
class RPCTHandler : public RPCHandler {
  using Responder = grpc::ServerAsyncResponseWriter<Response>;
  using Registrar = std::function<void(
    Service *, grpc::ServerContext *, Request *, Responder *, grpc::CompletionQueue *, grpc::ServerCompletionQueue *,
    void *)>;
  using Callback  = std::function<void(grpc::Status)>;
  using Handler   = std::function<void(const Request&, Response*, const Callback&)>;
 public:
  RPCTHandler(Service *service, grpc::ServerCompletionQueue *cq, Registrar registrar, Handler handler) :
    service_(service), cq_(cq), responder_(&ctx_), done_(false), registrar_(std::move(registrar)), handler_(handler) {
    Register();
  }

 public:
  void Proceed() override {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (!done_) {
      new RPCTHandler<Request, Response, Service>(this->service_, this->cq_, this->registrar_, this->handler_);
      handler_(request_, &response_, [=](grpc::Status status) {
        done_.store(true);
        responder_.Finish(response_, status, this);
      });
    } else {
      Cleanup();
    }
  }

  void Cleanup() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    delete this;
  }

 private:
  void Register() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    this->registrar_(service_, &ctx_, &request_, &responder_, cq_, cq_, this);
  }

 private:
  Service *service_;
  Request request_;
  Response response_;
  Responder responder_;
  grpc::ServerCompletionQueue *cq_;
  grpc::ServerContext ctx_;
  std::atomic_bool done_;
  Registrar registrar_;
  Handler handler_;
};

#endif //FRAMEWORK_RPC_HANDLER__INCLUDED