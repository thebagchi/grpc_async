#ifndef FRAMEWORK_EVENT_ENGINE_HPP_INCLUDED
#define FRAMEWORK_EVENT_ENGINE_HPP_INCLUDED

// Custom EventEngine skeleton for the framework.
//
// gRPC delegates all I/O, timers, and callback scheduling to an EventEngine
// instance.  By providing our own we can:
//   - Route all callbacks through our single completion queue / reactor.
//   - Replace gRPC's thread pools with our own scheduler.
//   - Implement I/O on a dedicated thread (or use io_uring, etc.).
//
// HOW TO PLUG IN
// --------------
// After building the gRPC ServerBuilder / ChannelArguments, pass an instance:
//
//   grpc::ServerBuilder builder;
//   builder.SetEventEngine(std::make_shared<framework::CustomEventEngine>());
//
//   grpc::ChannelArguments args;
//   args.SetEventEngine(std::make_shared<framework::CustomEventEngine>());
//
// The engine must outlive all channels and servers that use it.

#include <grpc/event_engine/event_engine.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace framework {

namespace ee = grpc_event_engine::experimental;

// ── StubEndpoint ─────────────────────────────────────────────────────────────
//
// Represents one end of a TCP (or other) connection.  The real implementation
// must wrap a non-blocking socket, drive epoll/kqueue/io_uring, and post
// completions back into gRPC.
class StubEndpoint : public ee::EventEngine::Endpoint {
public:
  // Read data from the underlying socket into `buffer`.
  //
  // TODO: Issue a non-blocking read on the socket.
  //       - If data is immediately available fill `buffer`, return true, and do
  //         NOT call `on_read`.
  //       - Otherwise arm the I/O poller, return false; when the socket
  //         becomes readable move data into `buffer` and invoke `on_read`.
  //       - On error (closed, cancelled) pass an appropriate absl::Status.
  //       - At most one outstanding Read per Endpoint at any time.
  bool Read(absl::AnyInvocable<void(absl::Status)> on_read,
            ee::SliceBuffer* /*buffer*/,
            ReadArgs /*args*/) override {
    return false;
  }

  // Write data from `data` to the underlying socket.
  //
  // TODO: Issue a non-blocking write from `data`.
  //       - If all bytes are written immediately, return true; do NOT call
  //         `on_writable`.
  //       - Otherwise arm the I/O poller, return false; call `on_writable`
  //         when the socket drains.
  //       - All bytes in `data` must be sent before signalling success.
  //       - At most one outstanding Write per Endpoint at any time.
  bool Write(absl::AnyInvocable<void(absl::Status)> on_writable,
             ee::SliceBuffer* /*data*/,
             WriteArgs /*args*/) override {
    return false;
  }

  // Return the remote address of the connection.
  //
  // TODO: Call getpeername(fd, ...) once during Connect/Accept and cache here.
  const ee::EventEngine::ResolvedAddress& GetPeerAddress() const override {
    return addr_;
  }

  // Return the local address of the connection.
  //
  // TODO: Call getsockname(fd, ...) once during Connect/Accept and cache here.
  const ee::EventEngine::ResolvedAddress& GetLocalAddress() const override {
    return addr_;
  }

  // Return optional telemetry (write metrics).
  // Return nullptr to opt out of metric collection entirely.
  std::shared_ptr<TelemetryInfo> GetTelemetryInfo() const override {
    return nullptr;
  }

private:
  ee::EventEngine::ResolvedAddress addr_;
};

// ── StubListener ─────────────────────────────────────────────────────────────
//
// Server-side: binds one or more ports and accepts incoming connections.
// For each accepted connection the engine calls `on_accept` with an Endpoint.
class StubListener : public ee::EventEngine::Listener {
public:
  // Bind to the given address.
  //
  // TODO: Create a non-blocking socket, setsockopt SO_REUSEADDR/SO_REUSEPORT,
  //       bind(fd, addr), listen(fd, backlog).
  //       Return the bound port number (useful when port 0 was requested).
  absl::StatusOr<int> Bind(const ee::EventEngine::ResolvedAddress& /*addr*/) override {
    // TODO: implement bind
    return absl::UnimplementedError("StubListener::Bind not implemented");
  }

