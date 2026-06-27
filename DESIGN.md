# Framework Design

A thin C++17 wrapper over the gRPC asynchronous completion-queue API. Application code
expresses RPC logic as plain callbacks; the framework hides tag casting, completion-queue
polling, streamer lifetime, and read-loop management. A protoc plugin (`protoc-gen-framework`)
generates per-service adapter headers that bind the framework primitives to each `.proto`
service, eliminating all per-RPC hand-wiring.

---

## File map

### Framework

| File | Role |
|---|---|
| `framework/common.hpp` | `Handler` base, `CallbackTag`, `Ptr<T>`, message/context factories, `Drain` loop |
| `framework/server.hpp` | `Service` base, four server call objects, `Server` |
| `framework/client.hpp` | `StreamState`/`StreamEvent`, four client stream objects, `Client` |

### Proto & generated

| File | Role |
|---|---|
| `proto/sample.proto` | One service exercising all four call shapes |
| `proto/CMakeLists.txt` | Runs `protoc` (cpp + grpc + framework), builds `proto_lib` |
| `proto/generated/` | `protoc --cpp_out` / `--grpc_out` output (not checked in) |
| `gen/` | `protoc --framework_out` output — one `.framework.pb.h` per proto (not checked in) |

### Example

| File | Role |
|---|---|
| `example/server.cc` | Demo service implementing all four shapes via generated base class |
| `example/client.cc` | Demo client driving all four shapes via generated client facade |

### Plugin (`tools/protoc-gen-framework/`)

| File | Role |
|---|---|
| `main.go` | `protogen` entry point; builds the `File` data model from proto descriptors |
| `template.go` | Data model types, template helpers, FuncMap, and template execution |
| `constants.go` | Named constants for all framework class names, file extensions, and shape comments |
| `header.tmpl` | `text/template` source that renders the C++ adapter header |
| `CMakeLists.txt` | `go build` custom command; outputs binary to `_build/protoc-gen-framework` |

---

## Core patterns

### 1. Handler / tag dispatch

gRPC's async API registers a raw `void*` tag with a completion queue and blocks on
`Next(&tag, &ok)`. The framework treats every tag as a pointer to a `Handler` — a
non-template base with two pure virtuals: `Proceed()` (ok == true) and `Errored()`
(ok == false). The single polling loop (`Drain`) casts the raw pointer back to `Handler*`
and dispatches, without knowing the concrete type.

### 2. One-shot `CallbackTag`

Most async operations (each `Read`, `Write`, `Finish`, `WritesDone`) need their own tag.
`CallbackTag` wraps a `std::function<void(bool)>`. `CallbackTag::New(cb)` allocates the tag
as a `shared_ptr`, seeds a self-reference so it stays alive while outstanding, returns the
raw `Handler*`, and releases the self-reference when the queue delivers it.

### 3. Shared-ownership lifetime via `self_`

Every call/stream object that is itself submitted as a CQ tag inherits
`std::enable_shared_from_this<Self>` and holds a `Ptr<Self> self_`. The static `New()`
factory builds the object via `std::make_shared`, sets `self_ = instance`, and hands the raw
pointer to gRPC. `self_` is released at the terminal transition. No `delete this` anywhere.

### 4. Single polling thread (guarded by `Reactor`)

`Drain(cq)` is the one polling loop; all callbacks run on the thread that calls it, so
call/stream state needs no locking. A `Reactor` wraps a completion queue and guards that loop
with an atomic flag: the first caller of `Reactor::Run` drains the queue, and any concurrent
or later caller returns immediately. `Server` and `Client` each hold a `Ptr<Reactor>`; a
process hosting both can share one reactor via `Server::GetReactor()`.

---

## Server side (`server.hpp`)

`Service` is the application base class: `Install(cq)` registers the first receiver for each
method; `Svc()` returns the underlying `grpc::*::AsyncService`. Four call objects, each with
a static `New()` that registers a fresh receiver and, on arrival, spawns its own successor:

| Class | Shape | Key methods |
|---|---|---|
| `UnaryCall` | 1 → 1 | `Finish(resp)`, `FinishWithError(status)` |
| `ServerStreamCall` | 1 → N | `Write(resp)` (queued single-flight), `Finish(status)` |
| `ClientStreamCall` | N → 1 | `Read(consumer, completer)`, `Finish(resp, status)` |
| `BidiCall` | N → N | `Read(consumer, completer)`, `Write(resp)`, `Finish(status)` |

`Server` owns the `grpc::Server`, the `ServerCompletionQueue`, and the registered services.
`Start()` builds and arms every receiver, then returns without blocking; `Wait()` runs the
polling loop. `Queue()` / `GetReactor()` expose the queue/reactor for sharing with a `Client`.

