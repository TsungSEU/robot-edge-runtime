//
// Created by xucong on 25-5-7.
// Copyright (c) 2025 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#include "data.h"

namespace aurora::collector {
void to_json(json& j, UploadType type) {
    j = static_cast<int>(type);
}

void from_json(const json& j, UploadType& type) {
    type = static_cast<UploadType>(j.get<int>());
}

void to_json(json& j, UploadStatus status) {
    j = static_cast<int>(status);
}

void from_json(const json& j, UploadStatus& status) {
    status = static_cast<UploadStatus>(j.get<int>());
}

void to_json(json& j, const UploadUrlResp::Object& o) {
    j = json{
        {"fileUuid", o.file_uuid},
        {"uploadId", o.upload_id},
        {"partPresignUploadUrlMap", o.upload_url_map},
        {"fileName", o.filename}
    };
}

void from_json(const json& j, UploadUrlResp::Object& o) {
    if (!j.at("fileUuid").is_null()) {
        j.at("fileUuid").get_to(o.file_uuid);
    }
    if (!j.at("uploadId").is_null()) {
        j.at("uploadId").get_to(o.upload_id);
    }
    if (!j.at("partPresignUploadUrlMap").is_null()) {
        j.at("partPresignUploadUrlMap").get_to(o.upload_url_map);
    }
    if (!j.at("fileName").is_null()) {
        j.at("fileName").get_to(o.filename);
    }
}

void to_json(json& j, const UploadUrlResp& r) {
    j = json{
        {"statusCode", r.status_code},
        {"statusMessage", r.status_message},
        {"data", r.data}
    };
}

void from_json(const json& j, UploadUrlResp& r) {
    if (!j.at("statusCode").is_null()) {
        j.at("statusCode").get_to(r.status_code);
    }
    if (!j.at("statusMessage").is_null()) {
        j.at("statusMessage").get_to(r.status_message);
    }
    if (!j.at("data").is_null()) {
        j.at("data").get_to(r.data);
    }
}

void to_json(json& j, const CompleteUploadResp::Object& o) {
    j = json{
        {"pubDownloadUrl", o.pub_download_url},
        {"presignDownloadUrl", o.presign_download_url}
    };
}

void from_json(const json& j, CompleteUploadResp::Object& o) {
    if (j.contains("pubDownloadUrl") && j.at("pubDownloadUrl").is_string()) {
        j.at("pubDownloadUrl").get_to(o.pub_download_url);
    }
    if (j.contains("presignDownloadUrl") && j.at("presignDownloadUrl").is_string()) {
        j.at("presignDownloadUrl").get_to(o.presign_download_url);
    }
}

void to_json(json& j, const CompleteUploadResp& r) {
    j = json{
        {"statusCode", r.status_code},
        {"statusMessage", r.status_message},
        {"data", r.data}
    };
}
void from_json(const json& j, CompleteUploadResp& r) {
    if (!j.at("statusCode").is_null()) {
        j.at("statusCode").get_to(r.status_code);
    }
    if (!j.at("statusMessage").is_null()) {
        j.at("statusMessage").get_to(r.status_message);
    }
    if (!j.at("data").is_null()) {
        j.at("data").get_to(r.data);
    }
}

void to_json(json& j, const UploadStatusResp::UploadPart& p) {
    j = json{
        {"partNumber", p.part_number},
        {"etag", p.etag}
    };
}

void from_json(const json& j, UploadStatusResp::UploadPart& p) {
    if (!j.at("partNumber").is_null()) {
        j.at("partNumber").get_to(p.part_number);
    }
    if (!j.at("etag").is_null()) {
        j.at("etag").get_to(p.etag);
    }
}

void to_json(json& j, const UploadStatusResp::Object& o) {
    j = json{
        {"uploadStatus", o.upload_status},
        {"uploadedPartList", o.uploaded_part_list}
    };
}

void from_json(const json& j, UploadStatusResp::Object& o) {
    if (!j.at("uploadStatus").is_null()) {
        j.at("uploadStatus").get_to(o.upload_status);
    }
    if (!j.at("uploadedPartList").is_null()) {
        j.at("uploadedPartList").get_to(o.uploaded_part_list);
    }
}

void to_json(json& j, const UploadStatusResp& r) {
    j = json{
        {"statusCode", r.status_code},
        {"statusMessage", r.status_message},
        {"data", r.data}
    };
}

void from_json(const json& j, UploadStatusResp& r) {
    if (!j.at("statusCode").is_null()) {
        j.at("statusCode").get_to(r.status_code);
    }
    if (!j.at("statusMessage").is_null()) {
        j.at("statusMessage").get_to(r.status_message);
    }
    if (!j.at("data").is_null()) {
        j.at("data").get_to(r.data);
    }
}

void to_json(json& j, const QueryTaskResp::Object& o) {
    j = json{
        {"vin", o.vin},
        {"logType", o.log_type},
        {"startDate", o.start_date},
        {"endDate", o.end_date},
        {"taskId", o.task_id},
        {"taskStatus", o.task_status}
    };
}

void from_json(const json& j, QueryTaskResp::Object& o) {
    if (j.contains("vin") && !j.at("vin").is_null()) {
        j.at("vin").get_to(o.vin);
    }
    if (j.contains("logType") && !j.at("logType").is_null()) {
        j.at("logType").get_to(o.log_type);
    }
    if (j.contains("startDate") && !j.at("startDate").is_null()) {
        j.at("startDate").get_to(o.start_date);
    }
    if (j.contains("endDate") && !j.at("endDate").is_null()) {
        j.at("endDate").get_to(o.end_date);
    }
    if (j.contains("taskId") && !j.at("taskId").is_null()) {
        j.at("taskId").get_to(o.task_id);
    }
    if (j.contains("taskStatus") && !j.at("taskStatus").is_null()) {
        j.at("taskStatus").get_to(o.task_status);
    }
}

void to_json(json& j, const QueryTaskResp& r) {
    j = json{
        {"statusCode", r.status_code},
        {"statusMessage", r.status_message},
        {"data", r.data}
    };
}

void from_json(const json& j, QueryTaskResp& r) {
    if (!j.at("statusCode").is_null()) {
        j.at("statusCode").get_to(r.status_code);
    }
    if (!j.at("statusMessage").is_null()) {
        j.at("statusMessage").get_to(r.status_message);
    }
    if (!j.at("data").is_null()) {
        j.at("data").get_to(r.data);
    }
}

void to_json(json& j, const UploadUrlReq& r) {
    j = json{
        {"type", r.type},
        {"fileName", r.filename},
        {"partNumber", r.part_number},
        {"vin", r.vin}
    };
    if (!r.task_id.empty()) {
        j["taskId"] = r.task_id;
    }
    if (r.expire_minutes > 0) {
        j["expireMinutes"] = r.expire_minutes;
    }
}

void from_json(const json& j, UploadUrlReq& r) {
    if (!j.at("type").is_null()) {
        j.at("type").get_to(r.type);
    }
    if (!j.at("fileName").is_null()) {
        j.at("fileName").get_to(r.filename);
    }
    if (!j.at("partNumber").is_null()) {
        j.at("partNumber").get_to(r.part_number);
    }
    if (j.contains("taskId") && !j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.task_id);
    }
    if (!j.at("vin").is_null()) {
        j.at("vin").get_to(r.vin);
    }
    if (j.contains("expireMinutes") && !j.at("expireMinutes").is_null()) {
        j.at("expireMinutes").get_to(r.expire_minutes);
    }
}

