/*
 * Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
 * Tsung Xu<congx0829@163.com>
 */

#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H

#include <string>
#include <mqtt/async_client.h>
#include <mqtt/ssl_options.h>
#include "curl_wrapper.h"
#include "mqtt_wrapper.h"
#include "common/data.h"
#include "common/log/logger.h"
#include "../../common/log_task_queue.hpp"
#include "../../common/app_config.h"


enum ErrorCode {
    SUCCESS = 0,
    TIMEOUT = 1,
    CONNECT_ERROR = 2,
    FILE_NOT_FOUND = 3,
    FILE_CHUNK_ERROR = 4,
    URL_ERROR = 5,
    INVALID_RESPONSE = 6,
    UPLOAD_INCOMPLETE = 7,
    UNKNOWN_ERROR = 8,
};

namespace aurora::collector {

using json = nlohmann::json;

ErrorCode CurlErrorMapping(CURLcode code);

class DataProto {
public:
    DataProto() = default;
    ~DataProto() = default;
    bool Init(const std::string& gateway,
              const std::string& client_cert_path = "",
              const std::string& client_key_path = "",
              const std::string& ca_cert_path = "");
    ErrorCode GetQueryTask(const std::string& vin, QueryTaskResp& resp);
    ErrorCode SendUploadMqttCmd(std::atomic<bool>& stop_flag);
    ErrorCode GetUploadUrl(const UploadUrlReq& req, UploadUrlResp& resp);
    ErrorCode UploadFileChunk(const std::vector<char>& buffer, const std::string& , std::string& resp);
    ErrorCode CompleteUpload(const CompleteUploadReq& req, CompleteUploadResp& resp);
    ErrorCode GetUploadStatus(const std::string& file_uuid, UploadStatusResp& resp);

private:
    CurlWrapper curl_wrapper_;
    std::string url_;
    std::string gateway_;
    AppConfigData::DataUpload config_;
    std::shared_ptr<MqttWrapper> mqtt_wrapper_;
};

}


#endif //DATA_PROTOCOL_H
