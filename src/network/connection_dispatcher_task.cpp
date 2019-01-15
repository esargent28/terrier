#include "network/connection_dispatcher_task.h"

#include <common/dedicated_thread_registry.h>

#include <csignal>
#include <memory>

#define MASTER_THREAD_ID (-1)

namespace terrier::network {

ConnectionDispatcherTask::ConnectionDispatcherTask(int num_handlers, int listen_fd,
                                                   DedicatedThreadOwner *dedicatedThreadOwner)
    : NotifiableTask(MASTER_THREAD_ID), next_handler_(0) {
  RegisterEvent(listen_fd, EV_READ | EV_PERSIST, METHOD_AS_CALLBACK(ConnectionDispatcherTask, DispatchConnection),
                this);
  RegisterSignalEvent(SIGHUP, METHOD_AS_CALLBACK(NotifiableTask, ExitLoop), this);

  // TODO(tianyu) Figure out what this initialization logic is doing and
  // potentially rewrite
  // register thread to epoch manager.
  /*if (concurrency::EpochManagerFactory::GetEpochType() ==
      EpochType::DECENTRALIZED_EPOCH) {
    for (size_t task_id = 0; task_id < (size_t)num_handlers; task_id++) {
      concurrency::EpochManagerFactory::GetInstance().RegisterThread(task_id);
    }
  }*/

  // create worker threads.
  for (int task_id = 0; task_id < num_handlers; task_id++) {
    auto handler = std::make_shared<ConnectionHandlerTask>(task_id);
    handlers_.push_back(handler);
    DedicatedThreadRegistry::GetInstance().RegisterDedicatedThread<ConnectionHandlerTask>(dedicatedThreadOwner,
                                                                                          handler);
  }
}

void ConnectionDispatcherTask::DispatchConnection(int fd, int16_t) {  // NOLINT
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);

  int new_conn_fd = accept(fd, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
  if (new_conn_fd == -1) {
    LOG_ERROR("Failed to accept");
  }

  // Dispatch by rand number
  uint64_t handler_id = next_handler_;

  // update next threadID
  next_handler_ = (next_handler_ + 1) % handlers_.size();

  std::shared_ptr<ConnectionHandlerTask> handler = handlers_[handler_id];
  LOG_DEBUG("Dispatching connection to worker %d", handler_id);

  handler->Notify(new_conn_fd);
}

void ConnectionDispatcherTask::ExitLoop() {
  NotifiableTask::ExitLoop();
  for (auto &handler : handlers_) handler->ExitLoop();
}

}  // namespace terrier::network
