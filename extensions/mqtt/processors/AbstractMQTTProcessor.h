/**
 * @file AbstractMQTTProcessor.h
 * AbstractMQTTProcessor class declaration
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

#include <set>
#include <string>
#include <memory>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

static constexpr const char* const MQTT_QOS_0 = "0";
static constexpr const char* const MQTT_QOS_1 = "1";
static constexpr const char* const MQTT_QOS_2 = "2";

static constexpr const char* const MQTT_SECURITY_PROTOCOL_PLAINTEXT = "plaintext";
static constexpr const char* const MQTT_SECURITY_PROTOCOL_SSL = "ssl";

// AbstractMQTTProcessor Class
class AbstractMQTTProcessor : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit AbstractMQTTProcessor(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
    client_ = nullptr;
    cleanSession_ = false;
    qos_ = 0;
    isSubscriber_ = false;
  }
  // Destructor
  ~AbstractMQTTProcessor() override {
    if (isSubscriber_) {
      MQTTClient_unsubscribe(client_, topic_.c_str());
    }
    if (client_ && MQTTClient_isConnected(client_)) {
      MQTTClient_disconnect(client_, std::chrono::milliseconds{connectionTimeout_}.count());
    }
    if (client_)
      MQTTClient_destroy(&client_);
  }
  // Processor Name
  static constexpr char const* ProcessorName = "AbstractMQTTProcessor";
  // Supported Properties
  static core::Property BrokerURL;
  static core::Property ClientID;
  static core::Property UserName;
  static core::Property PassWord;
  static core::Property CleanSession;
  static core::Property KeepLiveInterval;
  static core::Property ConnectionTimeout;
  static core::Property Topic;
  static core::Property QOS;
  static core::Property SecurityProtocol;
  static core::Property SecurityCA;
  static core::Property SecurityCert;
  static core::Property SecurityPrivateKey;
  static core::Property SecurityPrivateKeyPassWord;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

  // MQTT async callbacks
  static void msgDelivered(void *context, MQTTClient_deliveryToken dt) {
    AbstractMQTTProcessor *processor = reinterpret_cast<AbstractMQTTProcessor *>(context);
    processor->delivered_token_ = dt;
  }
  static int msgReceived(void *context, char *topicName, int /*topicLen*/, MQTTClient_message *message) {
    AbstractMQTTProcessor *processor = reinterpret_cast<AbstractMQTTProcessor *>(context);
    if (processor->isSubscriber_) {
      if (!processor->enqueueReceiveMQTTMsg(message))
        MQTTClient_freeMessage(&message);
    } else {
      MQTTClient_freeMessage(&message);
    }
    MQTTClient_free(topicName);
    return 1;
  }
  static void connectionLost(void *context, char* /*cause*/) {
    AbstractMQTTProcessor *processor = reinterpret_cast<AbstractMQTTProcessor *>(context);
    processor->reconnect();
  }
  bool reconnect();
  // enqueue receive MQTT message
  virtual bool enqueueReceiveMQTTMsg(MQTTClient_message* /*message*/) {
    return false;
  }

 protected:
  static const std::set<core::Property> getSupportedProperties();

  MQTTClient client_;
  MQTTClient_deliveryToken delivered_token_;
  std::string uri_;
  std::string topic_;
  std::chrono::milliseconds keepAliveInterval_ = std::chrono::seconds(60);
  std::chrono::milliseconds connectionTimeout_ = std::chrono::seconds(30);
  int64_t qos_;
  bool cleanSession_;
  std::string clientID_;
  std::string userName_;
  std::string passWord_;
  bool isSubscriber_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger();
  MQTTClient_SSLOptions sslopts_;
  bool sslEnabled_;
  std::string securityCA_;
  std::string securityCert_;
  std::string securityPrivateKey_;
  std::string securityPrivateKeyPassWord_;
};


} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
