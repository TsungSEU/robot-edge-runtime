/*
 * Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
 * Tsung Xu<congx0829@163.com>
 */

#include "data_protocol.h"

#include <atomic>
#include <nlohmann/json.hpp>
#include "common/data.h"

namespace aurora::collector {

ErrorCode CurlErrorMapping(CURLcode code) {
    switch (code) {
    case CURLE_OK:
        return ErrorCode::SUCCESS;
    case CURLE_URL_MALFORMAT:
        return ErrorCode::URL_ERROR;
    case CURLE_COULDNT_CONNECT:
        return ErrorCode::CONNECT_ERROR;
    case CURLE_OPERATION_TIMEDOUT:
        return ErrorCode::TIMEOUT;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }
}

bool DataProto::Init(const std::string& gateway,
                        const std::string& client_cert_path,
                        const std::string& client_key_path,
                        const std::string& ca_cert_path) {
    gateway_ = gateway;
    // 初始化MqttWrapper
    // mqtt_wrapper_ = std::make_unique<MqttWrapper>();

    // curl_wrapper_初始化保持不变
    auto ret = curl_wrapper_.Init(client_cert_path, client_key_path, ca_cert_path);
    return ret == CURLE_OK;
}

ErrorCode DataProto::GetQueryTask(const std::string& vin, QueryTaskResp& resp) {
    url_ = "https://" + gateway_ + "/feedback/driving/queryTask?vin=" + vin;
    std::string resp_str;
    auto ret = curl_wrapper_.HttpGet(url_, resp_str, {"Accept: application/json"});
    AD_WARN(DataProto, "Response: %s", resp_str.c_str());
    bool success = response_parser(resp_str, resp);
    if (!success) {
        return ErrorCode::INVALID_RESPONSE;
    }
    return CurlErrorMapping(ret);
}

ErrorCode DataProto::SendUploadMqttCmd(std::atomic<bool>& stop_flag) {
    // 读取mqtt配置文件
    const auto& app_config = AppConfig::getInstance().GetConfig();

    // mqtt
    std::string mqtt_broker_ssl = app_config.dataProto.mqtt.broker_ssl;
    std::string mqtt_username = app_config.dataProto.mqtt.username;
    std::string mqtt_password = app_config.dataProto.mqtt.password;

    std::string ca_cert = app_config.dataUpload.caCertPath;
    std::string client_cert = app_config.dataUpload.clientCertPath;
    std::string client_key = app_config.dataUpload.clientKeyPath;

    mqtt_wrapper_ = std::make_shared<MqttWrapper>();
    if (!app_config.debug.closeMqttSsl) {
        mqtt_wrapper_->Init(mqtt_broker_ssl, "shadow_tbox_" + app_config.dataProto.vin,
                            mqtt_username, mqtt_password, ca_cert, client_cert, client_key);
    }

    mqtt_wrapper_->SetMessageCallback([this](const std::string& topic, const std::string& payload) {
        AD_INFO(DataProto, "Message arrived: %s", payload.c_str());
        // common::MqttCmd mqtt_cmd;
        QueryTaskResp::Object mqtt_task;
        // auto success = common::response_parser(payload, mqtt_cmd);
        auto& pq = TaskQueue::GetInstance();
        auto log_task = GetLogTaskInfo(mqtt_task, UploadType::InstructionDelivery);
        pq.Push(2, log_task);
        AD_INFO(DataProto, "Add mqtt task to TaskQueue.");
    });

    auto ret = mqtt_wrapper_->Connect();
    if (ret != MqttWrapper::ErrorCode::SUCCESS) {
        AD_ERROR(DataProto, "MqttInit, Connect failed, ret: %d", ret);
        return ErrorCode::CONNECT_ERROR;
    }

    AD_INFO(DataProto, "MqttInit, Connect success");

    // Subscribe to topic
    AD_INFO(DataProto, "MqttInit, Subscribing to topic: %s", app_config.dataProto.mqtt.downTopic.c_str());
    mqtt_wrapper_->Subscribe(app_config.dataProto.mqtt.downTopic, 1);

    // 保持连接直到收到停止信号
    while (!stop_flag.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 断开连接
    mqtt_wrapper_->Disconnect();

    return ErrorCode::SUCCESS;
}

ErrorCode DataProto::GetUploadUrl(const UploadUrlReq& req, UploadUrlResp& resp) {
    // json j = req;
    json j = json{
        {"type", static_cast<int>(req.type)},
        {"partNumber", req.part_number},
        {"fileName", req.filename},
        {"vin", req.vin},
        // {"taskId", req.task_id},
        // {"expireMinutes", req.expire_minutes},
    };

    url_ = "https://" + gateway_ + "/msinfofeedback/common/file/uploadurl";
    std::string resp_str;
    auto ret = curl_wrapper_.HttpPost(
        url_, j.dump(), resp_str, {"Content-Type: application/json", "Accept: application/json"});
    AD_WARN(DataProto, "Response: %s", resp_str.c_str());
    auto parsed = json::parse(resp_str);
    bool success = response_parser(resp_str, resp);
    if (!success) {
        return ErrorCode::INVALID_RESPONSE;
    }
    return CurlErrorMapping(ret);
}

ErrorCode DataProto::UploadFileChunk(const std::vector<char>& buffer, const std::string& upload_url, std::string& resp) {
    resp.clear();
    std::string resp_str;
    auto ret = curl_wrapper_.HttpPut(
        upload_url, buffer, resp_str, {"Content-Type:"});
    AD_INFO(DataProto, "HttpPut resp tag: %s", resp.c_str());
    std::istringstream header_stream(resp_str);
    AD_INFO(DataProto, "HttpPut ret: %d", ret);

    std::string line;
    while (std::getline(header_stream, line)) {
        if (line.size() >= 40 && line.substr(0, 5) == "ETag:") {
            resp = line.substr(7, 32);
            AD_WARN(DataProto, "ETag: %s", resp.c_str());
            break;
        }
    }
    return resp.empty() ? ErrorCode::INVALID_RESPONSE : ErrorCode::SUCCESS;
}

ErrorCode DataProto::CompleteUpload(const CompleteUploadReq& req, CompleteUploadResp& resp) {
    url_ = "https://" + gateway_ + "/msinfofeedback/common/file/completeupload";
    // json j = req;
    json j = json{
        {"vin", req.vin},
        {"type", req.type},
        {"fileUuid", req.file_uuid},
        {"uploadStatus", static_cast<int>(req.upload_status)},
        {"uploadId", req.upload_id},
        // {"taskId", req.task_id},
        {"etagMap", json::object()},
    };

    std::string resp_str;
    auto ret = curl_wrapper_.HttpPost(
        url_, j.dump(), resp_str, {"Content-Type: application/json", "Accept: application/json"});
    AD_WARN(DataProto, "Response: %s", resp_str.c_str());
    bool success = response_parser(resp_str, resp);
    if (!success) {
        return ErrorCode::INVALID_RESPONSE;
    }
    return CurlErrorMapping(ret);
}

ErrorCode DataProto::GetUploadStatus(const std::string& file_uuid, UploadStatusResp& resp) {
    std::string resp_str;
    url_ = "https://" + gateway_ + "/msinfofeedback/common/file/uploadstatus";
    json j = json{{"fileUuid", file_uuid}};
    auto ret = curl_wrapper_.HttpPost(
        url_, j.dump(), resp_str, {"Content-Type: application/json", "Accept: application/json"});
    AD_WARN(DataProto, "Response: %s", resp_str.c_str());
    bool success = response_parser(resp_str, resp);
    if (!success) {
        return ErrorCode::INVALID_RESPONSE;
    }
    return CurlErrorMapping(ret);
}

}
