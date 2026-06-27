#ifndef FRAMEWORK_CLIENT_HPP_INCLUDED
#define FRAMEWORK_CLIENT_HPP_INCLUDED

#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include "framework/common.hpp"

namespace framework {

  // ── UnaryStream ──────────────────────────────────────────────────────────────
  //
  // Fire-and-forget unary caller. No FSM: the response and status are delivered
  // to the callback exactly once.
  template <typename Stub, typename Request, typename Response>
  class UnaryStream final
      : public Handler,
        public std::enable_shared_from_this<UnaryStream<Stub, Request, Response>> {
  public:
    using Self = UnaryStream<Stub, Request, Response>;
    using Reader = grpc::ClientAsyncResponseReaderInterface<Response>;
    using Callback = std::function<void(const Ptr<Response>&, const grpc::Status&)>;

    template <typename Factory>
    static void New(Stub* stub,
     grpc::CompletionQueue* cq,
     Factory factory,
     const Request& request,
     Callback callback,
     Ptr<grpc::ClientContext> context = nullptr) {
      // Starts the unary call and arranges delivery of the response.
      auto instance = std::make_shared<Self>();
      instance->context_ = context ? context : MakeClientContext();
      instance->callback_ = std::move(callback);
      instance->response_ = MakeMessage<Response>();
      instance->self_ = instance;
      instance->reader_.reset(
       std::invoke(factory, stub, instance->context_.get(), request, cq).release());
      instance->reader_->Finish(instance->response_.get(), &instance->status_, instance.get());
    }

    void Proceed() override {
      // The call completed: deliver response and status, then release self.
      auto self = std::move(self_);
      callback_(response_, status_);
    }

    void Errored() override {
      // The call failed before completion: deliver a cancelled status.
      auto self = std::move(self_);
      callback_(nullptr, grpc::Status(grpc::StatusCode::CANCELLED, "unary call failed"));
    }
  private:
    Ptr<grpc::ClientContext> context_;
    Callback callback_;
    Ptr<Response> response_;
    grpc::Status status_;
    std::unique_ptr<Reader> reader_;
    Ptr<Self> self_;
  };

  // ── InboundStream ────────────────────────────────────────────────────────────
  //
  // Server-streaming caller. Reads begin automatically once the call starts and
  // chain until the server closes the stream, then Finish runs automatically.
  template <typename Stub, typename Request, typename Response>
  class InboundStream final
      : public Handler,
        public std::enable_shared_from_this<InboundStream<Stub, Request, Response>> {
  public:
    using Self = InboundStream<Stub, Request, Response>;
    using Reader = grpc::ClientAsyncReaderInterface<Response>;
    using Callback = std::function<void(const Ptr<Self>&, StreamEvent)>;

    template <typename Factory>
    static void New(Stub* stub,
     grpc::CompletionQueue* cq,
     Factory factory,
     const Request& request,
     Callback callback,
     Ptr<grpc::ClientContext> context = nullptr) {
      // Starts the server-streaming call. The creation tag fires on StartCall.
      auto instance = std::make_shared<Self>();
      instance->context_ = context ? context : MakeClientContext();
      instance->callback_ = std::move(callback);
      instance->self_ = instance;
      instance->reader_.reset(
       std::invoke(factory, stub, instance->context_.get(), request, cq, instance.get()).release());
    }

    Ptr<Response> Message() const {
      // Returns the most recently received message.
      return response_;
    }

    StreamState State() const {
      // Returns the current stream state.
      return state_;
    }

    grpc::Status Status() const {
      // Returns the final status (valid once E_FINISHED has fired).
      return status_;
    }

    std::tuple<Ptr<Response>, grpc::Status> Read() const {
      // Returns the latest received message paired with the current status.
      return std::make_tuple(response_, status_);
    }

    void Proceed() override {
      // StartCall completed: go active and begin the read chain.
      state_ = StreamState::E_ACTIVE;
      _BeginRead();
    }

    void Errored() override {
      // StartCall failed: synthesize a cancelled finish.
      status_ = grpc::Status(grpc::StatusCode::CANCELLED, "stream start failed");
      state_ = StreamState::E_CLOSED;
      auto self = std::move(self_);
      callback_(self, StreamEvent::E_FINISHED);
    }
  private:
    void _BeginRead() {
      // Reads one message, delivers E_DATA, then chains the next read.
      auto self = this->shared_from_this();
      auto message = MakeMessage<Response>();
      reader_->Read(message.get(), CallbackTag::New([self, message](bool ok) {
        if (ok) {
          self->response_ = message;
          self->callback_(self, StreamEvent::E_DATA);
          self->_BeginRead();
        } else {
          self->state_ = StreamState::E_DRAINING;
          self->_Finish();
        }
      }));
    }

    void _Finish() {
      // Retrieves the final status exactly once, then delivers E_FINISHED.
      if (finishing_.exchange(true)) {
        return;
      }
      auto self = this->shared_from_this();
      reader_->Finish(&status_, CallbackTag::New([self](bool) {
        self->state_ = StreamState::E_CLOSED;
        self->callback_(self, StreamEvent::E_FINISHED);
        self->self_.reset();
      }));
    }

    Ptr<grpc::ClientContext> context_;
    Callback callback_;
    Ptr<Response> response_;
    grpc::Status status_;
    std::unique_ptr<Reader> reader_;
    StreamState state_ = StreamState::E_INITIALIZING;
    std::atomic_bool finishing_{ false };
    Ptr<Self> self_;
  };