void to_json(json& j, const CompleteUploadReq& r) {
    j = json{
        {"type", r.type},
        {"fileUuid", r.file_uuid},
        {"uploadStatus", r.upload_status},
        {"uploadId", r.upload_id},
        {"etagMap", r.etag_map},
        {"vin", r.vin}
    };
    if (!r.task_id.empty()) {
        j["taskId"] = r.task_id;
    }
}

void from_json(const json& j, CompleteUploadReq& r) {
    if (!j.at("type").is_null()) {
        j.at("type").get_to(r.type);
    }
    if (!j.at("fileUuid").is_null()) {
        j.at("fileUuid").get_to(r.file_uuid);
    }
    if (!j.at("uploadStatus").is_null()) {
        j.at("uploadStatus").get_to(r.upload_status);
    }
    if (!j.at("uploadId").is_null()) {
        j.at("uploadId").get_to(r.upload_id);
    }
    if (!j.at("etagMap").is_null()) {
        j.at("etagMap").get_to(r.etag_map);
    }
    if (j.contains("taskId") && !j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.task_id);
    }
    if (!j.at("vin").is_null()) {
        j.at("vin").get_to(r.vin);
    }
}

void to_json(json& j, const FileUploadRecord& r) {
    j = json{
        {"chunk_count", r.chunk_count},
        {"start_chunk", r.start_chunk},
        {"file_uuid", r.file_uuid},
        {"upload_id", r.upload_id},
        {"upload_url_map", r.upload_url_map}
    };
}

