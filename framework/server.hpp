#ifndef FRAMEWORK_SERVER_HPP_INCLUDED
#define FRAMEWORK_SERVER_HPP_INCLUDED

#include <deque>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "framework/common.hpp"

namespace framework {

  // ── Service base ─────────────────────────────────────────────────────────────
  //
  // Application services subclass this. Install registers the first receiver for
  // each RPC method; Svc returns the gRPC service to register with the builder.
  class Service {
  public:
    virtual ~Service() = default;

    // Registers the first call object for every RPC the service exposes.
    virtual void Install(grpc::ServerCompletionQueue* cq) = 0;

    // Returns the underlying gRPC service for ServerBuilder registration.
    virtual grpc::Service* Svc() = 0;
  };

  // ── UnaryCall ────────────────────────────────────────────────────────────────
  //
  // One request, one response.
  template <typename AsyncService, typename Request, typename Response>
  class UnaryCall final
      : public Handler,
        public std::enable_shared_from_this<UnaryCall<AsyncService, Request, Response>> {
  public:
    using Self = UnaryCall<AsyncService, Request, Response>;
    using Responder = grpc::ServerAsyncResponseWriter<Response>;
    using Registrar = std::function<void(AsyncService*,
     grpc::ServerContext*,
     Request*,
     Responder*,
     grpc::CompletionQueue*,
     grpc::ServerCompletionQueue*,
     void*)>;
    using Callback = std::function<void(const Request&, const Ptr<Self>&)>;

    static void New(AsyncService* service,
     grpc::ServerCompletionQueue* cq,
     Registrar registrar,
     Callback callback) {
      // Registers a fresh receiver and arms it for the next incoming call.
      auto instance = std::make_shared<Self>();
      instance->service_ = service;
      instance->cq_ = cq;
      instance->registrar_ = std::move(registrar);
      instance->callback_ = std::move(callback);
      instance->responder_ = std::make_unique<Responder>(&instance->context_);
      instance->self_ = instance;
      instance->registrar_(service,
       &instance->context_,
       &instance->request_,
       instance->responder_.get(),
       cq,
       cq,
       instance.get());
    }

    void Proceed() override {
      // A call arrived: spawn the successor receiver, then deliver to the user.
      auto self = this->shared_from_this();
      self_.reset();
      New(service_, cq_, registrar_, callback_);
      callback_(request_, self);
    }

    void Errored() override {
      // Registration failed (server shutting down): drop self-ownership.
      self_.reset();
    }

    void Finish(const Response& response, const grpc::Status& status = grpc::Status::OK) {
      // Sends the single response and closes the call.
      auto self = this->shared_from_this();
      responder_->Finish(response, status, CallbackTag::New([self](bool) {
        // Do Nothing
      }));
    }

    void FinishWithError(const grpc::Status& status) {
      // Closes the call with an error status and no response message.
      auto self = this->shared_from_this();
      responder_->FinishWithError(status, CallbackTag::New([self](bool) {
        // Do Nothing
      }));
    }

    grpc::Status Status() const {
      // Returns the current call status.
      return status_;
    }

    grpc::ServerContext* Context() {
      // Exposes the server context for metadata, peer info, and cancellation.
      return &context_;
    }
  private:
    grpc::Status status_;
    AsyncService* service_ = nullptr;
    grpc::ServerCompletionQueue* cq_ = nullptr;
    Registrar registrar_;
    Callback callback_;
    grpc::ServerContext context_;
    Request request_;
    std::unique_ptr<Responder> responder_;
    Ptr<Self> self_;
  };

