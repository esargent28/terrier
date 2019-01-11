#pragma once
#include <functional>
#include <memory>
#include "network/network_io_utils.h"
#include "network/network_types.h"
//
namespace terrier {
namespace network {
//
class ProtocolInterpreter {
 public:
  virtual Transition Process(std::shared_ptr<ReadBuffer> in, std::shared_ptr<WriteQueue> out,
                             CallbackFunc callback) = 0;
  //
  // TODO(Tianyu): Do we really need this crap?
  virtual void GetResult(std::shared_ptr<WriteQueue> out) = 0;

  virtual ~ProtocolInterpreter() = default;
};
//
}  // namespace network
}  // namespace terrier