void from_json(const json& j, FileUploadRecord& r) {
    if (!j.at("chunk_count").is_null()) {
        j.at("chunk_count").get_to(r.chunk_count);
    }
    if (!j.at("start_chunk").is_null()) {
        j.at("start_chunk").get_to(r.start_chunk);
    }
    if (!j.at("file_uuid").is_null()) {
        j.at("file_uuid").get_to(r.file_uuid);
    }
    if (!j.at("upload_id").is_null()) {
        j.at("upload_id").get_to(r.upload_id);
    }
    if (!j.at("upload_url_map").is_null()) {
        j.at("upload_url_map").get_to(r.upload_url_map);
    }
}

void to_json(json& j, const FileUploadProgress& r) {
    j = json{
        {"vin", r.vin},
        {"taskId", r.taskId},
        {"batchId", r.batchId},
        {"uploadUrl", r.uploadUrl},
        {"fileName", r.fileName},
        {"fileUuid", r.fileUuid},
        {"dataSize", r.dataSize},
        {"uploadStatus", r.uploadStatus},
        {"progress" , r.progress}
    };
}

void from_json(const json& j, FileUploadProgress& r) {
    if (!j.at("vin").is_null()) {
        j.at("vin").get_to(r.vin);
    }
    if (!j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.taskId);
    }
    if (!j.at("batchId").is_null()) {
        j.at("batchId").get_to(r.batchId);
    }
    if (!j.at("uploadUrl").is_null()) {
        j.at("uploadUrl").get_to(r.uploadUrl);
    }
    if (!j.at("fileName").is_null()) {
        j.at("fileName").get_to(r.fileName);
    }
    if (!j.at("fileUuid").is_null()) {
        j.at("fileUuid").get_to(r.fileUuid);
    }
    if (!j.at("dataSize").is_null()) {
        j.at("dataSize").get_to(r.dataSize);
    }
    if (!j.at("uploadStatus").is_null()) {
        j.at("uploadStatus").get_to(r.uploadStatus);
    }
    if (!j.at("progress").is_null()) {
        j.at("progress").get_to(r.progress);
    }
}

void to_json(json& j, const SysInfo& r) {
    j = json{
        {"taskId", r.taskId},
        {"cpuUsage", r.cpuUsage},
        {"memUsage", r.memUsage},
        {"harddriveUsage", r.harddriveUsage},
        {"ssdUsage", 0.0},
        {"imageFrameRateException" , r.imageFrameRateException},
        {"pcFrameRateException" , r.pcFrameRateException},
        {"canbusFrameRateException", r.canbusFrameRateException},
        {"insFrameRateException ", r.insFrameRateException}
    };
}

void from_json(const json& j, SysInfo& r) {
    if (!j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.taskId);
    }
    if (!j.at("cpuUsage").is_null()) {
        j.at("cpuUsage").get_to(r.cpuUsage);
    }
    if (!j.at("memUsage").is_null()) {
        j.at("memUsage").get_to(r.memUsage);
    }
    if (!j.at("harddriveUsage").is_null()) {
        j.at("harddriveUsage").get_to(r.harddriveUsage);
    }
    if (!j.at("ssdUsage").is_null()) {
        j.at("ssdUsage").get_to(r.ssdUsage);
    }
    if (!j.at("imageFrameRateException").is_null()) {
        j.at("imageFrameRateException").get_to(r.imageFrameRateException);
    }
    if (!j.at("pcFrameRateException").is_null()) {
        j.at("pcFrameRateException").get_to(r.pcFrameRateException);
    }
    if (!j.at("canbusFrameRateException").is_null()) {
        j.at("canbusFrameRateException").get_to(r.canbusFrameRateException);
    }
    if (!j.at("insFrameRateException").is_null()) {
        j.at("insFrameRateException").get_to(r.insFrameRateException);
    }
}

