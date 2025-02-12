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

#include <memory>
#include <string>
#include <vector>

#include <catch.hpp>
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "../TestBase.h"
#include "StreamPipe.h"

#pragma once

namespace ContentRepositoryDependentTests {

struct WriteStringToFlowFile : public minifi::OutputStreamCallback {
  const std::vector<uint8_t> buffer_;

  explicit WriteStringToFlowFile(const std::string& buffer) : buffer_(buffer.begin(), buffer.end()) {}

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    size_t bytes_written = stream->write(buffer_, buffer_.size());
    return minifi::io::isError(bytes_written) ? -1 : gsl::narrow<int64_t>(bytes_written);
  }
};

struct ReadUntilItCan : public minifi::InputStreamCallback {
  std::string value_;

  int64_t process(const std::shared_ptr<minifi::io::BaseStream> &stream) override {
    value_.clear();
    std::vector<uint8_t> buffer;
    size_t bytes_read = 0;
    while (true) {
      size_t read_result = stream->read(buffer, 1024);
      if (minifi::io::isError(read_result))
        return -1;
      if (read_result == 0)
        return bytes_read;
      bytes_read += read_result;
      value_.append(buffer.begin(), buffer.end());
    }
  }
};

class DummyProcessor : public core::Processor {
  using core::Processor::Processor;
};

REGISTER_RESOURCE(DummyProcessor, "A processor that does nothing.");

class Fixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};
  const core::Relationship Failure{"failure", "something has gone awry"};

  explicit Fixture(std::shared_ptr<core::ContentRepository> content_repo) {
    test_plan_ = test_controller_.createPlan(nullptr, nullptr, content_repo);
    dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
    context_ = [this] {
      test_plan_->runNextProcessor();
      return test_plan_->getCurrentContext();
    }();
    process_session_ = std::make_unique<core::ProcessSession>(context_);
  }

  core::ProcessSession &processSession() { return *process_session_; }

  void transferAndCommit(const std::shared_ptr<core::FlowFile>& flow_file) {
    process_session_->transfer(flow_file, Success);
    process_session_->commit();
  }

  void writeToFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::string content) {
    WriteStringToFlowFile callback(content);
    process_session_->write(flow_file, &callback);
  }

  void appendToFlowFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::string content_to_append) {
    WriteStringToFlowFile callback(content_to_append);
    process_session_->add(flow_file);
    process_session_->append(flow_file, &callback);
  }

 private:
  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
};

void testReadOnSmallerClonedFlowFiles(std::shared_ptr<core::ContentRepository> content_repo) {
  Fixture fixture = Fixture(content_repo);
  core::ProcessSession& process_session = fixture.processSession();
  const auto original_ff = process_session.create();
  fixture.writeToFlowFile(original_ff, "foobar");
  fixture.transferAndCommit(original_ff);
  REQUIRE(original_ff);
  auto clone_first_half = process_session.clone(original_ff, 0, 3);
  auto clone_second_half = process_session.clone(original_ff, 3, 3);
  REQUIRE(clone_first_half != nullptr);
  REQUIRE(clone_second_half != nullptr);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result_original = process_session.readBuffer(original_ff);
  process_session.read(original_ff, &read_until_it_can_callback);
  CHECK(original_ff->getSize() == 6);
  CHECK(to_string(read_result_original) == "foobar");
  CHECK(read_until_it_can_callback.value_ == "foobar");
  const auto read_result_first_half = process_session.readBuffer(clone_first_half);
  process_session.read(clone_first_half, &read_until_it_can_callback);
  CHECK(clone_first_half->getSize() == 3);
  CHECK(to_string(read_result_first_half) == "foo");
  CHECK(read_until_it_can_callback.value_ == "foo");
  const auto read_result_second_half = process_session.readBuffer(clone_second_half);
  process_session.read(clone_second_half, &read_until_it_can_callback);
  CHECK(clone_second_half->getSize() == 3);
  CHECK(to_string(read_result_second_half) == "bar");
  CHECK(read_until_it_can_callback.value_ == "bar");
}

void testAppendToUnmanagedFlowFile(std::shared_ptr<core::ContentRepository> content_repo) {
  Fixture fixture = Fixture(content_repo);
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  REQUIRE(flow_file);

  fixture.writeToFlowFile(flow_file, "my");
  fixture.transferAndCommit(flow_file);
  fixture.appendToFlowFile(flow_file, "foobar");
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 8);
  ReadUntilItCan read_until_it_can_callback;
  const auto read_result = process_session.readBuffer(flow_file);
  process_session.read(flow_file, &read_until_it_can_callback);
  CHECK(to_string(read_result) == "myfoobar");
  CHECK(read_until_it_can_callback.value_ == "myfoobar");
}

void testAppendToManagedFlowFile(std::shared_ptr<core::ContentRepository> content_repo) {
  Fixture fixture = Fixture(content_repo);
  core::ProcessSession& process_session = fixture.processSession();
  const auto flow_file = process_session.create();
  REQUIRE(flow_file);

  fixture.writeToFlowFile(flow_file, "my");
  fixture.appendToFlowFile(flow_file, "foobar");
  fixture.transferAndCommit(flow_file);

  CHECK(flow_file->getSize() == 8);
  const auto read_result = process_session.readBuffer(flow_file);
  ReadUntilItCan read_until_it_can_callback;
  process_session.read(flow_file, &read_until_it_can_callback);
  CHECK(to_string(read_result) == "myfoobar");
  CHECK(read_until_it_can_callback.value_ == "myfoobar");
}
}  // namespace ContentRepositoryDependentTests