  // ── ServerStreamCall ─────────────────────────────────────────────────────────
  //
  // One request, a stream of responses. Writes are queued and pumped one at a
  // time (gRPC allows only one outstanding write); Finish runs after they drain.
  template <typename AsyncService, typename Request, typename Response>
  class ServerStreamCall final
      : public Handler,
        public std::enable_shared_from_this<ServerStreamCall<AsyncService, Request, Response>> {
  public:
    using Self = ServerStreamCall<AsyncService, Request, Response>;
    using Writer = grpc::ServerAsyncWriter<Response>;
    using Registrar = std::function<void(AsyncService*,
     grpc::ServerContext*,
     Request*,
     Writer*,
     grpc::CompletionQueue*,
     grpc::ServerCompletionQueue*,
     void*)>;
    using Callback = std::function<void(const Request&, const Ptr<Self>&)>;

    static void New(AsyncService* service,
     grpc::ServerCompletionQueue* cq,
     Registrar registrar,
     Callback callback) {
      // Registers a fresh receiver and arms it for the next incoming call.
      auto instance = std::make_shared<Self>();
      instance->service_ = service;
      instance->cq_ = cq;
      instance->registrar_ = std::move(registrar);
      instance->callback_ = std::move(callback);
      instance->writer_ = std::make_unique<Writer>(&instance->context_);
      instance->self_ = instance;
      instance->registrar_(service,
       &instance->context_,
       &instance->request_,
       instance->writer_.get(),
       cq,
       cq,
       instance.get());
    }

    void Proceed() override {
      // A call arrived: spawn the successor receiver, then deliver to the user.
      auto self = this->shared_from_this();
      self_.reset();
      New(service_, cq_, registrar_, callback_);
      callback_(request_, self);
    }

    void Errored() override {
      // Registration failed (server shutting down): drop self-ownership.
      self_.reset();
    }

    void Write(const Response& response) {
      // Queues a response message for writing.
      queue_.push_back(response);
      _Pump();
    }

    void Finish(const grpc::Status& status = grpc::Status::OK) {
      // Requests stream closure once all queued writes have been flushed.
      finishing_ = true;
      status_ = status;
      _Pump();
    }

    grpc::Status Status() const {
      // Returns the finish status (valid once State() == E_CLOSED).
      return status_;
    }

    StreamState State() const {
      // Returns E_ACTIVE while the stream is open, E_CLOSED once Finish has completed.
      return state_;
    }

    grpc::ServerContext* Context() {
      // Exposes the server context for metadata, peer info, and cancellation.
      return &context_;
    }
  private:
    void _Pump() {
      // Drives the single-flight write queue, then Finish once drained.
      if (writing_ || finished_) {
        return;
      }
      if (!queue_.empty()) {
        writing_ = true;
        pending_ = queue_.front();
        queue_.pop_front();
        auto self = this->shared_from_this();
        writer_->Write(pending_, CallbackTag::New([self](bool ok) {
          self->writing_ = false;
          if (ok) {
            self->_Pump();
          } else {
            self->_Finish(grpc::Status(grpc::StatusCode::CANCELLED, "write failed"));
          }
        }));
      } else if (finishing_) {
        _Finish(status_);
      }
    }

    void _Finish(const grpc::Status& status) {
      // Issues the terminal Finish exactly once, then marks the stream closed.
      if (finished_) {
        return;
      }
      finished_ = true;
      status_ = status;
      auto self = this->shared_from_this();
      writer_->Finish(status, CallbackTag::New([self](bool) {
        self->state_ = StreamState::E_CLOSED;
      }));
    }

    AsyncService* service_ = nullptr;
    grpc::ServerCompletionQueue* cq_ = nullptr;
    Registrar registrar_;
    Callback callback_;
    grpc::ServerContext context_;
    Request request_;
    std::unique_ptr<Writer> writer_;
    std::deque<Response> queue_;
    Response pending_;
    grpc::Status status_;
    StreamState state_ = StreamState::E_ACTIVE;
    bool writing_ = false;
    bool finishing_ = false;
    bool finished_ = false;
    Ptr<Self> self_;
  };