void to_json(json& j, const CollectInfo& r) {
    j = json{
        {"vin", r.vin},
        {"taskId", r.taskId},
        {"distanceCollect", r.distanceCollect},
        {"durationCollect", r.durationCollect},
        {"triggercountCollect", r.triggercountCollect},
        {"bagcapacityCollect", r.bagcapacityCollect},
        {"distanceUpload", r.distanceUpload},
        {"durationUpload", r.durationUpload},
        {"triggercountUpload", r.triggercountUpload},
        {"bagcapacityUpload", r.bagcapacityUpload}
    };
}

void from_json(const json& j, CollectInfo& r) {
    if (!j.at("vin").is_null()) {
        j.at("vin").get_to(r.vin);
    }
    if (!j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.taskId);
    }
    if (!j.at("distanceCollect").is_null()) {
        j.at("distanceCollect").get_to(r.distanceCollect);
    }
    if (!j.at("durationCollect").is_null()) {
        j.at("durationCollect").get_to(r.durationCollect);
    }
    if (!j.at("triggercountCollect").is_null()) {
        j.at("triggercountCollect").get_to(r.triggercountCollect);
    }
    if (!j.at("bagcapacityCollect").is_null()) {
        j.at("bagcapacityCollect").get_to(r.bagcapacityCollect);
    }
    if (!j.at("distanceUpload").is_null()) {
        j.at("distanceUpload").get_to(r.distanceUpload);
    }
    if (!j.at("triggercountUpload").is_null()) {
        j.at("triggercountUpload").get_to(r.triggercountUpload);
    }
    if (!j.at("bagcapacityUpload").is_null()) {
        j.at("bagcapacityUpload").get_to(r.bagcapacityUpload);
    }
}

void to_json(json& j, const RtspInfo& r) {
    j = json{
        {"taskId", r.taskId},
        {"status", r.status},
        {"rtspUrl", r.rtspUrl}
    };
}

void from_json(const json& j, RtspInfo& r) {
    if (!j.at("taskId").is_null()) {
        j.at("taskId").get_to(r.taskId);
    }
    if (!j.at("status").is_null()) {
        j.at("status").get_to(r.status);
    }
    if (!j.at("rtspUrl").is_null()) {
        j.at("rtspUrl").get_to(r.rtspUrl);
    }
}

void to_json(json& j, const OBUStateInfo& r) {
    j = json{
        {"vin", r.vin},
        {"longitude", r.longitude},
        {"latitude", r.latitude},
        {"heading", 0.0},
        {"speed", r.speed},
        {"accspeed", 0.0},
        {"gpstime", r.gpsTime},
        {"altitude", r.altitude},
        {"starNm", 0},
        {"rtkState", "A"},
        {"light", r.light},
        {"brakeState", r.brakeState},
        {"gasState", 0},
        {"gearState", r.gearState},
        {"wheelAngle", r.wheelAngle},
        {"driveState","0"},
        {"abnormalState","0"},
        {"traffic_light",0},
        {"mileage","0"},
        {"engineSpeed","0"},
        {"chargeState",0},
        {"handbrake",0},
        {"startState",0},
        {"elecQuantity","0"},
        {"endurance",0},
        {"collectState", r.collectState}
    };
}

void from_json(const json& j, OBUStateInfo& r) {
    if (!j.at("vin").is_null()) {
        j.at("vin").get_to(r.vin);
    }
    if (!j.at("longitude").is_null()) {
        j.at("longitude").get_to(r.longitude);
    }
    if (!j.at("latitude").is_null()) {
        j.at("latitude").get_to(r.latitude);
    }
    if (!j.at("speed").is_null()) {
        j.at("speed").get_to(r.speed);
    }
    if (!j.at("gpstime").is_null()) {
        j.at("gpstime").get_to(r.gpsTime);
    }
    if (!j.at("altitude").is_null()) {
        j.at("altitude").get_to(r.altitude);
    }
    if (!j.at("light").is_null()) {
        j.at("light").get_to(r.light);
    }
    if (!j.at("brakeState").is_null()) {
        j.at("brakeState").get_to(r.brakeState);
    }
    if (!j.at("gearState").is_null()) {
        j.at("gearState").get_to(r.gearState);
    }
    if (!j.at("wheelAngle").is_null()) {
        j.at("wheelAngle").get_to(r.wheelAngle);
    }
    if (!j.at("collectState").is_null()) {
        j.at("collectState").get_to(r.collectState);
    }
}

} 
