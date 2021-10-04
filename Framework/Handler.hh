#ifndef FRAMEWORK_HANDLER__INCLUDED
#define FRAMEWORK_HANDLER__INCLUDED

#include <grpcpp/grpcpp.h>

class Handler {
 public:
  virtual ~Handler() = default;

  virtual void Install(grpc::ServerCompletionQueue* cq) = 0;
  virtual grpc::Service* Service() = 0;
};

#endif //FRAMEWORK_HANDLER__INCLUDED