  // ── ClientStreamCall ─────────────────────────────────────────────────────────
  //
  // A stream of requests, one response. The framework reads one message at a time
  // into a buffer and fires the Callback on each arrival. The user calls Read()
  // to take ownership of that message and arm the next gRPC read. At half-close
  // Read() returns nullptr; the user then sends the single response via Finish.
  template <typename AsyncService, typename Request, typename Response>
  class ClientStreamCall final
      : public Handler,
        public std::enable_shared_from_this<ClientStreamCall<AsyncService, Request, Response>> {
  public:
    using Self = ClientStreamCall<AsyncService, Request, Response>;
    using Reader = grpc::ServerAsyncReader<Response, Request>;
    using Registrar = std::function<void(AsyncService*,
     grpc::ServerContext*,
     Reader*,
     grpc::CompletionQueue*,
     grpc::ServerCompletionQueue*,
     void*)>;
    using Callback = std::function<void(const Ptr<Self>&)>;

    static void New(AsyncService* service,
     grpc::ServerCompletionQueue* cq,
     Registrar registrar,
     Callback callback) {
      // Registers a fresh receiver and arms it for the next incoming call.
      auto instance = std::make_shared<Self>();
      instance->service_ = service;
      instance->cq_ = cq;
      instance->registrar_ = std::move(registrar);
      instance->callback_ = std::move(callback);
      instance->reader_ = std::make_unique<Reader>(&instance->context_);
      instance->self_ = instance;
      instance->registrar_(
       service, &instance->context_, instance->reader_.get(), cq, cq, instance.get());
    }

    void Proceed() override {
      // A call arrived: spawn the successor receiver, then begin the first read.
      auto self = this->shared_from_this();
      self_.reset();
      New(service_, cq_, registrar_, callback_);
      _BeginRead();
    }

    void Errored() override {
      // Registration failed (server shutting down): drop self-ownership.
      self_.reset();
    }

    std::tuple<Ptr<Request>, grpc::Status> Read() {
      // Returns the buffered message paired with a status. Status is OK when a
      // message is present; OUT_OF_RANGE signals a clean half-close (EOF).
      // Arms the next gRPC read for any non-EOF invocation.
      auto msg = std::move(pending_);
      auto status = status_;
      if (!done_) {
        _BeginRead();
      }
      return { msg, status };
    }

    void Finish(const Response& response, const grpc::Status& status = grpc::Status::OK) {
      // Sends the single response and closes the call.
      auto self = this->shared_from_this();
      reader_->Finish(response, status, CallbackTag::New([self](bool) {
        // Do Nothing
      }));
    }

    grpc::Status Status() const {
      // Returns the current read status: OK while data flows, OUT_OF_RANGE at half-close.
      return status_;
    }

    grpc::ServerContext* Context() {
      // Exposes the server context for metadata, peer info, and cancellation.
      return &context_;
    }
  private:
    void _BeginRead() {
      // Issues one async read; on arrival stores the message and status, fires the callback.
      auto self = this->shared_from_this();
      auto message = MakeMessage<Request>();
      reader_->Read(message.get(), CallbackTag::New([self, message](bool ok) {
        if (ok) {
          self->pending_ = message;
          self->status_ = grpc::Status::OK;
        } else {
          self->done_ = true;
          self->status_ = grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "end of stream");
        }
        self->callback_(self);
      }));
    }

