# 数据闭环系统 - 完整代码流程分析

> 项目名称: senseauto_data_closed_loop
> 分析时间: 2026-04-07
> 代码行数: 254个文件, 主要为C++代码
> 框架: ADFramework, Cyber, FastRTPS, Protobuf

---

## 📋 目录

- [系统架构概览](#系统架构概览)
- [完整执行流程](#完整执行流程)
  - [阶段1: 系统启动与初始化](#阶段1-系统启动与初始化)
  - [阶段2: 组件初始化](#阶段2-组件初始化)
  - [阶段3: 数据采集流程](#阶段3-数据采集流程)
  - [阶段4: 数据记录流程](#阶段4-数据记录流程)
  - [阶段5: 脱敏处理流程](#阶段5-脱敏处理流程-异步后处理)
  - [阶段6: 数据上传流程](#阶段6-数据上传流程)
  - [阶段7: 日志上传流程](#阶段7-日志上传流程)
- [完整数据流转图](#完整数据流转图)
- [关键技术要点](#关键技术要点)
- [脱敏功能详解](#脱敏功能详解)

---

## 系统架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                    DataClosedLoopContext                     │
│                       (中央协调器)                            │
│                                                             │
│  管理所有子系统的生命周期和协调工作流程                        │
└─────────────────────────────────────────────────────────────┘
           │
           ├─── ChannelManager (通信层)
           │     └── RSCL通道管理和观察者模式
           │
           ├─── BusinessManager (业务逻辑层)
           │     ├── CacheRecordBusiness (缓存记录)
           │     ├── LoopRecordBusiness (循环记录)
           │     └── SustainBusiness (持续记录)
           │
           ├─── TriggerChecker/TriggerService (触发系统)
           │     └── 基于信号的数据采集触发器
           │
           ├─── DataDesenProcess (脱敏处理) ⭐
           │     └── AI驱动的隐私保护后处理
           │
           ├─── DataUploader (数据上传)
           │     └── 云端存储上传管理
           │
           ├─── LogManager (日志管理)
           │     └── 日志收集和上传
           │
           └─── DataProto (协议处理)
                 └── MQTT云端通信
```

### 核心模块说明

| 模块 | 文件位置 | 职责 |
|------|---------|------|
| **DataClosedLoopContext** | `src/DataClosedLoopContext.cc/.h` | 中央协调器，管理所有子系统 |
| **ChannelManager** | `src/channel/ChannelManager.cc/.h` | RSCL通道管理，实现观察者模式 |
| **BusinessManager** | `src/business/BusinessManager.cc/.h` | 业务逻辑编排，支持多种记录模式 |
| **DataDesenProcess** | `src/desen/DesenProcess.cpp/.h` | 脱敏处理，AI隐私保护 |
| **DataUploader** | `src/data_upload/DataUploader.cc/.h` | 数据上传，云存储集成 |
| **StrategyParser** | `src/strategy/StrategyParser.cc/.h` | 策略配置解析 |

---

## 完整执行流程

### 阶段1: 系统启动与初始化

**入口文件**: `src/main.cc`

```cpp
main() [main.cc:71-123]
│
├─ 1.1 初始化配置
│     └─ InitConfig()
│         └─ 读取 /opt/senseauto/senseauto_data_closed_loop/active/config/app_config.json
│             ├── 数据存储路径配置
│             ├── 上传配置
│             ├── 日志配置
│             └── 调试配置
│
├─ 1.2 设置日志环境变量
│     └─ SetDCLEnvironmentVariable()
│         └─ LOG_level, LOG_path, LOG_pattern, LOG_rotating_file等
│
├─ 1.3 初始化RSCL运行时
│     └─ senseAD::rscl::GetCurRuntime()->Init(argc, argv)
│         └── SenseTime Real-time Sensor Communication Layer
│
├─ 1.4 创建主组件
│     └─ DataClosedLoopContext e;
│         e.Initialize(cfg);
│         └── 定时器组件，1000ms间隔
│
└─ 1.5 注册信号处理器
      └─ signal(SIGTERM, SignalHandler)
          └── 优雅关闭系统
```

**代码片段** (`src/main.cc:71-104`):
```cpp
int main(int argc, char* argv[]) {
    printf("Init Appconfig\n");
    if (!InitConfig()) {
        printf("InitConfig failed!\n");
    }

    auto rt = senseAD::rscl::GetCurRuntime();
    printf("Init Runtime\n");
    rt->Init(argc, argv);

    printf("Init Component\n");
    senseAD::rscl::component::ComponentConfig cfg;
    cfg.timer_conf.set_name("Component_dcl");
    cfg.timer_conf.set_interval_ms(1000);

    dcl::DataClosedLoopContext e;
    g_data_closed_loop_context = &e;
    e.Initialize(cfg);

    struct sigaction sig_action;
    sig_action.sa_handler = SignalHandler;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = 0;
    sigaction(SIGTERM, &sig_action, nullptr);

    printf("Wait For Shutdown\n");
    while (!g_shutdown_requested && rt->OK()) {
        rt->WaitForShutdown(std::chrono::seconds(1));
    }

    return 0;
}
```

---

### 阶段2: 组件初始化

**入口文件**: `src/DataClosedLoopContext.cc:13-100`

```cpp
DataClosedLoopContext::OnInit()
│
├─ 2.1 InitLogger() - 初始化日志系统
│     └─ 配置日志级别、输出路径、滚动策略
│
├─ 2.2 InitConfig() - 读取应用配置
│     └─ 读取配置文件:
│         ├── app_config.json
│         ├── default_strategy_config.json
│         └── log_config.json
│
├─ 2.3 InitMemMount() - 挂载内存文件系统
│     └─ MemoryMount::Mount()
│         └── 创建 tmpfs 用于临时数据存储，提升性能
│
├─ 2.4 Selfcheck() - 系统自检
│     └─ 创建必要的存储目录:
│         ├── dataPath - 原始数据目录
│         ├── waitMaskingPath - 待脱敏目录
│         ├── MaskedPath - 已脱敏目录
│         ├── compressPath - 压缩后目录
│         └── uploadPath - 待上传目录
│
├─ 2.5 创建RSCL节点
│     └─ rt->CreateNode("node_dcl_" + getpid(), INTERNAL_NODE_SPACE)
│         └── 内部节点，用于组件间通信
│
├─ 2.6 InitStrategy() - 初始化策略配置
│     └─ StrategyParser::Parse()
│         └── 读取 default_strategy_config.json
│             ├── 触发条件定义
│             ├── DDS通道订阅配置
│             ├── 记录模式 (缓存/循环/持续)
│             └── 滚动删除策略
│
├─ 2.7 InitBusinessesManager() - 初始化业务管理器
│     └─ BusinessManager::Init()
│         └─ 根据策略创建业务实例:
│             ├── triggerMode == 0: CacheRecordBusiness (缓存模式)
│             │   └── 支持前向/后向缓存，触发时合并数据
│             ├── triggerMode == 1: LoopRecordBusiness (循环模式)
│             │   └── 持续记录，循环覆盖旧数据
│             └── triggerMode == 2: SustainBusiness (持续模式)
│                 └── 触发后持续记录一段时间
│
├─ 2.8 初始化触发系统 (二选一)
│     ├─ triggerVersion == 1: InitTriggerChecker()
│     │   └── TriggerChecker + RsclSignalFetcher
│     │       ├── 基于表达式的触发检查
│     │       └── 从RSCL获取信号值
│     │
│     └─ triggerVersion == 2: InitTriggerService()
│         └── TriggerService + TriggerClient
│             ├── 基于RSCL服务的触发系统
│           └── 支持周期性触发
│
├─ 2.9 InitDataCollecting() - 初始化数据采集
│     └─ ChannelManager::Init()
│         ├── InitChannels() - 创建RSCL通道
│         │   └── 为每个topic订阅传感器数据
│         │       ├── /sensor/camera/center_camera_fov120/encode
│         │       ├── /sensor/camera/left_front_camera/encode
│         │       └── ... (共11种摄像头)
│         │
│         └── InitObservers() - 注册观察者
│             └── Business实例订阅感兴趣的通道
│
├─ 2.10 InitDataProto() - 初始化MQTT协议
│      └─ DataProto::Init()
│          └── 连接到云端MQTT网关
│              ├── 订阅任务下发topic
│              ├── 上报数据状态
│              └── 处理云端指令
│
├─ 2.11 InitDataUploader() - 初始化数据上传
│      └─ DataUploader::Init()
│          ├── 创建上传工作线程
│          ├── 初始化CurlWrapper (HTTP客户端)
│          ├── 初始化FileStatusManager (状态持久化)
│          └── 初始化UploadQueue (线程安全队列)
│
├─ 2.12 InitDataDesen() - 初始化脱敏处理 ⭐
│      └─ DataDesenProcess::Init()
│          ├── 设置 waitMaskingPath (待脱敏目录)
│          ├── 设置 MaskedPath (已脱敏目录)
│          └── 创建11个DclDesen实例 (对应11种摄像头)
│              ├── center-camera-fov120: 3840x2160
│              ├── center-camera-fov30: 3840x2160
│              ├── left-front-camera: 1920x1280
│              └── ... (共11种)
│
└─ 2.13 Start() - 启动所有组件
      ├── data_desen_->Start()
      │   ├── 启动11个并行处理线程: DesenOneBag(sid)
      │   └── 启动目录监控线程: DirMonitor()
      ├── channel_manager_->StartCollecting()
      └── business_manager_->Start()
```

**关键代码片段** (`src/DataClosedLoopContext.cc:78-84`):
```cpp
// 初始化数据上传
if (!debug_config.closeDataUpload) {
    ok = InitDataUploader();
    CHECK_AND_RETURN(ok, Context, "InitDataUploader failed", false);

    // 初始化脱敏处理
    if (!debug_config.closeMasking) {
        ok = InitDataDesen();
        CHECK_AND_RETURN(ok, Context, "InitDataDesen failed", false);
    } else {
        AD_LWARN(Context) << "close mask!!";
    }
}
```

---

### 阶段3: 数据采集流程

**运行时主循环** - 实时数据采集

```cpp
传感器数据采集流程:
│
├─ 3.1 RSCL通道接收数据
│     └─ RsclChannel::Callback()
│         └── 收到传感器RawMessage
│             ├── 传感器数据
│             ├── 时间戳
│             └── 通道信息
│
├─ 3.2 通知所有观察者
│     └─ Channel::NotifyObservers(msg)
│         └── 遍历所有注册的观察者:
│             ├── TriggerChecker::Observe(msg)
│             │   └── 检查是否满足触发条件
│             └── Business::Observe(msg, sig)
│                 └── 业务观察者接收数据
│
├─ 3.3 RingBuffer缓存 (CacheRecordBusiness)
│     └─ Observe() [CacheRecordBusiness.cc:14-42]
│         │
│         ├─ ProcState::ForwardCaching (前向缓存)
│         │   └─ forward_ringbuffers_[topic]->push_back(std::make_pair(msg, timestamp))
│         │       └── 缓存触发后的数据
│         │
│         └─ ProcState::BackwardCaching (后向缓存)
│             └─ backward_ringbuffers_[topic]->push_back(std::make_pair(msg, timestamp))
│                 └── 缓存触发前的数据
│
├─ 3.4 触发器检测
│     └─ TriggerChecker::Check()
│         ├── 解析触发表达式
│         │   └── 示例: "speed > 60 && brake == true"
│         ├── 从RsclSignalFetcher获取信号值
│         │   └── 获取车辆速度、制动状态等
│         └── 判断是否满足触发条件
│             ├── 满足 → 触发业务逻辑
│             └── 不满足 → 继续缓存
│
└─ 3.5 触发业务逻辑
      └─ BusinessManager::SetBusinessTriggerState(triggerIds)
          └─ 对每个业务实例:
              └─ Business::ProcessBusinessState(BusinessState::Triggered)
                  └─ CacheRecordBusiness::AsyncProcessForTriggered()
                      └── 启动异步数据处理
```

**关键代码片段** (`src/business/CacheRecordBusiness.cc:14-42`):
```cpp
void CacheRecordBusiness::Observe(const std::string& topic,
                                  const std::shared_ptr<ReceivedMsg<senseAD::rscl::comm::RawMessage>>& msg,
                                  const dcl::any& sig) {
    auto now = common::GetCurrentTimestampNs();

    if (proc_state_.load() == ProcState::ForwardCaching) {
        // 前向缓存：触发后的数据
        forward_ringbuffers_.at(topic)->push_back(std::make_pair(msg, now));
        AD_LDEBUG(CacheRecordBusiness) << "topic: " << topic << ", forward caching";
    } else if (proc_state_.load() == ProcState::BackwardCaching) {
        // 后向缓存：触发前的数据
        backward_ringbuffers_.at(topic)->push_back(std::make_pair(msg, now));
        AD_LDEBUG(CacheRecordBusiness) << "topic: " << topic << ", backward caching";
    }
}
```

---

### 阶段4: 数据记录流程

**业务逻辑层** - 触发后的数据处理

```cpp
CacheRecordBusiness::Process() [触发后执行]
│
├─ 4.1 合并RingBuffer数据
│     └─ backward_buffer + forward_buffer
│         ├── 获取触发前的数据 (后向缓存)
│         └── 获取触发后的数据 (前向缓存)
│         └── 合并得到完整的事件数据
│
├─ 4.2 初始化Recorder
│     └─ recorder->Open(OPT_WRITE, file_path)
│         ├── 创建 .record 文件
│         └── 初始化ROS bag格式写入器
│
├─ 4.3 写入传感器数据
│     └─ for each msg in merged_buffer:
│         └─ recorder->Write(timestamp, data, size, channel_info)
│             ├── 写入时间戳
│             ├── 写入传感器数据
│             └── 写入通道描述信息
│
├─ 4.4 关闭Recorder
│     └─ recorder->Close()
│         ├── 刷新缓冲区
│         ├── 写入索引信息
│         └── 生成最终 .bag 文件
│
├─ 4.5 处理校准文件 (可选)
│     └─ ProcessCalibrationFiles()
│         └── 拷贝传感器校准数据
│
└─ 4.6 移动到待脱敏目录 ⭐
      └─ mv data_path/*.bag waitMaskingPath/
          └── 原始数据文件移动到待脱敏目录
              └── 触发脱敏处理流程
```

**数据文件命名规则**:
```
原始文件: {sensor_name}_{timestamp}_{random}.record
示例: center-camera-fov120_20250407_123456_abc123.record
```

---

### 阶段5: 脱敏处理流程 (异步后处理) ⭐

**脱敏模块** - AI驱动的隐私保护

#### 5.1 目录监控线程

```cpp
DataDesenProcess::DirMonitor() [目录监控线程]
│
├─ 5.1.1 监控待脱敏目录
│     └─ fs::directory_iterator(dir_wait_mask_)
│         └── 扫描 waitMaskingPath 目录
│             └── 查找新的 .bag 文件
│
├─ 5.1.2 按传感器类型分类文件
│     └─ FindFilesByKeys(dir_wait_mask_, k_sensor_names)
│         ├── k_sensor_names = {
│         │     "center-camera-fov120",
│         │     "center-camera-fov30",
│         │     "left-front-camera",
│         │     "left-rear-camera",
│         │     "right-front-camera",
│         │     "right-rear-camera",
│         │     "camera-rear-camera",
│         │     "front-camera-fov195",
│         │     "left-camera-fov195",
│         │     "rear-camera-fov195",
│         │     "right-camera-fov195"
│         │   }
│         └── wait_process_files_[sensor_name] = {file_paths}
│
├─ 5.1.3 唤醒处理线程
│     └─ wait_cv_.notify_one()
│         └── 通知对应的工作线程有新文件
│
└─ 5.1.4 定期清理旧文件
      └─ CleanOldFiles()
          └── 删除超过保留期的文件
```

#### 5.2 并行处理线程

```cpp
DataDesenProcess::DesenOneBag(uint32_t sid) [11个并行线程]
│
├─ 5.2.1 初始化脱敏引擎
│     └─ desen_map_[sid]->Init(width, height, quality)
│         ├── width: 3840 (4K摄像头) 或 1920 (1080p摄像头)
│         ├── height: 2160 (4K) 或 1280/1536 (1080p)
│         └── quality: 10 (脱敏质量参数)
│
├─ 5.2.2 等待文件到达
│     └─ while (wait_process_files_.find(k_sensor_names[sid]) == end):
│         └─ sleep(1 second)
│             └── 轮询检查是否有该传感器的文件
│
├─ 5.2.3 打开待脱敏文件
│     └─ BagReader reader(path)
│         ├── 读取ROS bag文件
│         └── 解析H.265编码的视频数据
│
├─ 5.2.4 创建输出文件
│     └─ RsclRecorder writer->Open(OPT_WRITE, dst_path)
│         └── dst_path = dir_masked_ / filename
│             └── 脱敏后的输出路径
│
└─ 5.2.5 逐帧处理 [DesenProcess.cpp:155-221]
      └─ while (reader.ReadNextMessage(&out)):
          │
          ├─ 5.2.5.1 解析视频帧
          │   └─ CameraEncodedStruct:
          │       ├── frame_type (I帧=2, P帧=1)
          │       ├── data (H.265编码数据)
          │       ├── timestamp (帧时间戳)
          │       └── frame_size (数据大小)
          │
          ├─ 5.2.5.2 跳过P帧，等待I帧
          │   └─ if (frame_type != 2): continue
          │       └── I帧是关键帧，包含完整图像
          │
          ├─ 5.2.5.3 执行脱敏处理 ⭐⭐⭐
          │   └─ desen_map_[sid]->Process(frame_bin, masked_frame, frame_type)
          │       │
          │       ├── [THOR SDK实现路径]
          │       │   └─ dcl_desen_impl_thor.hpp:
          │       │       │
          │       │       ├─ Step 1: H.265解码
          │       │       │   └─ NvMedia decoder
          │       │       │       ├── 输入: H.265编码帧 (frame_bin)
          │       │       │       ├── 处理: 硬件解码
          │       │       │       └── 输出: YUV帧 (NV12格式)
          │       │       │
          │       │       ├─ Step 2: 格式转换
          │       │       │   └─ CUDA converter (converter.cu)
          │       │       │       ├── 输入: YUV NV12
          │       │       │       ├── 处理: GPU色彩空间转换
          │       │       │       └── 输出: RGB/BGR格式
          │       │       │
          │       │       ├─ Step 3: AI推理
          │       │       │   └─ desentization_thor_aar64_v1.0.0.model
          │       │       │       ├── 模型类型: 目标检测神经网络
          │       │       │       ├── 检测目标:
          │       │       │       │   ├── 人脸 (face)
          │       │       │       │   ├── 车牌 (license plate)
          │       │       │       │   └── 其他敏感对象
          │       │       │       └── 输出: 检测框坐标 + 类别
          │       │       │
          │       │       ├─ Step 4: 隐私模糊处理
          │       │       │   └── 对检测区域进行处理
          │       │       │       ├── 人脸区域: 高斯模糊
          │       │       │       ├── 车牌区域: 遮罩/马赛克
          │       │       │       └── 其他区域: 可配置的模糊算法
          │       │       │
          │       │       ├─ Step 5: 格式转回
          │       │       │   └─ CUDA converter
          │       │       │       ├── 输入: RGB/BGR (已脱敏)
          │       │       │       ├── 处理: GPU色彩空间转换
          │       │       │       └── 输出: YUV NV12
          │       │       │
          │       │       └─ Step 6: H.265编码
          │       │           └─ NvMedia encoder
          │       │               ├── 输入: YUV帧 (已脱敏)
          │       │               ├── 处理: 硬件编码
          │       │               └── 输出: H.265编码帧 (masked_frame)
          │       │
          │       └── [空实现路径]
          │           └─ dcl_desen_impl_empty.hpp:
          │               └── 直接返回原始帧 (不脱敏)
          │                   └── 用于测试或无脱敏需求的场景
          │
          ├─ 5.2.5.4 构建脱敏后的消息
          │   └─ 创建新的 CameraEncodedStruct:
          │       ├── 复制时间戳 (timestamp)
          │       ├── 复制帧序号 (frame_id)
          │       ├── 设置 frame_type
          │       └── 替换 data 为 masked_frame
          │
          ├─ 5.2.5.5 写入输出文件
          │   └─ writer->Write(timestamp, serialized_data, size)
          │       └── 写入脱敏后的帧数据
          │
          └─ 5.2.5.6 控制处理频率
              └─ sleep(1000ms / freq)
                  └── freq 来自配置，控制处理速度
                      └── 避免占用过多GPU资源
```

#### 5.3 处理完成

```cpp
│
├─ 5.3.1 关闭输出文件
│     └─ writer->Close()
│         └── 生成脱敏后的 bag 文件
│
├─ 5.3.2 重命名文件 (去除 .00000.rsclbag 后缀)
│     └─ RenameRecordFile(dst_path + ".00000.rsclbag", renamed)
│         └── RSCL recorder自动添加后缀，需要重命名
│
├─ 5.3.3 加入上传队列 ⭐
│     └─ UploadQueue::GetInstance().Push({
│             path: renamed.string(),
│             type: ActivelyReport,
│             source: "desen_task"
│         })
│         └── 脱敏完成后自动加入上传队列
│
├─ 5.3.4 删除原始文件
│     └─ fs::remove(path)
│         └── 删除 waitMaskingPath 中的原始文件
│             └── 释放存储空间
│
└─ 5.3.5 继续处理下一个文件
      └─ return to step 5.2.2
```

**关键代码片段** (`src/desen/DesenProcess.cpp:176-220`):
```cpp
// 执行脱敏处理
desen_map_[sid]->Process(frame_bin, masked_frame, frame_type);

// 构建脱敏后的数据
::capnp::MallocMessageBuilder desen_builder;
auto desen_data = desen_builder.initRoot<senseAD::msg::std_msgs::CameraEncodedStruct>();

// 复制元数据
desen_data.setExposeLineTime(reader_data.getExposeLineTime());
desen_data.setFrameStartTime(reader_data.getFrameStartTime());
desen_data.setFrameTime(reader_data.getFrameTime());

// 替换视频数据
auto newData = desen_data.initData(masked_frame.size());
memcpy(newData.begin(), masked_frame.data(), masked_frame.size());
desen_data.setDataSize(newData.size());

// 序列化并写入
kj::Array<capnp::word> serialized = capnp::messageToFlatArray(desen_builder);
writer->Write(msg_timestamp, serialized.asChars().begin(), serialized.asChars().size());
```

---

### 阶段6: 数据上传流程

**数据上传模块** - 云端存储集成

```cpp
DataUploader::ProcessQueue() [上传工作线程]
│
├─ 6.1 从队列获取任务
│     └─ UploadQueue::Pop(task)
│         └── 线程安全队列，包含:
│             ├── path: 文件路径
│             ├── upload_type: ActivelyReport/PassivelyReport
│             └── source: 数据来源 (desen_task/manual)
│
├─ 6.2 读取文件状态
│     └─ FileStatusManager::GetFileStatus(path)
│         ├── 从本地数据库读取上传状态
│         ├── 如果已上传: 跳过
│         └── 如果未上传或失败: 继续
│
├─ 6.3 数据加密 (可选)
│     └─ DataEncryption::Encrypt(data)
│         ├── 生成随机AES密钥
│         ├── 使用RSA公钥加密AES密钥
│         ├── 使用AES加密文件数据
│         └── 返回加密后的数据
│
├─ 6.4 文件分割 (大文件)
│     └─ FileSplitter::Split(path, chunk_size)
│         ├── 检查文件大小
│         ├── 如果超过阈值: 分割为多个小块
│         │   └── 支持断点续传
│         └── 生成分片列表
│
├─ 6.5 HTTP上传到云端
│     └─ CurlWrapper::Upload(chunk, url, headers)
│         ├── 构造PUT请求
│         ├── 设置认证头
│         │   └── Authorization: Bearer {token}
│         ├── 上传到腾讯云COS
│         │   └── PUT /cos/{bucket}/{object_path}
│         └── 支持分片上传
│
├─ 6.6 更新上传状态
│     └─ FileStatusManager::UpdateStatus(path, UPLOADED)
│         ├── 持久化到本地数据库
│         └── 防止重复上传
│
├─ 6.7 上报数据状态 (通过MQTT)
│     └─ DataProto::ReportUploadStatus({
│             file_name: xxx,
│             status: UPLOADED,
│             upload_time: timestamp
│         })
│         └── 通知云端数据已上传
│
└─ 6.8 删除本地文件
      └─ fs::remove(path)
          └── 释放本地存储空间
```

**关键配置** (`config/app_config.json`):
```json
{
  "dataUpload": {
    "uploadPaths": {
      "uploadPath": "/data/dcl/upload"
    },
    "cosConfig": {
      "bucket": "dcl-data-prod",
      "region": "ap-guangzhou",
      "secretId": "***",
      "secretKey": "***"
    },
    "dataMask": {
      "freq": 10
    }
  }
}
```

---

### 阶段7: 日志上传流程

**日志管理模块** - 日志收集和上传

```cpp
LogManager::ProcessLogTasks()
│
├─ 7.1 收集日志文件
│     └─ LogFileProcess::CollectLogs()
│         └── 扫描 log_path 目录
│             ├── 应用日志 (*.log)
│             ├── 系统日志
│             └── 错误日志
│
├─ 7.2 压缩日志
│     └─ FileCompress::Compress(log_files)
│         ├── 使用tar打包
│         ├── 使用gzip压缩
│         └── 生成 .tgz 文件
│
├─ 7.3 上传日志
│     └─ 通过 DataUploader 或专门的日志上传接口
│         ├── 上传到日志bucket
│         └── 按日期组织目录结构
│
└─ 7.4 定期清理旧日志
      └─ PeriodicTask::Execute()
          ├── 检查日志保留策略
          ├── 删除超过保留期的日志
          └── 释放存储空间
```

---

## 完整数据流转图

```
┌─────────────┐
│  车辆传感器  │
│ (摄像头等)  │
└──────┬──────┘
       │ RSCL Real-time Sensor Communication Layer
       │ 实时传感器数据流
       ↓
┌──────────────────────────────────────────────────┐
│            ChannelManager                        │
│  ┌─────────────────────────────────────────┐     │
│  │ RsclChannel (订阅传感器topic)           │     │
│  │  - /sensor/camera/center_camera_fov120  │     │
│  │  - /sensor/camera/left_front_camera     │     │
│  │  - ... (11种摄像头)                     │     │
│  └─────────────────────────────────────────┘     │
└──────┬───────────────────────────────────────────┘
       │ NotifyObservers()
       │ 观察者模式分发数据
       ↓
┌──────────────────────────────────────────────────┐
│         观察者模式分发                           │
├──────────────────────────────────────────────────┤
│  TriggerChecker ←─ 检查触发条件                  │
│  Business ←─ 缓存数据到RingBuffer                │
└──────┬───────────────────────────────────────────┘
       │
       ↓ (触发条件满足)
┌──────────────────────────────────────────────────┐
│         BusinessManager                          │
│  ┌─────────────────────────────────────────┐     │
│  │ CacheRecordBusiness (缓存记录业务)      │     │
│  │  - 前向缓存 RingBuffer                  │     │
│  │  - 后向缓存 RingBuffer                  │     │
│  │  - 触发后合并数据                       │     │
│  └─────────────────────────────────────────┘     │
└──────┬───────────────────────────────────────────┘
       │ Record()
       │ 写入ROS bag文件
       ↓
┌───────────────────────────────────────────────────┐
│         RsclRecorder                              │
│  写入 .record 文件 (ROS bag格式)                  │
│  - 传感器原始数据                                 │
│  - 时间戳                                         │
│  - 元数据                                         │
└──────┬────────────────────────────────────────────┘
       │ mv to waitMaskingPath
       │ 移动到待脱敏目录
       ↓
┌───────────────────────────────────────────────────┐
│         DataDesenProcess ⭐ 脱敏处理               │
├───────────────────────────────────────────────────┤
│  DirMonitor: 监控 waitMaskingPath 目录            │
│    ↓ 发现新 .bag 文件                             │
│    ↓ 按传感器类型分类 (11种摄像头)                │
│    ↓                                              │
│  11个并行线程: DesenOneBag(sid)                   │
│    ├── 打开 BagReader (读取H.265视频)             │
│    ├── 逐帧读取 CameraEncodedStruct               │
│    ├── DclDesen::Process() ⭐⭐⭐                    │
│    │   ├── NvMedia Decoder (H.265→YUV)            │
│    │   ├── CUDA Converter (YUV→RGB)               │
│    │   ├── AI Model (人脸/车牌检测)               │
│    │   │   └── desentization_thor_aar64_v1.0.0.model│
│    │   ├── 隐私模糊 (高斯模糊/遮罩)               │
│    │   ├── CUDA Converter (RGB→YUV)               │
│    │   └── NvMedia Encoder (YUV→H.265)            │
│    ├── 写入 RsclRecorder                          │
│    └── 生成脱敏后的 .bag 文件                     │
└──────┬────────────────────────────────────────────┘
       │ mv to MaskedPath
       │ 移动到已脱敏目录
       ↓ Push to UploadQueue
┌──────────────────────────────────────────────────┐
│         DataUploader                             │
│  ┌─────────────────────────────────────────┐     │
│  │ FileStatusManager (状态管理)            │     │
│  │ DataEncryption (RSA+AES加密)            │     │
│  │ FileSplitter (大文件分割)               │     │
│  │ CurlWrapper (HTTP上传)                  │     │
│  └─────────────────────────────────────────┘     │
└──────┬───────────────────────────────────────────┘
       │ PUT to Cloud COS
       │ 上传到云端对象存储
       ↓
┌───────────────────────────────────────────────────┐
│         云端存储 (Tencent COS)                    │
│  Bucket: dcl-data-prod                            │
│  Region: ap-guangzhou                             │
└───────────────────────────────────────────────────┘
```

---

## 关键技术要点

### 1. 异步解耦架构

**设计理念**: 脱敏是计算密集型操作，设计为独立的后处理流程

**优势**:
- ✅ 不阻塞实时数据采集
- ✅ 充分利用硬件并行能力
- ✅ 提高系统吞吐量
- ✅ 故障隔离，互不影响

### 2. 并行处理

**实现**: 11个线程对应11种摄像头

```cpp
// 启动11个并行处理线程
for (int i = 0; i < 11; ++i) {
    desen_map_[i] = std::make_unique<DclDesen>();
    work_pool_.push_back(std::thread(&DataDesenProcess::DesenOneBag, this, i));
}
```

**摄像头类型**:
| ID | 传感器名称 | 分辨率 | 用途 |
|----|-----------|--------|------|
| 0 | center-camera-fov120 | 3840x2160 | 前视广角 |
| 1 | center-camera-fov30 | 3840x2160 | 前视窄角 |
| 2 | left-front-camera | 1920x1280 | 左前 |
| 3 | left-rear-camera | 1920x1280 | 左后 |
| 4 | right-front-camera | 1920x1280 | 右前 |
| 5 | right-rear-camera | 1920x1280 | 右后 |
| 6 | camera-rear-camera | 1920x1280 | 后视 |
| 7-10 | *-camera-fov195 | 1920x1536 | 环视 |

### 3. 硬件加速

**NVIDIA NvMedia**: 用于视频编解码
```cpp
// 硬件解码器
NvMediaVideoDecoder* decoder_;
// 硬件编码器
NvMediaVideoEncoder* encoder_;
```

**CUDA加速**: 用于图像格式转换
```cuda
// CUDA kernel (converter.cu)
__global__ void ConvertYUVToRGB(uint8_t* yuv, uint8_t* rgb, int width, int height);
```

### 4. AI驱动脱敏

**模型**: `desentization_thor_aar64_v1.0.0.model`
- 类型: 深度学习目标检测模型
- 输入: RGB图像帧
- 输出: 检测框 + 类别 (人脸、车牌等)
- 推理框架: SenseTime THOR SDK

### 5. 队列管理

**目录监控 + 工作队列**:
```cpp
// 目录监控线程
void DirMonitor() {
    while (is_run_) {
        // 扫描新文件
        auto new_files = GetFilesInWaitDir();
        // 分类并加入队列
        wait_process_files_[sensor] = files;
        // 唤醒工作线程
        wait_cv_.notify_one();
    }
}

// 工作线程
void DesenOneBag(uint32_t sid) {
    while (is_run_) {
        // 等待任务
        std::unique_lock<std::mutex> lock(mtx_);
        wait_cv_.wait(lock, [&] {
            return wait_process_files_.count(k_sensor_names[sid]) > 0;
        });
        // 处理文件...
    }
}
```

### 6. 容错机制

**处理中断恢复**:
```cpp
if (is_run_) {
    // 正常处理
} else {
    // 停止信号，保留.active文件
    AD_LWARN(DataDesenProcess) << "stop while process, will desen again";
    return;
}
```

**特点**:
- 保留未完成的文件
- 下次启动时重新处理
- 确保数据不丢失

---

## 脱敏功能详解

### 为什么需要脱敏？

1. **法律合规**: 满足《个人信息保护法》等法规要求
2. **隐私保护**: 保护行人、乘客的面部和车牌信息
3. **数据价值**: 脱敏后的数据可用于算法训练
4. **防止泄露**: 避免敏感信息在传输/存储过程中泄露

### 脱敏在流程中的位置

```
数据采集 → [实时记录] → 文件完成 → [脱敏处理] → 加密上传
                       ↑                    ↑
                   原始数据              脱敏数据
                (waitMaskingPath)     (MaskedPath)
```

**关键特性**:
- 🔄 **异步处理**: 不影响实时采集
- 🚀 **并行加速**: 11个线程同时处理
- 🎯 **AI驱动**: 自动检测敏感区域
- 🔒 **安全可靠**: 硬件加速，性能稳定

### 脱敏处理流程图

```
┌─────────────────────────────────────────────────────┐
│  输入: H.265编码的视频帧 (原始数据)                  │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 1: H.265 解码                                │
│  ├─ NvMedia 硬件解码器                              │
│  ├─ 输入: H.265 bitstream                          │
│  └─ 输出: YUV NV12 帧                               │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 2: 格式转换 (YUV → RGB)                       │
│  ├─ CUDA kernel 加速                                │
│  ├─ GPU 并行处理                                    │
│  └─ 输出: RGB/BGR 格式                              │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 3: AI 目标检测                                │
│  ├─ 加载 THOR SDK                                   │
│  ├─ 输入: RGB 图像                                  │
│  ├─ 模型: desentization_thor_aar64_v1.0.0.model     │
│  ├─ 检测: 人脸、车牌等敏感目标                       │
│  └─ 输出: 检测框列表 (坐标+类别)                     │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 4: 隐私模糊处理                               │
│  ├─ 遍历检测框                                      │
│  ├─ 人脸区域: 高斯模糊                              │
│  ├─ 车牌区域: 马赛克/遮罩                           │
│  └─ 其他区域: 可配置处理方式                         │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 5: 格式转回 (RGB → YUV)                       │
│  ├─ CUDA kernel 加速                                │
│  └─ 输出: YUV NV12 格式 (已脱敏)                     │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  Step 6: H.265 编码                                 │
│  ├─ NvMedia 硬件编码器                              │
│  ├─ 输入: YUV 帧 (已脱敏)                           │
│  └─ 输出: H.265 bitstream                           │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│  输出: H.265编码的视频帧 (脱敏后数据)                │
└─────────────────────────────────────────────────────┘
```

### 性能参数

| 参数 | 值 | 说明 |
|-----|-----|------|
| 处理频率 | 10 fps | 可配置，默认每秒10帧 |
| 并行线程 | 11 | 对应11种摄像头 |
| 分辨率支持 | 1920x1280, 3840x2160 | 根据摄像头类型 |
| 编解码 | H.265 | 硬件加速 |
| GPU加速 | CUDA | NVIDIA平台 |

---

## 总结

SenseAuto 数据闭环系统是一个高度模块化、异步解耦的自动驾驶数据采集平台：

### 核心优势

1. **实时性**: 异步脱敏不阻塞数据采集
2. **高性能**: 硬件加速 + 并行处理
3. **智能化**: AI驱动的隐私保护
4. **可靠性**: 完善的容错和恢复机制
5. **可扩展**: 模块化设计，易于扩展

### 技术栈

- **语言**: C++17
- **框架**: ADFramework, Cyber, RSCL
- **硬件加速**: NVIDIA NvMedia, CUDA
- **AI模型**: SenseTime THOR SDK
- **通信**: RSCL, MQTT
- **存储**: ROS bag格式

### 关键流程

```
传感器采集 → 实时记录 → 异步脱敏 → 加密上传 → 云端存储
```