  // Start accepting connections on all bound addresses.
  //
  // TODO: Register the listening socket(s) with the I/O poller.
  //       On each readable event call accept4/accept, wrap the new fd in a
  //       StubEndpoint, then call on_accept_(std::move(endpoint), allocator).
  //       When the listener is destroyed call on_shutdown_ with the reason.
  absl::Status Start() override {
    // TODO: implement accept loop
    return absl::UnimplementedError("StubListener::Start not implemented");
  }
};

// ── StubDNSResolver ──────────────────────────────────────────────────────────
//
// Asynchronous DNS lookup.  Destroying the resolver cancels pending lookups.
class StubDNSResolver : public ee::EventEngine::DNSResolver {
public:
  // Resolve `name` (e.g. "example.com:443") to a list of IP:port addresses.
  //
  // TODO: Call getaddrinfo (or c-ares / udns) asynchronously.
  //       On completion invoke `on_resolve` with the vector of ResolvedAddress.
  //       On error pass an appropriate status (NOT_FOUND, CANCELLED, etc.).
  void LookupHostname(LookupHostnameCallback on_resolve,
                      absl::string_view /*name*/,
                      absl::string_view /*default_port*/) override {
    // TODO: implement async hostname resolution
    on_resolve(absl::UnimplementedError("LookupHostname not implemented"));
  }

  // Look up DNS SRV records for `name`.
  //
  // TODO: Issue an async SRV query (c-ares: ares_query with T_SRV).
  void LookupSRV(LookupSRVCallback on_resolve,
                 absl::string_view /*name*/) override {
    // TODO: implement async SRV lookup
    on_resolve(absl::UnimplementedError("LookupSRV not implemented"));
  }

  // Look up DNS TXT records for `name`.
  //
  // TODO: Issue an async TXT query (c-ares: ares_query with T_TXT).
  void LookupTXT(LookupTXTCallback on_resolve,
                 absl::string_view /*name*/) override {
    // TODO: implement async TXT lookup
    on_resolve(absl::UnimplementedError("LookupTXT not implemented"));
  }
};

// ── CustomEventEngine ─────────────────────────────────────────────────────────
//
// Top-level EventEngine.  Owns the I/O poller and the task scheduler.
//
// SINGLE-THREAD DESIGN
// --------------------
// The simplest way to drive everything from one thread (the Builder reactor):
//
//   Run / RunAfter → push closures into the same completion queue as gRPC tags.
//   I/O polling    → epoll_wait / io_uring_enter on the same fd set.
//
// The completion-queue drain loop (Reactor::Run) then services both gRPC
// completions and EventEngine closures without any extra threads.
class CustomEventEngine : public ee::EventEngine {
public:
  // Returns true if the calling thread belongs to this engine's thread pool.
  //
  // TODO: Compare std::this_thread::get_id() against the set of worker
  //       thread IDs maintained by the engine.  Return false for now so that
  //       gRPC does not try to inline callbacks.
  bool IsWorkerThread() override {
    // TODO: track worker thread IDs and compare
    return false;
  }

  // Create a DNS resolver, optionally using a custom server from `options`.
  //
  // TODO: Construct a StubDNSResolver (or a c-ares-backed one).
  //       If options.dns_server is non-empty and the implementation does not
  //       support it, return absl::UnimplementedError.
  absl::StatusOr<std::unique_ptr<DNSResolver>> GetDNSResolver(
      const DNSResolver::ResolverOptions& /*options*/) override {
    // TODO: return a real DNS resolver
    return std::make_unique<StubDNSResolver>();
  }

  // Schedule `closure` to run as soon as possible (non-blocking from caller).
  //
  // TODO: Post `closure` to the I/O thread's run queue (e.g. a lock-free
  //       ring buffer or a pipe-wake + deque).  Do NOT run it inline here —
  //       calling thread may hold locks that the closure needs.
  //       Ownership of `closure` stays with the caller; do not delete it.
  void Run(Closure* closure) override {
    // TODO: enqueue closure->Run() on the worker thread
    (void)closure;
  }

