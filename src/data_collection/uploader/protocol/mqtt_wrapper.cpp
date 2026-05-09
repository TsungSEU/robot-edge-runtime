//
// Created by xucong on 25-7-8.
// Copyright (c) 2025 T3CAIC Group Limited. All rights reserved.
// Tsung Xu<xucong@t3caic.com>
//

#include "mqtt_wrapper.h"
#include "common/log/logger.h"

namespace aurora::collector {

MqttWrapper::MqttWrapper() : isConnected_(false), autoReconnect_(true) {}

MqttWrapper::~MqttWrapper() {
    if (isConnected_)
        Disconnect();
}

MqttWrapper::ErrorCode MqttWrapper::Init(const std::string& serverURI,
                                         const std::string& clientId,
                                         const std::string& username,
                                         const std::string& password,
                                         int keepAliveInterval) {
    if (serverURI.empty() || clientId.empty() || keepAliveInterval <= 0) {
        return ErrorCode::INVALID_PARAMETER;
    }

    serverURI_ = serverURI;
    clientId_ = clientId;
    callback_ = std::make_unique<MqttCallbackImpl>(this);
    client_ = std::make_unique<mqtt::async_client>(serverURI, clientId);
    client_->set_callback(*callback_);

    // 配置conn
    connOpts_ = mqtt::connect_options_builder()
                .keep_alive_interval(std::chrono::seconds(keepAliveInterval))
                .clean_session(true)
                .user_name(username)
                .password( password)
                .finalize();

    return ErrorCode::SUCCESS;

}

MqttWrapper::ErrorCode MqttWrapper::Init(const std::string& serverURI,
                                         const std::string& clientId,
                                         const std::string& username,
                                         const std::string& password,
                                         const std::string& caCertPath,
                                         const std::string& clientCertPath,
                                         const std::string& clientKeyPath,
                                         int keepAliveInterval) {
    if (serverURI.empty() || clientId.empty() || keepAliveInterval <= 0) {
        return ErrorCode::INVALID_PARAMETER;
    }

    serverURI_ = serverURI;
    clientId_ = clientId;
    callback_ = std::make_unique<MqttCallbackImpl>(this);
    client_ = std::make_unique<mqtt::async_client>(serverURI, clientId);
    client_->set_callback(*callback_);

    // 配置SSL
    auto ssl_opts = mqtt::ssl_options_builder()
                    .trust_store(caCertPath)
                    .key_store(clientCertPath)
                    .private_key(clientKeyPath)
                    .verify(true)
                    .finalize();

    connOpts_ = mqtt::connect_options_builder()
                .keep_alive_interval(std::chrono::seconds(keepAliveInterval))
                .clean_session(true)
                .ssl(std::move(ssl_opts))
                .user_name(username)
                .password( password)
                .finalize();

    return ErrorCode::SUCCESS;
}

MqttWrapper::ErrorCode MqttWrapper::Connect(int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isConnected_ || !client_) {
        return isConnected_ ? ErrorCode::SUCCESS : ErrorCode::INIT_FAILED;
    }

    try {
        mqtt::token_ptr tok = client_->connect(connOpts_);
        tok->wait();
        // tok->wait_for(std::chrono::milliseconds(timeoutMs));
        isConnected_ = true;
        return ErrorCode::SUCCESS;
    } catch (const mqtt::exception&) {
        return ErrorCode::CONNECTION_FAILED;
    }
}

void MqttWrapper::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isConnected_ && client_) {
        try {
            client_->disconnect()->wait_for(std::chrono::seconds(2));
        } catch (...) {}
        isConnected_ = false;
    }
}

MqttWrapper::ErrorCode MqttWrapper::Publish(const std::string& topic,
                                            const std::string& payload,
                                            int qos,
                                            bool retain) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isConnected_ || topic.empty() || qos < 0 || qos > 2) {
        return ErrorCode::INVALID_PARAMETER;
    }

    try {
        auto msg = mqtt::make_message(topic, payload, qos, retain);
        client_->publish(msg)->wait_for(std::chrono::seconds(3));
        return ErrorCode::SUCCESS;
    } catch (const mqtt::exception&) {
        return ErrorCode::PUBLISH_FAILED;
    }
}

MqttWrapper::ErrorCode MqttWrapper::Subscribe(const std::string& topic, int qos) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isConnected_ || topic.empty() || qos < 0 || qos > 2) {
        return ErrorCode::INVALID_PARAMETER;
    }

    try {
        mqtt::token_ptr tok = client_->subscribe(topic, qos);
        tok->wait();
        // tok->wait_for(std::chrono::seconds(3));
        if (tok->get_return_code() == mqtt::ReasonCode::SUCCESS) {
            subscriptions_.emplace_back(topic, qos);
            return ErrorCode::SUCCESS;
        }
    } catch (const mqtt::exception&) {
        return ErrorCode::SUBSCRIBE_FAILED;
    }
}

MqttWrapper::MqttCallbackImpl::MqttCallbackImpl(MqttWrapper* parent) {
    this->parent_ = parent;
}

void MqttWrapper::MqttCallbackImpl::message_arrived(mqtt::const_message_ptr msg) {
    if (parent_->userCallback_) {
        parent_->userCallback_(msg->get_topic(), msg->to_string());
    }

}

void MqttWrapper::MqttCallbackImpl::connection_lost(const std::string& cause) {
    std::lock_guard<std::mutex> lock(parent_->mutex_);
    parent_->isConnected_ = false;
    AD_WARN(MqttWrapper, "MQTT connection lost: %s", cause.c_str());

    if (parent_->autoReconnect_) {
        std::thread([parent = parent_]() {
            int retryCount = 0;
            const int maxRetries = 5;
            double retryIntervalMs = 5000.0;  // 初始5秒，指数退避

            while (retryCount < maxRetries && !parent->isConnected_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int64_t>(retryIntervalMs)));
                AD_INFO(MqttWrapper, "MQTT reconnect attempt %d/%d", retryCount + 1, maxRetries);

                if (parent->Connect(3000) == ErrorCode::SUCCESS) {
                    std::lock_guard<std::mutex> lock(parent->mutex_);
                    for (const auto& sub : parent->subscriptions_) {
                        parent->client_->subscribe(sub.first, sub.second)->wait();
                    }
                    AD_INFO(MqttWrapper, "MQTT reconnected successfully");
                    break;
                }
                retryCount++;
                retryIntervalMs = std::min(retryIntervalMs * 2.0, 60000.0);  // 最大60秒
            }

            if (retryCount >= maxRetries && !parent->isConnected_) {
                AD_ERROR(MqttWrapper, "MQTT reconnection failed after %d attempts, giving up", maxRetries);
            }
        }).detach();
    }
}

void MqttWrapper::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    userCallback_ = std::move(callback);
}


}