    AsyncService* service_ = nullptr;
    grpc::ServerCompletionQueue* cq_ = nullptr;
    Registrar registrar_;
    Callback callback_;
    grpc::ServerContext context_;
    std::unique_ptr<Reader> reader_;
    Ptr<Request> pending_;
    grpc::Status status_;
    bool done_ = false;
    Ptr<Self> self_;
  };

  // ── BidiCall ─────────────────────────────────────────────────────────────────
  //
  // A stream of requests and a stream of responses. The framework reads one
  // message at a time into a buffer and fires the Callback on each arrival. The
  // user calls Read() to take ownership of that message and arm the next gRPC
  // read. At half-close Read() returns nullptr. Writes are queued single-flight
  // and Finish runs once writes drain.
  template <typename AsyncService, typename Request, typename Response>
  class BidiCall final
      : public Handler,
        public std::enable_shared_from_this<BidiCall<AsyncService, Request, Response>> {
  public:
    using Self = BidiCall<AsyncService, Request, Response>;
    using Stream = grpc::ServerAsyncReaderWriter<Response, Request>;
    using Registrar = std::function<void(AsyncService*,
     grpc::ServerContext*,
     Stream*,
     grpc::CompletionQueue*,
     grpc::ServerCompletionQueue*,
     void*)>;
    using Callback = std::function<void(const Ptr<Self>&)>;

    static void New(AsyncService* service,
     grpc::ServerCompletionQueue* cq,
     Registrar registrar,
     Callback callback) {
      // Registers a fresh receiver and arms it for the next incoming call.
      auto instance = std::make_shared<Self>();
      instance->service_ = service;
      instance->cq_ = cq;
      instance->registrar_ = std::move(registrar);
      instance->callback_ = std::move(callback);
      instance->stream_ = std::make_unique<Stream>(&instance->context_);
      instance->self_ = instance;
      instance->registrar_(
       service, &instance->context_, instance->stream_.get(), cq, cq, instance.get());
    }

    void Proceed() override {
      // A call arrived: spawn the successor receiver, then begin the first read.
      auto self = this->shared_from_this();
      self_.reset();
      New(service_, cq_, registrar_, callback_);
      _BeginRead();
    }

    void Errored() override {
      // Registration failed (server shutting down): drop self-ownership.
      self_.reset();
    }

    std::tuple<Ptr<Request>, grpc::Status> Read() {
      // Returns the buffered message paired with a status. Status is OK when a
      // message is present; OUT_OF_RANGE signals a clean half-close (EOF).
      // Arms the next gRPC read for any non-EOF invocation.
      auto msg = std::move(pending_);
      auto status = status_;
      if (!done_) {
        _BeginRead();
      }
      return { msg, status };
    }

    void Write(const Response& response) {
      // Queues a response message for writing.
      queue_.push_back(response);
      _Pump();
    }

    void Finish(const grpc::Status& status = grpc::Status::OK) {
      // Requests stream closure once all queued writes have been flushed.
      finishing_ = true;
      finish_status_ = status;
      _Pump();
    }

    grpc::Status Status() const {
      // Returns the current read status: OK while data flows, OUT_OF_RANGE at half-close.
      return status_;
    }

    grpc::ServerContext* Context() {
      // Exposes the server context for metadata, peer info, and cancellation.
      return &context_;
    }
  private:
    void _BeginRead() {
      // Issues one async read; on arrival stores the message and status, fires the callback.
      auto self = this->shared_from_this();
      auto message = MakeMessage<Request>();
      stream_->Read(message.get(), CallbackTag::New([self, message](bool ok) {
        if (ok) {
          self->pending_ = message;
          self->status_ = grpc::Status::OK;
        } else {
          self->done_ = true;
          self->status_ = grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "end of stream");
        }
        self->callback_(self);
      }));
    }

    void _Pump() {
      // Drives the single-flight write queue, then Finish once drained.
      if (writing_ || finished_) {
        return;
      }
      if (!queue_.empty()) {
        writing_ = true;
        buffered_ = queue_.front();
        queue_.pop_front();
        auto self = this->shared_from_this();
        stream_->Write(buffered_, CallbackTag::New([self](bool ok) {
          self->writing_ = false;
          if (ok) {
            self->_Pump();
          } else {
            self->_Finish(grpc::Status(grpc::StatusCode::CANCELLED, "write failed"));
          }
        }));
      } else if (finishing_) {
        _Finish(finish_status_);
      }
    }

    void _Finish(const grpc::Status& status) {
      // Issues the terminal Finish exactly once. Updates status_ so Status().ok() == active.
      if (finished_) {
        return;
      }
      finished_ = true;
      status_ =
       status.ok() ? grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "stream finished") : status;
      auto self = this->shared_from_this();
      stream_->Finish(status, CallbackTag::New([self](bool) {
        // Do Nothing
      }));
    }

    AsyncService* service_ = nullptr;
    grpc::ServerCompletionQueue* cq_ = nullptr;
    Registrar registrar_;
    Callback callback_;
    grpc::ServerContext context_;
    std::unique_ptr<Stream> stream_;
    Ptr<Request> pending_;
    grpc::Status status_;
    bool done_ = false;
    std::deque<Response> queue_;
    Response buffered_;
    grpc::Status finish_status_;
    bool writing_ = false;
    bool finishing_ = false;
    bool finished_ = false;
    Ptr<Self> self_;
  };

  // ── Server ───────────────────────────────────────────────────────────────────
  //
  // Lightweight configuration holder: stores an address and the services to
  // host on it.  Builder::Start() collects all registered Server objects,
  // builds a single gRPC server with one shared completion queue, and calls
  // Register then Install on each one.
  class Server {
  public:
    explicit Server(std::string address) : address_(std::move(address)) {
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void AddHandler(Ptr<Service> service) {
      // Registers a service whose ownership is shared with the server.
      services_.push_back(std::move(service));
    }

    // Adds the listening port and registers all services with the builder.
    // Called once by Builder::Start() before BuildAndStart.
    void Register(grpc::ServerBuilder& builder) {
      builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
      for (auto& service : services_) {
        builder.RegisterService(service->Svc());
      }
    }

    // Arms the first receiver for every RPC on the shared completion queue.
    // Called once by Builder::Start() after BuildAndStart.
    void Install(grpc::ServerCompletionQueue* cq) {
      for (auto& service : services_) {
        service->Install(cq);
      }
    }
  private:
    std::string address_;
    std::vector<Ptr<Service>> services_;
  };

} // namespace framework

#endif // FRAMEWORK_SERVER_HPP_INCLUDED
