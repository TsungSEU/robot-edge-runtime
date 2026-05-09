#pragma once

#include <string>
#include "common/data.h"
#include "protocol/data_protocol.h"

namespace aurora::collector {

// 数据上传器接口
class IDataUploader {
public:
    virtual ~IDataUploader() = default;
    
    virtual bool Init(const AppConfigData::DataUpload& config) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual ErrorCode UploadFile(const std::string& full_path, UploadType upload_type) = 0;
};

}