/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include <cinttypes>

using namespace std::literals::chrono_literals;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property AbstractAutoPersistingKeyValueStoreService::AlwaysPersist(
    core::PropertyBuilder::createProperty("Always Persist")->withDescription("Persist every change instead of persisting it periodically.")
        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property AbstractAutoPersistingKeyValueStoreService::AutoPersistenceInterval(
    core::PropertyBuilder::createProperty("Auto Persistence Interval")->withDescription("The interval of the periodic task persisting all values. "
                                                                                        "Only used if Always Persist is false. "
                                                                                        "If set to 0 seconds, auto persistence will be disabled.")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("1 min")->build());

AbstractAutoPersistingKeyValueStoreService::AbstractAutoPersistingKeyValueStoreService(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : PersistableKeyValueStoreService(name, uuid)
    , always_persist_(false)
    , auto_persistence_interval_(0U)
    , running_(false) {
}

AbstractAutoPersistingKeyValueStoreService::~AbstractAutoPersistingKeyValueStoreService() {
  stopPersistingThread();
}

void AbstractAutoPersistingKeyValueStoreService::stopPersistingThread() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);
  if (persisting_thread_.joinable()) {
    running_ = false;
    persisting_cv_.notify_one();
    lock.unlock();
    persisting_thread_.join();
  }
}

void AbstractAutoPersistingKeyValueStoreService::initialize() {
  ControllerService::initialize();
  updateSupportedProperties({AlwaysPersist, AutoPersistenceInterval});
}

void AbstractAutoPersistingKeyValueStoreService::onEnable() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable AbstractAutoPersistingKeyValueStoreService");
    return;
  }

  std::string value;
  if (!getProperty(AlwaysPersist.getName(), value)) {
    logger_->log_error("Always Persist attribute is missing or invalid");
  } else {
    always_persist_ = utils::StringUtils::toBool(value).value_or(false);
  }
  core::TimePeriodValue auto_persistence_interval;
  if (!getProperty(AutoPersistenceInterval.getName(), auto_persistence_interval)) {
    logger_->log_error("Auto Persistence Interval attribute is missing or invalid");
  } else {
    auto_persistence_interval_ = auto_persistence_interval.getMilliseconds();
  }

  if (!always_persist_ && auto_persistence_interval_ != 0s) {
    if (!persisting_thread_.joinable()) {
      logger_->log_trace("Starting auto persistence thread");
      running_ = true;
      persisting_thread_ = std::thread(&AbstractAutoPersistingKeyValueStoreService::persistingThreadFunc, this);
    }
  }

  logger_->log_trace("Enabled AbstractAutoPersistingKeyValueStoreService");
}

void AbstractAutoPersistingKeyValueStoreService::notifyStop() {
  stopPersistingThread();
}

void AbstractAutoPersistingKeyValueStoreService::persistingThreadFunc() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  while (true) {
    logger_->log_trace("Persisting thread is going to sleep for %" PRId64 " ms", int64_t{auto_persistence_interval_.count()});
    persisting_cv_.wait_for(lock, auto_persistence_interval_, [this] {
      return !running_;
    });

    if (!running_) {
      logger_->log_trace("Stopping persistence thread");
      return;
    }

    persist();
  }
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
