//
// Created by xucong on 25-7-8.
// Copyright (c) 2025 T3CAIC Group Limited. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#ifndef MQTT_WRAPPER_H
#define MQTT_WRAPPER_H

#include <string>
#include <memory>
#include <vector>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

namespace aurora::collector {

class MqttWrapper {
public:
    enum class ErrorCode {
        SUCCESS = 0,
        INIT_FAILED,
        CONNECTION_FAILED,
        SSL_FAILED,
        PUBLISH_FAILED,
        SUBSCRIBE_FAILED,
        INVALID_PARAMETER
    };

    MqttWrapper();
    ~MqttWrapper();

    ErrorCode Init(const std::string& serverURI,
                   const std::string& clientId,
                   const std::string& username,
                   const std::string& password,
                   int keepAliveInterval= 5);

    ErrorCode Init(const std::string& serverURI,
                   const std::string& clientId,
                   const std::string& username,
                   const std::string& password,
                   const std::string& caCertPath,
                   const std::string& clientCertPath,
                   const std::string& clientKeyPath,
                   int keepAliveInterval= 5);

    ErrorCode Connect(int timeoutMs = 5000); // 5s

    void Disconnect();

    ErrorCode Publish(const std::string& topic,
                      const std::string& payload,
                      int qos = 1,
                      bool retain = false);

    ErrorCode Subscribe(const std::string& topic,
                        int qos = 1);

    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
    void SetMessageCallback(MessageCallback callback);

    MqttWrapper(const MqttWrapper&) = delete;
    MqttWrapper& operator=(const MqttWrapper&) = delete;

private:
    class MqttCallbackImpl : public virtual mqtt::callback {
    public:
        MqttCallbackImpl(MqttWrapper* parent);
        void message_arrived(mqtt::const_message_ptr msg) override;
        void connection_lost(const std::string& cause) override;
    private:
        MqttWrapper* parent_;
    };

    std::unique_ptr<mqtt::async_client> client_;
    std::unique_ptr<MqttCallbackImpl> callback_;
    mqtt::connect_options connOpts_;
    MessageCallback userCallback_;
    std::string serverURI_;
    std::string clientId_;
    bool isConnected_;
    bool autoReconnect_;
    std::mutex mutex_;
    std::vector<std::pair<std::string, int>> subscriptions_;
};

}

#endif //MQTT_WRAPPER_H