  // ── OutboundStream ───────────────────────────────────────────────────────────
  //
  // Client-streaming caller. The user writes messages then calls Close, which
  // sends WritesDone and Finish; the single response arrives with E_FINISHED.
  template <typename Stub, typename Request, typename Response>
  class OutboundStream final
      : public Handler,
        public std::enable_shared_from_this<OutboundStream<Stub, Request, Response>> {
  public:
    using Self = OutboundStream<Stub, Request, Response>;
    using Writer = grpc::ClientAsyncWriterInterface<Request>;
    using Callback = std::function<void(const Ptr<Self>&, StreamEvent)>;

    template <typename Factory>
    static void New(Stub* stub,
     grpc::CompletionQueue* cq,
     Factory factory,
     Callback callback,
     Ptr<grpc::ClientContext> context = nullptr) {
      // Starts the client-streaming call. The creation tag fires on StartCall.
      auto instance = std::make_shared<Self>();
      instance->context_ = context ? context : MakeClientContext();
      instance->callback_ = std::move(callback);
      instance->response_ = MakeMessage<Response>();
      instance->self_ = instance;
      instance->writer_.reset(std::invoke(
       factory, stub, instance->context_.get(), instance->response_.get(), cq, instance.get())
                               .release());
    }

    Ptr<Response> Message() const {
      // Returns the response (valid once E_FINISHED has fired).
      return response_;
    }

    StreamState State() const {
      // Returns the current stream state.
      return state_;
    }

    grpc::Status Status() const {
      // Returns the final status (valid once E_FINISHED has fired).
      return status_;
    }

    std::tuple<Ptr<Response>, grpc::Status> Read() const {
      // Returns the single response paired with the current status.
      return std::make_tuple(response_, status_);
    }

    void Write(const Request& request) {
      // Queues a request message for writing.
      queue_.push_back(request);
      _Pump();
    }

    void Close() {
      // Signals end of writes; WritesDone and Finish run after the queue drains.
      closing_ = true;
      _Pump();
    }

    void Proceed() override {
      // StartCall completed: go active and notify the user it may write.
      state_ = StreamState::E_ACTIVE;
      callback_(this->shared_from_this(), StreamEvent::E_READY);
    }

    void Errored() override {
      // StartCall failed: synthesize a cancelled finish.
      status_ = grpc::Status(grpc::StatusCode::CANCELLED, "stream start failed");
      state_ = StreamState::E_CLOSED;
      auto self = std::move(self_);
      callback_(self, StreamEvent::E_FINISHED);
    }
  private:
    void _Pump() {
      // Drives the single-flight write queue, then WritesDone once drained.
      if (writing_) {
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
            self->_Finish();
          }
        }));
      } else if (closing_ && !halfclosed_) {
        halfclosed_ = true;
        auto self = this->shared_from_this();
        writer_->WritesDone(CallbackTag::New([self](bool) {
          self->_Finish();
        }));
      }
    }

    void _Finish() {
      // Retrieves the response and final status exactly once, delivers E_FINISHED.
      if (finishing_.exchange(true)) {
        return;
      }
      state_ = StreamState::E_DRAINING;
      auto self = this->shared_from_this();
      writer_->Finish(&status_, CallbackTag::New([self](bool) {
        self->state_ = StreamState::E_CLOSED;
        self->callback_(self, StreamEvent::E_FINISHED);
        self->self_.reset();
      }));
    }

    Ptr<grpc::ClientContext> context_;
    Callback callback_;
    Ptr<Response> response_;
    grpc::Status status_;
    std::unique_ptr<Writer> writer_;
    std::deque<Request> queue_;
    Request pending_;
    StreamState state_ = StreamState::E_INITIALIZING;
    bool writing_ = false;
    bool closing_ = false;
    bool halfclosed_ = false;
    std::atomic_bool finishing_{ false };
    Ptr<Self> self_;
  };

  // ── DuplexStream ─────────────────────────────────────────────────────────────
  //
  // Bidirectional caller. Auto-reads like InboundStream and queues writes like
  // OutboundStream. Close half-closes the write side; Finish runs automatically
  // when the server closes its write side.
  template <typename Stub, typename Request, typename Response>
  class DuplexStream final
      : public Handler,
        public std::enable_shared_from_this<DuplexStream<Stub, Request, Response>> {
  public:
    using Self = DuplexStream<Stub, Request, Response>;
    using Stream = grpc::ClientAsyncReaderWriterInterface<Request, Response>;
    using Callback = std::function<void(const Ptr<Self>&, StreamEvent)>;

    template <typename Factory>
    static void New(Stub* stub,
     grpc::CompletionQueue* cq,
     Factory factory,
     Callback callback,
     Ptr<grpc::ClientContext> context = nullptr) {
      // Starts the bidi call. The creation tag fires on StartCall.
      auto instance = std::make_shared<Self>();
      instance->context_ = context ? context : MakeClientContext();
      instance->callback_ = std::move(callback);
      instance->self_ = instance;
      instance->stream_.reset(
       std::invoke(factory, stub, instance->context_.get(), cq, instance.get()).release());
    }

    Ptr<Response> Message() const {
      // Returns the most recently received message.
      return response_;
    }

    StreamState State() const {
      // Returns the current stream state.
      return state_;
    }

    grpc::Status Status() const {
      // Returns the final status (valid once E_FINISHED has fired).
      return status_;
    }

    std::tuple<Ptr<Response>, grpc::Status> Read() const {
      // Returns the latest received message paired with the current status.
      return std::make_tuple(response_, status_);
    }

    void Write(const Request& request) {
      // Queues a request message for writing.
      queue_.push_back(request);
      _Pump();
    }

    void Close() {
      // Half-closes the write side once queued writes have drained.
      closing_ = true;
      _Pump();
    }

    void Proceed() override {
      // StartCall completed: go active, begin reading, and notify the user.
      state_ = StreamState::E_ACTIVE;
      _BeginRead();
      callback_(this->shared_from_this(), StreamEvent::E_READY);
    }

    void Errored() override {
      // StartCall failed: synthesize a cancelled finish.
      status_ = grpc::Status(grpc::StatusCode::CANCELLED, "stream start failed");
      state_ = StreamState::E_CLOSED;
      auto self = std::move(self_);
      callback_(self, StreamEvent::E_FINISHED);
    }
  private:
    void _BeginRead() {
      // Reads one message, delivers E_DATA, then chains the next read.
      auto self = this->shared_from_this();
      auto message = MakeMessage<Response>();
      stream_->Read(message.get(), CallbackTag::New([self, message](bool ok) {
        if (ok) {
          self->response_ = message;
          self->callback_(self, StreamEvent::E_DATA);
          self->_BeginRead();
        } else {
          self->state_ = StreamState::E_DRAINING;
          self->_Finish();
        }
      }));
    }

    void _Pump() {
      // Drives the single-flight write queue, then WritesDone once drained.
      if (writing_) {
        return;
      }
      if (!queue_.empty()) {
        writing_ = true;
        pending_ = queue_.front();
        queue_.pop_front();
        auto self = this->shared_from_this();
        stream_->Write(pending_, CallbackTag::New([self](bool ok) {
          self->writing_ = false;
          if (ok) {
            self->_Pump();
          } else {
            self->_Finish();
          }
        }));
      } else if (closing_ && !halfclosed_) {
        halfclosed_ = true;
        auto self = this->shared_from_this();
        stream_->WritesDone(CallbackTag::New([self](bool) {
          // Do Nothing
        }));
      }
    }

    void _Finish() {
      // Retrieves the final status exactly once, then delivers E_FINISHED.
      if (finishing_.exchange(true)) {
        return;
      }
      auto self = this->shared_from_this();
      stream_->Finish(&status_, CallbackTag::New([self](bool) {
        self->state_ = StreamState::E_CLOSED;
        self->callback_(self, StreamEvent::E_FINISHED);
        self->self_.reset();
      }));
    }

    Ptr<grpc::ClientContext> context_;
    Callback callback_;
    Ptr<Response> response_;
    grpc::Status status_;
    std::unique_ptr<Stream> stream_;
    std::deque<Request> queue_;
    Request pending_;
    StreamState state_ = StreamState::E_INITIALIZING;
    bool writing_ = false;
    bool closing_ = false;
    bool halfclosed_ = false;
    std::atomic_bool finishing_{ false };
    Ptr<Self> self_;
  };

  // ── Client ───────────────────────────────────────────────────────────────────
  //
  // Owns a client completion queue and a reactor that drains it. The reactor may
  // instead be borrowed from a Server (or another Client) so a single thread
  // services both — see the constructor taking a Ptr<Reactor>.
  class Client {
  public:
    Client()
        : cq_(std::make_shared<grpc::CompletionQueue>()),
          reactor_(std::make_shared<Reactor>(cq_.get())) {
      // Do Nothing
    }

    explicit Client(Ptr<Reactor> reactor) : reactor_(std::move(reactor)) {
      // Do Nothing
    }

    grpc::CompletionQueue* CQ() {
      // Returns the raw queue for passing to stream constructors.
      return reactor_->Queue();
    }

    Ptr<Reactor> GetReactor() const {
      // Returns the reactor so other components can share this polling loop.
      return reactor_;
    }

    void Run() {
      // Drains the queue until Shutdown, unless another caller is already draining
      // the shared reactor, in which case it returns immediately.
      reactor_->Run();
    }

    void Shutdown() {
      // Shuts down the owned queue so Run returns; a no-op when the queue is
      // shared with (and owned by) another component such as a Server.
      if (cq_) {
        cq_->Shutdown();
      }
    }
  private:
    Ptr<grpc::CompletionQueue> cq_;
    Ptr<Reactor> reactor_;
  };

} // namespace framework

#endif // FRAMEWORK_CLIENT_HPP_INCLUDED