  // Schedule a one-shot lambda to run as soon as possible.
  //
  // TODO: Same as Run(Closure*) but the AnyInvocable is owned by the engine;
  //       call and destroy it on the worker thread.
  void Run(absl::AnyInvocable<void()> closure) override {
    // TODO: move closure onto the run queue; call and destroy on worker thread
    (void)closure;
  }

  // Schedule `closure` to run after `when` has elapsed.
  //
  // TODO: Insert a timer entry (deadline = now + when) into a min-heap or
  //       timerfd / io_uring timeout.  On expiry call closure->Run().
  //       Return a TaskHandle that can be passed to Cancel().
  //       Return TaskHandle::kInvalid only when the closure is already runnable.
  //       Ownership stays with the caller.
  TaskHandle RunAfter(Duration when, Closure* closure) override {
    // TODO: register timer, return cancellable TaskHandle
    (void)when;
    (void)closure;
    return TaskHandle::kInvalid;
  }

  // Schedule a one-shot lambda to run after `when` has elapsed.
  //
  // TODO: Same as RunAfter(Duration, Closure*) but the AnyInvocable is owned
  //       by the engine; destroy it after calling or on cancellation.
  TaskHandle RunAfter(Duration when, absl::AnyInvocable<void()> closure) override {
    // TODO: register timer, return cancellable TaskHandle
    (void)when;
    (void)closure;
    return TaskHandle::kInvalid;
  }

  // Cancel a previously scheduled task.
  //
  // TODO: Look up `handle` in the timer heap / run queue.
  //       - If found and not yet run: remove it, destroy any AnyInvocable,
  //         return true.
  //       - If already run or not found: return false.
  bool Cancel(TaskHandle handle) override {
    // TODO: look up handle and cancel if pending
    (void)handle;
    return false;
  }

  // Create a server-side Listener that accepts incoming connections.
  //
  // TODO: Construct a StubListener, store on_accept and on_shutdown callbacks
  //       so they can be called when connections arrive and when the listener
  //       is destroyed.  The MemoryAllocatorFactory must be used to create
  //       allocators for each accepted Endpoint.
  absl::StatusOr<std::unique_ptr<Listener>> CreateListener(
      Listener::AcceptCallback on_accept,
      absl::AnyInvocable<void(absl::Status)> on_shutdown,
      const ee::EndpointConfig& /*config*/,
      std::unique_ptr<ee::MemoryAllocatorFactory> /*memory_allocator_factory*/) override {
    // TODO: create a real StubListener that calls on_accept / on_shutdown
    (void)on_accept;
    (void)on_shutdown;
    return absl::UnimplementedError("CreateListener not implemented");
  }

  // Initiate an outbound connection to `addr`.
  //
  // TODO: Create a non-blocking socket, call connect(fd, addr).
  //       - Poll for POLLOUT; on success wrap the fd in a StubEndpoint and
  //         call on_connect(std::move(endpoint)).
  //       - On failure call on_connect(error_status).
  //       - `on_connect` MUST be called exactly once, even on cancellation.
  //       - Use `memory_allocator` for all read/write buffer allocations.
  //       - Honour `timeout` by scheduling a cancellation timer.
  //       Return a ConnectionHandle so the caller can cancel via CancelConnect.
  ConnectionHandle Connect(OnConnectCallback on_connect,
                           const ResolvedAddress& /*addr*/,
                           const ee::EndpointConfig& /*args*/,
                           ee::MemoryAllocator /*memory_allocator*/,
                           Duration /*timeout*/) override {
    // TODO: start async connect; call on_connect exactly once
    (void)on_connect;
    return ConnectionHandle::kInvalid;
  }

  // Cancel an in-progress connection attempt.
  //
  // TODO: Look up the connection state for `handle`.
  //       - If still connecting: close the socket, do NOT call on_connect
  //         (per the contract for the Closure* overloads), return true.
  //       - If already connected or not found: return false.
  bool CancelConnect(ConnectionHandle handle) override {
    // TODO: cancel the in-progress connect and clean up resources
    (void)handle;
    return false;
  }
};

} // namespace framework

#endif // FRAMEWORK_EVENT_ENGINE_HPP_INCLUDED
