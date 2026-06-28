#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "framework/builder.hpp"
#include "sample.framework.pb.h"

using framework::StreamEvent;
using sample::Chunk;
using sample::SampleServiceClient;
using sample::Summary;

// Exercises all four call shapes against the demo server using one polling loop.
int main() {
  auto& builder = framework::Builder::Instance();
  auto client = builder.AddClient();

  SampleServiceClient svc("localhost:50051", client);

  // Shuts the client down once all four logical calls have finished.
  auto remaining = std::make_shared<int>(4);
  std::function<void()> done_one = [remaining]() {
    if (--(*remaining) == 0) {
      framework::Builder::Instance().Stop();
    }
  };

  // Unary.
  {
    Chunk request;
    request.set_payload("hello");
    svc.Unary(request,
     [done_one](const framework::Ptr<Summary>& response, const grpc::Status& status) {
      if (status.ok() && response) {
        std::cout << "[unary] " << response->note() << " count=" << response->count() << "\n";
      } else {
        std::cout << "[unary] failed: " << status.error_message() << "\n";
      }
      done_one();
    });
  }

  // Server-streaming.
  {
    Chunk request;
    request.set_payload("stream");
    svc.ServerStream(request,
     [done_one](
      const framework::Ptr<SampleServiceClient::ServerStreamCaller>& stream, StreamEvent event) {
      if (event == StreamEvent::E_DATA) {
        std::cout << "[server-stream] " << stream->Message()->payload() << "\n";
      } else if (event == StreamEvent::E_FINISHED) {
        std::cout << "[server-stream] finished ok=" << stream->Status().ok() << "\n";
        done_one();
      }
    });
  }

  // Client-streaming.
  {
    svc.ClientStream(
     [done_one](
      const framework::Ptr<SampleServiceClient::ClientStreamCaller>& stream, StreamEvent event) {
      if (event == StreamEvent::E_READY) {
        for (std::uint64_t i = 0; i < 3; ++i) {
          Chunk chunk;
          chunk.set_payload("c" + std::to_string(i));
          chunk.set_seq(i);
          stream->Write(chunk);
        }
        stream->Close();
      } else if (event == StreamEvent::E_FINISHED) {
        std::cout << "[client-stream] finished count=" << stream->Message()->count()
                  << " ok=" << stream->Status().ok() << "\n";
        done_one();
      }
    });
  }

  // Bidi.
  {
    svc.BidiStream([done_one](const framework::Ptr<SampleServiceClient::BidiStreamCaller>& stream,
                    StreamEvent event) {
      if (event == StreamEvent::E_READY) {
        for (std::uint64_t i = 0; i < 3; ++i) {
          Chunk chunk;
          chunk.set_payload("b" + std::to_string(i));
          chunk.set_seq(i);
          stream->Write(chunk);
        }
        stream->Close();
      } else if (event == StreamEvent::E_DATA) {
        std::cout << "[bidi] " << stream->Message()->payload() << "\n";
      } else if (event == StreamEvent::E_FINISHED) {
        std::cout << "[bidi] finished ok=" << stream->Status().ok() << "\n";
        done_one();
      }
    });
  }

  builder.Start();
  return 0;
}
