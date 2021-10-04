#include <iostream>
#include <utility>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <absl/strings/str_join.h>
#include "proto/rpc.grpc.pb.h"
#include "proto/sample.grpc.pb.h"

#include "app_msg.h"
#include "processor.hpp"
#include "queue.hpp"

class RPCHandler {
 public:
  virtual ~RPCHandler() = default;

  virtual void Proceed() = 0;
};

#ifndef USING_TEMPLATE
class RPC1Handler : public RPCHandler {
 public:
  RPC1Handler(rpc::SampleSvc::AsyncService *service, grpc::ServerCompletionQueue *cq) :
    service_(service), cq_(cq), responder_(&ctx_), done_(false) {
    Register();
  }

  ~RPC1Handler() override = default;

 public:
  void Proceed() override {
    if (!done_) {
      std::cout << "Inside Proceed RPC1" << std::endl;
      new RPC1Handler(this->service_, this->cq_);
      done_.store(true);
      responder_.Finish(response_, grpc::Status::OK, this);
    } else {
      Cleanup();
    }
  }

 private:
  void Register() {
    std::cout << "Inside Register RPC1" << std::endl;
    service_->RequestRPC_1(&ctx_, &request_, &responder_, cq_, cq_, this);
  }

  void Cleanup() {
    std::cout << "Inside Cleanup RPC1" << std::endl;
    delete this;
  }

 private:
  rpc::SampleSvc::AsyncService *service_;
  grpc::ServerCompletionQueue *cq_;
  grpc::ServerContext ctx_;
  rpc::RPC1Request request_;
  rpc::RPC1Response response_;
  grpc::ServerAsyncResponseWriter<rpc::RPC1Response> responder_;
  std::atomic_bool done_;
};

class RPC2Handler : public RPCHandler {
 public:
  RPC2Handler(rpc::SampleSvc::AsyncService *service, grpc::ServerCompletionQueue *cq) :
    service_(service), cq_(cq), responder_(&ctx_), done_(false) {
    Register();
  }

  ~RPC2Handler() override = default;

 public:
  void Proceed() override {
    if (!done_) {
      std::cout << "Inside Proceed RPC2" << std::endl;
      new RPC2Handler(this->service_, this->cq_);
      done_.store(true);
      responder_.Finish(response_, grpc::Status::OK, this);
    } else {
      Cleanup();
    }
  }

 private:
  void Register() {
    std::cout << "Inside Register RPC2" << std::endl;
    service_->RequestRPC_2(&ctx_, &request_, &responder_, cq_, cq_, this);
  }

  void Cleanup() {
    std::cout << "Inside Cleanup RPC2" << std::endl;
    delete this;
  }

 private:
  rpc::SampleSvc::AsyncService *service_;
  grpc::ServerCompletionQueue *cq_;
  grpc::ServerContext ctx_;
  rpc::RPC2Request request_;
  rpc::RPC2Response response_;
  grpc::ServerAsyncResponseWriter<rpc::RPC2Response> responder_;
  std::atomic_bool done_;
};
#endif

