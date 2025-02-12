/**
 *
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

#pragma once

#include <memory>
#include <string>

#include "MockConnectors.h"
#include "services/DatabaseService.h"
#include "core/Resource.h"
#include "data/DatabaseConnectors.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {
namespace controllers {

class MockODBCService : public DatabaseService {
 public:
  explicit MockODBCService(const std::string &name, utils::Identifier uuid = utils::Identifier())
    : DatabaseService(name, uuid),
      logger_(logging::LoggerFactory<MockODBCService>::getLogger()) {
    initialize();
  }

  explicit MockODBCService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : DatabaseService(name),
        logger_(logging::LoggerFactory<MockODBCService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  std::unique_ptr<sql::Connection> getConnection() const {
    return std::make_unique<sql::MockODBCConnection>(connection_string_);
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(MockODBCService, "Controller service that provides Mock ODBC database connection");

} /* namespace controllers */
} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
