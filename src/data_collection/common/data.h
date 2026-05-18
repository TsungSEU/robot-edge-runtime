//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（Order Shapes Intelligence）. All rights reserved.
//

#ifndef DATA_H
#define DATA_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <map>


namespace aurora::collector {

using json = nlohmann::json;

struct TBussiness {
    std::string bussiness_type;
    std::string data_source; // Cam Radar Log Other
    std::string data_type; // syslog、otherlog、edr、 AutoDrivingData
};

const std::vector<TBussiness> kBussiness{
    {"NOP", "Other", "AutoDrivingData"},
    {"AVP", "Other", "AutoDrivingData"},
    {"EDR", "Other", "edr"},
    {"ITC", "Log", "syslog"}
};

enum class UploadStatus : uint8_t
{ 
    Uploaded = 3, 
    Failed = 4,  
};

enum class UploadType : uint8_t
{
    None = 0,
    ActivelyReport = 3,
    InstructionDelivery = 4,
};

// 上传URL请求
struct UploadUrlReq {
    UploadType type; // 业务类型 (3:智驾日志主动上传;4:智驾日志指令下发)
    std::string filename; // 文件名称
    int part_number; // 文件分段数 (单段文件大小限制为 100kb-5gb,最后一段为 0-5gb, 最多 10000 段)
    std::string task_id; // 任务 id
    std::string vin; // vin 码
    int expire_minutes; // 地址过期时间(单位分钟，非必填，默认三天)
};

struct UploadUrlResp {
    struct Object {
        std::string file_uuid; // 文件uuid
        std::string upload_id; // 分段上传uploadId
        std::map<std::string, std::string> upload_url_map; // 预签名上传地址列表，key-分片序号，value-分片序号对应预签名上传地址
        std::string filename; // 文件名称
    };
    std::string status_code; // 状态码0表示成功
    std::string status_message; // 状态信息
    Object data;
};

// 反馈请求
struct FeedBackReq {
    std::string text_content;
    std::string soft_version;
    std::string vin;
    std::string pdsn;
    std::string log_url;
    int64_t file_size;
    std::string tbox_pdsn;
    int64_t start_time;
    int64_t end_time;
    std::string device;
    std::string deviceId;
    std::string deviceType;
    std::string deviceModel;
    std::vector<int> devicesIdList;
    std::string deviceSerial;
    std::string deviceManufacturer;
    std::vector<int> devicesTypeList;
    std::string deviceManufacturerName;
    std::string deviceModelName;
    std::vector<int> devicesManufacturerList;
    std::string authorization;
};

// 完成上传请求
struct CompleteUploadReq {
    std::string vin;
    UploadType type;
    std::string file_uuid;
    UploadStatus upload_status;
    std::string upload_id;
    std::string task_id;
    std::map<std::string, std::string> etag_map;
};

struct CompleteUploadResp {
    struct Object {
        std::string pub_download_url; // 文件公开时返回
        std::string presign_download_url; // 文件非公开时返回（临时下载可使用）
    };
    std::string status_code; // 状态码0表示成功
    std::string status_message; // 状态信息
    Object data;
};

struct UploadStatusResp {
    struct UploadPart {
        int part_number; // 成功上传的分片序号
        std::string etag; // 成功上传返回的etag
    };
    struct Object {
        UploadStatus upload_status; // 整个文件上传状态：1-开始上传，3-上传完成，4-上传失败
        std::vector<UploadPart> uploaded_part_list; // 只有upload_status为1（开始上传）状态下才返回
    };
    std::string status_code; // 状态码0表示成功
    std::string status_message; // 状态信息
    Object data;
};

// 文件上传记录
struct FileUploadRecord {
    int8_t start_chunk;
    std::string file_uuid;
    std::map<int, std::string> upload_url_map;
    std::map<int, std::string> uploaded_url_map;
    std::string upload_id;
    int8_t chunk_count;
};

// 文件上传进度
struct FileUploadProgress {
    std::string vin;
    std::string taskId;
    std::string batchId;   
    std::string uploadUrl;
    std::string fileName;
    std::string fileUuid;
    double dataSize;
    int8_t uploadStatus;
    float progress;

};

// 系统状态信息
struct SysInfo {
    std::string taskId;
    float cpuUsage;
    float memUsage;
    float harddriveUsage;
    float ssdUsage;
    int8_t imageFrameRateException;
    int8_t pcFrameRateException;
    int8_t canbusFrameRateException;
    int8_t insFrameRateException;
};


// 采集状态信息
struct CollectInfo {
    std::string vin;
    std::string taskId;
    double distanceCollect;
    int32_t durationCollect;
    int32_t triggercountCollect;
    double bagcapacityCollect;
    double distanceUpload;
    int32_t durationUpload;
    int32_t triggercountUpload;
    double bagcapacityUpload; 
};

// 采集状态信息
struct RtspInfo {
    std::string taskId; 
    int8_t status;
    std::string rtspUrl;
};

struct OBUStateInfo {
    std::string vin;
    std::string longitude;
    std::string latitude;
    std::string heading;
    std::string speed;
    std::string accSpeed;
    std::string gpsTime;
    std::string altitude;
    std::string rtkState;
    std::string light;
    int32_t brakeState;
    int32_t gasState;
    std::string gearState;
    std::string wheelAngle;
    std::string driveState;
    std::string abnormalState;
    int32_t traffic_light;
    std::string mileage;
    std::string engineSpeed;
    int8_t chargeState;
    int8_t handbrake;
    int8_t startState;
    std::string elecQuantity;
    std::string endurance;
    int8_t starNm;
    int8_t collectState;
};


struct CPUData {
    long user;
    long nice;
    long system;
    long idle;
    long iowait;
    long irq;
    long softirq;
};

struct FrameCounter {
    int32_t frame_counter_canbus = 0; 
    int32_t frame_counter_inspva = 0; 
    int32_t frame_counter_img = 0; 
    int32_t frame_counter_pc = 0; 
};

enum LogType {
    Syslog = 1,
    Otherlog = 2,
    EDR = 3,
    DSSAD = 4,
    Reserved = 5,
};

struct QueryTaskResp {
    struct Object {
        std::string vin; // vin码
        std::vector<int> log_type; // 日志类别集合（1-syslog, 2-otherlog, 3-edr, 4-reserved）
        std::string start_date; // 开始时间起（时间戳）
        std::string end_date; // 结束时间止（时间戳）
        std::string task_id; // 任务id
        std::string task_status; // 任务状态（0-等待执行，1-执行中）
    };
    std::string status_code; // 状态码0表示成功
    std::string status_message; // 状态信息
    std::vector<Object> data; // 数据集合
};

struct LogUploadTask {
    std::string vin;
    std::vector<int> log_type;
    std::string start_date;
    std::string end_date;
    std::string task_id;
    UploadType upload_type;
};

// upload status
void to_json(json& j, UploadType type);
void from_json(const json& j, UploadType& type);
void to_json(json& j, UploadStatus status);
void from_json(const json& j, UploadStatus& status);

// url response
void to_json(json& j, const UploadUrlResp::Object& o);
void from_json(const json& j, UploadUrlResp::Object& o);
void to_json(json& j, const UploadUrlResp& r);
void from_json(const json& j, UploadUrlResp& r);
void to_json(json& j, const CompleteUploadResp::Object& o);
void from_json(const json& j, CompleteUploadResp::Object& o);
void to_json(json& j, const CompleteUploadResp& r);
void from_json(const json& j, CompleteUploadResp& r);
void to_json(json& j, const UploadStatusResp::UploadPart& p);
void from_json(const json& j, UploadStatusResp::UploadPart& p);
void to_json(json& j, const UploadStatusResp::Object& o);
void from_json(const json& j, UploadStatusResp::Object& o);
void to_json(json& j, const UploadStatusResp& r);
void from_json(const json& j, UploadStatusResp& r);
void to_json(json& j, const QueryTaskResp::Object& o);
void from_json(const json& j, QueryTaskResp::Object& o);
void to_json(json& j, const QueryTaskResp& r);
void from_json(const json& j, QueryTaskResp& r);

// url request
void to_json(json& j, const UploadUrlReq& r);
void from_json(const json& j, UploadUrlReq& r);
void to_json(json& j, const CompleteUploadReq& r);
void from_json(const json& j, CompleteUploadReq& r);
void to_json(json& j, const FileUploadRecord& r);
void from_json(const json& j, FileUploadRecord& r);

// data report
void to_json(json& j, const FileUploadProgress& r);
void from_json(const json& j, FileUploadProgress& r);
void to_json(json& j, const SysInfo& r);
void from_json(const json& j, SysInfo& r);
void to_json(json& j, const CollectInfo& r);
void from_json(const json& j, CollectInfo& r);
void to_json(json& j, const RtspInfo& r);
void from_json(const json& j, RtspInfo& r);
void to_json(json& j, const OBUStateInfo& r);
void from_json(const json& j, OBUStateInfo& r);

template <typename T>
bool response_parser(const std::string& json_str, T& resp) {
    try {
        auto parsed = json::parse(json_str);
        resp = parsed.get<T>();
    } catch (...) {
        return false;
    }
    return true;
}


inline LogUploadTask GetLogTaskInfo(const QueryTaskResp::Object& obj, UploadType type) {
    LogUploadTask task;
    task.vin = obj.vin;
    task.log_type = std::move(obj.log_type);
    task.start_date = obj.start_date;
    task.end_date = obj.end_date;
    task.task_id = obj.task_id;
    task.upload_type = type;
    return task;
}

} 


#endif //DATA_H