#ifdef USING_TEMPLATE
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
  // Grpc server thread context 
  void Proceed() override {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (!done_) {
        // This new object is enqueud into the async server using the register call in the constructor 
      new RPCTHandler<Request, Response, Service>(this->service_, this->cq_, this->registrar_, this->handler_);
      // Grpc server thread context
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
#endif

class Handler {
 public:
  virtual ~Handler() = default;

  virtual void Install(grpc::ServerCompletionQueue* cq) = 0;
  virtual grpc::Service* Service() = 0;
};

class SampleSvcHandler : public Handler {
 public:
  ~SampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<rpc::RPC1Request, rpc::RPC1Response, rpc::SampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::SampleSvc::AsyncService::RequestRPC_1, std::bind(
        &SampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<rpc::RPC2Request, rpc::RPC2Response, rpc::SampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::SampleSvc::AsyncService::RequestRPC_2, std::bind(
        &SampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  grpc::Service* Service() final {
    return &service_;
  }

  virtual void HandleRPC_1(
    const rpc::RPC1Request& request, rpc::RPC1Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  virtual void HandleRPC_2(
    const rpc::RPC2Request& request, rpc::RPC2Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  rpc::SampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};

class AnotherSampleSvcHandler : public Handler {
 public:
  ~AnotherSampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<rpc::RPC1Request, rpc::RPC1Response, rpc::AnotherSampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::AnotherSampleSvc::AsyncService::RequestRPC_1, std::bind(
        &AnotherSampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<rpc::RPC2Request, rpc::RPC2Response, rpc::AnotherSampleSvc::AsyncService>(
      &this->service_, this->cq_, &rpc::AnotherSampleSvc::AsyncService::RequestRPC_2, std::bind(
        &AnotherSampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  grpc::Service* Service() final {
    return &service_;
  }

  virtual void HandleRPC_1(
    const rpc::RPC1Request& request, rpc::RPC1Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  virtual void HandleRPC_2(
    const rpc::RPC2Request& request, rpc::RPC2Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  rpc::AnotherSampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};

class SampleSvcHandlerImpl final : public SampleSvcHandler {
 public:
  ~SampleSvcHandlerImpl() override = default;
#if 1
  void HandleRPC_1(
    const rpc::RPC1Request& request, rpc::RPC1Response* response, const std::function<void(grpc::Status)>& callback) override {
    std::vector<std::string> items = {"Hello", request.name()};
    response->set_message(absl::StrJoin(items, " "));
    callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }

  void HandleRPC_2(
    const rpc::RPC2Request& request, rpc::RPC2Response* response, const std::function<void(grpc::Status)>& callback) override {
    std::vector<std::string> items = {"Hello", request.name()};
    response->set_message(absl::StrJoin(items, " "));
    callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }
#endif
};


class AnotherSampleSvcHandlerImpl final : public AnotherSampleSvcHandler {
 public:
  ~AnotherSampleSvcHandlerImpl() override = default;
#if 1
  void HandleRPC_1(
    const rpc::RPC1Request& request, rpc::RPC1Response* response, const std::function<void(grpc::Status)>& callback) override {
    std::vector<std::string> items = {"Hello", request.name(), "!!!"};
    response->set_message(absl::StrJoin(items, " "));
    callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }

  void HandleRPC_2(
    const rpc::RPC2Request& request, rpc::RPC2Response* response, const std::function<void(grpc::Status)>& callback) override {
    std::vector<std::string> items = {"Hello", request.name(), "!!!"};
    response->set_message(absl::StrJoin(items, " "));
    callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }
#endif
};

class ExampleSvcHandler : public Handler {
 public:
  ~ExampleSvcHandler() override = default;

 public:
  void Install(grpc::ServerCompletionQueue* cq) final {
    this->cq_ = cq;
    new RPCTHandler<sample::GRPC1Request, sample::GRPC1Response, sample::ExampleSvc::AsyncService>(
      &this->service_, this->cq_, &sample::ExampleSvc::AsyncService::RequestRPC_1, std::bind(
        &ExampleSvcHandler::HandleRPC_1, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    new RPCTHandler<sample::GRPC2Request, sample::GRPC2Response, sample::ExampleSvc::AsyncService>(
      &this->service_, this->cq_, &sample::ExampleSvc::AsyncService::RequestRPC_2, std::bind(
        &ExampleSvcHandler::HandleRPC_2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }

  grpc::Service* Service() final {
    return &service_;
  }

  virtual void HandleRPC_1(
    const sample::GRPC1Request& request, sample::GRPC1Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  virtual void HandleRPC_2(
    const sample::GRPC2Request& request, sample::GRPC2Response* response, const std::function<void(grpc::Status)>& callback) {
    callback(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, ""));
  }

  sample::ExampleSvc::AsyncService service_;
  grpc::ServerCompletionQueue* cq_ = nullptr;
};


class ExampleSvcHandlerImpl final : public ExampleSvcHandler {
 public:
  ~ExampleSvcHandlerImpl() override = default;
#if 1
  // Grpcs server thread context
  void HandleRPC_1(
    const sample::GRPC1Request& request, sample::GRPC1Response* response, const std::function<void(grpc::Status)>& callback) override {
      //std::vector<std::string> items = {"Hello", request.name(), "!!!"};
      //
      auto rpc1AppMsgRequest = std::make_shared<RPC1AppMsgRequest>();
      rpc1AppMsgRequest->value = std::string(request.name());

      auto response_fn = [response, callback](std::shared_ptr<RPC1AppMsgResponse> app_response, bool ok) {
          std::cout << "Calling custom HandleRPC_1 response fn " << std::endl;
          response->set_message(app_response->value);
          if (ok) {
              callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
          } else {
              callback(grpc::Status(::grpc::StatusCode::UNKNOWN, "ERROR"));
          }
      };

      auto rpc1Event = new RPC1Event(rpc1AppMsgRequest, response_fn);
      rpc1Event->event_type = E_RPC1;
      std::cout << "Pushing event to event queue " << std::endl;
      event_queue.PutElementInQueue(static_cast<EventBase*>(rpc1Event));
    
      //response->set_message(absl::StrJoin(items, " "));
      //callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }

  void HandleRPC_2(
    const sample::GRPC2Request& request, sample::GRPC2Response* response, const std::function<void(grpc::Status)>& callback) override {
    std::vector<std::string> items = {"Hello", request.name(), "!!!"};
    response->set_message(absl::StrJoin(items, " "));
    callback(grpc::Status(::grpc::StatusCode::OK, "OK"));
  }
#endif
};

class Server {
 public:
  explicit Server(std::string addr) : addr_(std::move(addr)) {
    // Empty
  }

  ~Server() {
    server_->Shutdown();
    cq_->Shutdown();
  }

  void AddHandler(Handler* handler) {
    handlers_.push_back(handler);
  }

  void Start() {
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

 private:
  void Install() {
#ifndef USING_TEMPLATE
    new RPC1Handler(&this->service_, this->cq_.get());
    new RPC2Handler(&this->service_, this->cq_.get());
#endif
    for (const auto &item : handlers_) {
      item->Install(this->cq_.get());
    }
#ifdef USING_TEMPLATE
    //new RPCTHandler<rpc::RPC1Request, rpc::RPC1Response, rpc::SampleSvc::AsyncService>(
    //  &this->service_, this->cq_.get(), &rpc::SampleSvc::AsyncService::RequestRPC_1);
    //new RPCTHandler<rpc::RPC2Request, rpc::RPC2Response, rpc::SampleSvc::AsyncService>(
    //  &this->service_, this->cq_.get(), &rpc::SampleSvc::AsyncService::RequestRPC_2);
#endif
  }

  [[noreturn]] [[noreturn]]
  void Loop() {
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

 private:
  std::string addr_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::vector<Handler*> handlers_;
};

int main() {

  rpc::SampleMessage message;
  std::string json_string;
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  google::protobuf::util::MessageToJsonString(message, &json_string, options);
  std::cout << json_string << std::endl;

  {
    auto msg = std::unique_ptr<rpc::SampleMessage>(rpc::SampleMessage().New());
    google::protobuf::util::JsonStringToMessage(json_string, msg.get());
    std::cout << "---------------------------------------------------------------------------------------" << std::endl;
    std::cout << msg->DebugString() << std::endl;
    std::cout << "---------------------------------------------------------------------------------------" << std::endl;
  }

  std::cout << "Hello, World!" << std::endl;
  SampleSvcHandlerImpl sampleSvcHandler;
  AnotherSampleSvcHandlerImpl anotherSampleSvcHandler;
  ExampleSvcHandlerImpl exampleSvcHandler;
  Server server("0.0.0.0:12345");
  server.AddHandler(&sampleSvcHandler);
  server.AddHandler(&anotherSampleSvcHandler);
  server.AddHandler(&exampleSvcHandler);
  std::thread t1(RunProcessor, nullptr);
  // Start event thread and establish message queue between them
  server.Start();
  t1.join();
  return 0;
}