---

## Client side (`client.hpp`)

Four stream objects, each templated on `<Stub, Request, Response>`. `New()` takes the stub
member-function pointer as a deduced `Factory` and invokes it via `std::invoke`.

| Class | Shape | Events |
|---|---|---|
| `UnaryStream` | 1 → 1 | none — callback delivers `(response, status)` once |
| `InboundStream` | 1 → N | `E_DATA` per message, `E_FINISHED` |
| `OutboundStream` | N → 1 | `E_READY`, `E_FINISHED` (`Write`, then `Close`) |
| `DuplexStream` | N → N | `E_READY`, `E_DATA`, `E_FINISHED` |

**Uniform terminal model:** every terminal outcome — clean close, transport failure, or a
failed `StartCall` — surfaces as a single `E_FINISHED` event carrying a `grpc::Status`.

`Client` owns a `CompletionQueue` and a `Reactor` over it; `Run()` drives the reactor,
`Shutdown()` ends it. To co-host a `Server` and `Client` on one thread, construct
`Client(server.GetReactor())`.

---

## `protoc-gen-framework` plugin

The plugin is a `protoc` plugin written in Go. It reads the `FileDescriptorProto` set
`protoc` already parsed and emits one `<basename>.framework.pb.h` per `.proto` file.

### Shape selection

The two streaming flags map deterministically to one framework class pair:

| `client_streaming` | `server_streaming` | Server class | Client class |
|:---:|:---:|---|---|
| false | false | `framework::UnaryCall` | `framework::UnaryStream` |
| false | true | `framework::ServerStreamCall` | `framework::InboundStream` |
| true | false | `framework::ClientStreamCall` | `framework::OutboundStream` |
| true | true | `framework::BidiCall` | `framework::DuplexStream` |

### Generated output

Per service `S` in proto package `pkg` (→ C++ namespace `pkg`), the plugin emits:

**`<pkg>SBase`** — extends `framework::Service`:
- One `using <M>Call = framework::<ServerClass><AsyncService, Req, Resp>` alias per RPC
- One `virtual void On<M>(grpc::ServerContext*, [const Req& request,] const Ptr<<M>Call>&) = 0` per RPC
- Generated `Install(cq)` that arms one receiver per RPC with the correct shape, method pointer, and forwarding lambda
- Generated `Svc()` returning `&service_`

**`<pkg>SClient`** — standalone facade:
- One `using <M>Caller = framework::<ClientClass><Stub, Req, Resp>` alias per RPC
- Constructor taking `shared_ptr<grpc::Channel>` and `CompletionQueue*`
- One `void <M>([const Req& request,] <M>Caller::Callback, Ptr<ClientContext>)` per RPC

The generated header is piped through `clang-format --style=file` immediately after `protoc`
writes it, so the output always matches the project's `.clang-format` rules.

### Plugin internals

```
main.go         protogen entry point; builds File/Service/Method model from descriptors
template.go     data model types (*Method, *Service, File), helpers, FuncMap, template execution
constants.go    CAPITAL_SNAKE_CASE constants for class names, extensions, shape comments
header.tmpl     text/template source; logic-free name substitution only
```

`main.go` walks the proto descriptor to populate the model; all C++ string decisions
(`framework::UnaryCall`, `"request, "`, etc.) are named constants in `constants.go` and
selected by helper functions in `template.go`. The template (`header.tmpl`) only substitutes
names — no Go logic lives inside it.

---

## Build

```
_build/
  protoc-gen-framework   ← go build output (tools/protoc-gen-framework/)
  proto_lib              ← static library from proto/generated/ sources
  example_server
  example_client

gen/
  *.framework.pb.h       ← protoc --framework_out output, clang-formatted in-place

proto/generated/
  *.pb.{h,cc}            ← protoc --cpp_out output
  *.grpc.pb.{h,cc}       ← protoc --grpc_out output
```

Build sequence (handled by CMake):

1. `tools/protoc-gen-framework/` → `go build` → `_build/protoc-gen-framework`
2. `proto/` → `protoc --cpp_out --grpc_out` → `proto/generated/`
3. `proto/` → `protoc --framework_out` (using plugin) → `gen/`, then `clang-format -i` on each header
4. Root → `clang-format -i` over `framework/*.hpp` and `example/*.cc`
5. Compile `proto_lib`, `example_server`, `example_client`

```sh
cmake -S . -B _build
cmake --build _build
./_build/example_server &   # listens on 0.0.0.0:50051
./_build/example_client     # drives all four call shapes, then exits
```
