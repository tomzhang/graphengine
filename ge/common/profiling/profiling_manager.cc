/**
 * Copyright 2019-2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common/profiling/profiling_manager.h"

#include "framework/common/debug/ge_log.h"
#include "framework/common/debug/log.h"
#include "framework/common/string_util.h"
#include "graph/ge_context.h"
#include "runtime/base.h"

namespace {
const char *const kJobID = "jobID";
const char *const kDeviceID = "deviceID";
const char *const kStartCfg = "startCfg";
const char *const kFeatures = "features";
const char *const kConf = "conf";
const char *const kEvents = "events";
const char *const kAiCoreEvents = "ai_core_events";
const char *const kName = "name";
const char *const kTraceID = "traceId";
const char *const kProfDir = "resultPath";
const size_t kReportMaxLen = 2048;
const int32_t kMaxDeviceNum = 256;
const std::string kConfigNumsdev = "devNums";
const std::string kConfigDevIdList = "devIdList";
const std::string kProfStart = "prof_start";
const std::string kProfStop = "prof_stop";
}  // namespace

namespace ge {
ProfilingManager::ProfilingManager() {}

ProfilingManager::~ProfilingManager() {}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ProfilingManager &ProfilingManager::Instance() {
  static ProfilingManager profiling_manager;
  return profiling_manager;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ge::Status ProfilingManager::Init(const Options &options) {
#ifdef DAVINCI_SUPPORT_PROFILING
  vector<int32_t>().swap(device_id_);
  job_id_ = options.job_id;

  GELOGI("ProfilingManager::Init  job_id:%s", job_id_.c_str());

  Status ret;
  if (!recv_profiling_config_.empty()) {
    GELOGI("Profiling json config from acl:%s", recv_profiling_config_.c_str());
    ret = InitFromAclCfg(recv_profiling_config_);
  } else {
    ret = InitFromOptions(options);
    if (ret == SUCCESS && is_load_profiling_) {
      device_id_.push_back(options.device_id);
    }
  }
  if (ret != SUCCESS) {
    GELOGE(ret, "Failed to init profiling.");
    return ret;
  }

  if (is_load_profiling_) {
    // register Framework to profiling
    int result = Msprof::Engine::Init(GE_PROFILING_MODULE, &engine_);
    if (result != 0) {
      GELOGE(FAILED, "Register profiling engine failed.");
      return FAILED;
    }
    // profiling startup first time
    GELOGI("Begin to init profiling, device num %zu", device_id_.size());
    for (size_t i = 0; i < device_id_.size(); ++i) {
      ret = StartProfiling(0, device_id_[i]);
      if (ret != SUCCESS) {
        GELOGW("Profiling start failed on device %d.", device_id_[i]);
        continue;
      }
      GELOGI("Profiling init succ on device %d.", device_id_[i]);
    }
  } else {
    GELOGI("The profiling is off, skip the initialization");
  }
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ge::Status ProfilingManager::InitFromAclCfg(
  const std::string &config) {
#ifdef DAVINCI_SUPPORT_PROFILING
  try {
    is_load_profiling_ = false;
    is_execute_profiling_ = false;
    profiling_opts_.clear();
    op_trace_conf_.clear();
    Json start_prof_conf = Json::parse(config);
    Json &prof_conf = start_prof_conf[kStartCfg][0];
    job_id_ = prof_conf[kJobID];
    auto iter = prof_conf.find(kProfDir);
    if (iter != prof_conf.end()) {
      prof_dir_ = prof_conf[kProfDir];
    }
    Json &device_id = prof_conf[kDeviceID];
    if (device_id.size() != 0) {
      vector<int32_t>().swap(device_id_);
      bool is_all = false;
      for (size_t i = 0; i < device_id.size(); i++) {
        std::string device_id_str = device_id[i].get<std::string>();
        if (device_id_str == "all") {
          is_all = true;
          break;
        }
        device_id_.push_back(std::stoi(device_id_str));
      }
      if (is_all) {
        int32_t count = 0;
        rtError_t rt_err = rtGetDeviceCount(&count);
        if (rt_err != RT_ERROR_NONE) {
          GELOGE(FAILED, "Call rtGetDeviceCount to get device failed.");
        }

        vector<int32_t>().swap(device_id_);
        for (int32_t i = 0; i < count; ++i) {
          device_id_.push_back(i);
        }
      }
    }

    Json &features = prof_conf[kFeatures];
    if (ParseFeaturesFromAclCfg(features) != SUCCESS) {
      GELOGE(FAILED, "Parse feature from acl cfg failed.");
      return FAILED;
    }
    is_load_profiling_ = true;
    is_execute_profiling_ = true;
  } catch (...) {
    GELOGE(FAILED, "Json conf is not invalid !");
    return ge::PARAM_INVALID;
  }
#endif
  return ge::SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ge::Status ProfilingManager::ParseFeaturesFromAclCfg(
  const Json &features) {
#ifdef DAVINCI_SUPPORT_PROFILING
  try {
    for (size_t i = 0; i < features.size(); ++i) {
      const Json &feature = features[i];
      if ((feature.find(kName) == feature.end()) || feature[kName].is_null()) {
        continue;
      }
      const std::string &name = feature[kName];
      if (name == "op_trace") {
        const Json &conf = feature[kConf];
        const Json &events = conf[0][kEvents];
        const std::string &ai_core_events = events[0][kAiCoreEvents];
        GELOGI("Op trace config from acl ai_core_events:%s", ai_core_events.c_str());
        is_op_trace_ = true;
        ProfMgrConf prof_mgr_conf;
        int result = ProfMgrGetConf(ai_core_events, &prof_mgr_conf);
        if (result != 0) {
          GELOGE(FAILED, "ProfMgrGetConf failed.");
          return FAILED;
        }
        op_trace_conf_ = prof_mgr_conf.conf;
        op_trace_iter_num_ = static_cast<int32_t>(op_trace_conf_.size());
        GELOGI("Op trace profiling iter num %d,", op_trace_iter_num_);
      } else if (name == "task_trace") {
        is_op_trace_ = false;
        if (feature.find(kConf) != feature.end()) {
          const Json &conf = feature[kConf];
          std::stringstream task_trace_conf;
          task_trace_conf << conf;
          task_trace_conf_ = task_trace_conf.str();
        }
        GELOGI("Task trace config from acl");
      } else if (name == "system_trace") {
        is_op_trace_ = false;
        const Json &conf = feature[kConf];
        std::stringstream system_trace_conf;
        system_trace_conf << conf;
        system_trace_conf_ = system_trace_conf.str();
        GELOGI("System trace config from acl");
      }
      profiling_opts_.push_back(name);
    }
  } catch (...) {
    GELOGE(ge::PARAM_INVALID, "Json conf feature is not invalid !");
    return ge::PARAM_INVALID;
  }
#endif
  return ge::SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ge::Status ProfilingManager::InitFromOptions(const Options &options) {
#ifdef DAVINCI_SUPPORT_PROFILING
  // enable profiling support two ways: env and front end
  const char *profiling_mode = std::getenv("PROFILING_MODE");
  const char *prof_options = std::getenv("PROFILING_OPTIONS");
  if ((profiling_mode == nullptr) || (strcmp("true", profiling_mode) != 0) || (prof_options == nullptr)) {
    is_load_profiling_ = false;
    is_execute_profiling_ = false;
  } else {
    std::string prof_options_str = std::string(prof_options);
    profiling_opts_ = StringUtils::Split(prof_options_str, ':');
    is_load_profiling_ = true;
    is_execute_profiling_ = true;
    GELOGI("The profiling in env is %s, %s", profiling_mode, prof_options);
  }
  if (!is_load_profiling_) {
    const std::string enable_profiling = "1";
    if (options.profiling_mode != enable_profiling || options.profiling_options.empty()) {
      is_load_profiling_ = false;
      is_execute_profiling_ = false;
      return SUCCESS;
    } else {
      profiling_opts_ = StringUtils::Split(options.profiling_options, ':');
      is_load_profiling_ = true;
      is_execute_profiling_ = true;
      GELOGI("The profiling in options is %s, %s", options.profiling_mode.c_str(), options.profiling_options.c_str());
    }
  }
  // features:'training_trace', 'task_trace' or 'op_trace'  etc
  if (!profiling_opts_.empty()) {
    if (profiling_opts_[0] == "op_trace") {
      is_op_trace_ = true;
      // op trace get conf
      ProfMgrConf prof_mgr_conf;
      int result = ProfMgrGetConf("", &prof_mgr_conf);
      if (result != 0) {
        GELOGE(FAILED, "ProfMgrGetConf failed.");
        return FAILED;
      }
      op_trace_conf_ = prof_mgr_conf.conf;
      op_trace_iter_num_ = static_cast<int32_t>(op_trace_conf_.size());
      GELOGI("op trace profiling iter num %d,", op_trace_iter_num_);
    } else {
      is_op_trace_ = false;
      op_trace_iter_num_ = 1;
    }
  }
#endif
  return ge::SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY ge::Status ProfilingManager::StartProfiling(int32_t iter_num,
                                                                                             int32_t device_id) {
#ifdef DAVINCI_SUPPORT_PROFILING
  if (!profiling_opts_.empty()) {
    GELOGI("Start profiling index is %d", iter_num);
    // current one docker only use one device
    Json p_device;

    try {
      // profiling need physical_device_id
      p_device[kDeviceID] = std::to_string(device_id);
      p_device[kJobID] = job_id_;
      p_device[kTraceID] = std::to_string(GetContext().TraceId());
      if (!prof_dir_.empty()) {
        p_device[kProfDir] = prof_dir_;
        GELOGI("Prof dir: %s.", prof_dir_.c_str());
      }

      Json features;
      if (is_op_trace_) {
        Json f;
        f[kName] = "op_trace";
        Json conf;
        if (op_trace_conf_.size() <= static_cast<size_t>(iter_num)) {
          GELOGE(FAILED, "Op trace iter num is invalid!");
          return FAILED;
        }
        Json events;
        events[0] = nlohmann::json::parse(op_trace_conf_[iter_num]);
        conf[0][kEvents] = events;
        f[kConf] = conf;
        features[0] = f;
        if (iter_num == 0) {
          is_load_ = true;
        }
      } else {
        for (std::vector<std::string>::size_type i = 0; i < profiling_opts_.size(); i++) {
          Json f;
          if (profiling_opts_[i] == "system_trace") {
            f[kConf] = nlohmann::json::parse(system_trace_conf_);
          } else if (profiling_opts_[i] == "task_trace") {
            if (!task_trace_conf_.empty()) {
              f[kConf] = nlohmann::json::parse(task_trace_conf_);
            }
          }
          f[kName] = profiling_opts_[i];
          features[i] = f;
        }
        is_load_ = true;
      }
      p_device[kFeatures] = features;
      // only one device, but sProfMgrStartUp API require for device list
      Json devices;
      devices[0] = p_device;

      Json start_cfg;
      start_cfg[kStartCfg] = devices;

      // convert json to string
      std::stringstream ss;
      ss << start_cfg;
      send_profiling_config_ = ss.str();
      GELOGI("Profiling config %s\n", send_profiling_config_.c_str());
    } catch (...) {
      GELOGE(FAILED, "Op trace json conf is not invalid !");
      return FAILED;
    }

    // runtime startup for profiling
    uint64_t module = GetProfilingModule();
    int32_t device_num = 1;
    uint32_t device_id_rt = static_cast<uint32_t>(device_id);
    GE_CHK_RT_RET(rtProfilerStart(module, device_num, &device_id_rt));

    // call profiling startup API
    ProfMgrCfg prof_cfg = {send_profiling_config_};
    void *prof_handle = ProfMgrStartUp(&prof_cfg);
    if (prof_handle == nullptr) {
      GELOGW("ProfMgrStartUp failed on device %d ", device_id);
      return FAILED;
    }
    GELOGD("StartProfiling, prof_handle: %p", prof_handle);
    prof_handle_vec_.push_back(prof_handle);
  }
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::StopProfiling() {
#ifdef DAVINCI_SUPPORT_PROFILING
  Msprof::Engine::Reporter *reporter = PluginImpl::GetPluginReporter();
  if (reporter != nullptr) {
    int ret = reporter->Flush();
    GELOGI("Report data end, ret is %d", ret);
  }
  uint64_t module = GetProfilingModule();
  int32_t device_num = static_cast<int32_t>(device_id_.size());
  uint32_t *device_id_ptr = new (std::nothrow) uint32_t[device_num];
  if (device_id_ptr == nullptr) {
    GELOGE(FAILED, "Stop profiling device id ptr is null.");
    return;
  }
  for (int32_t i = 0; i < device_num; i++) {
    device_id_ptr[i] = static_cast<uint32_t>(device_id_[i]);
  }
  rtError_t rt_ret = rtProfilerStop(module, device_num, device_id_ptr);
  if (rt_ret != RT_ERROR_NONE) {
    GELOGW("Call rtProfilerStop failed, ret:%d", rt_ret);
  }
  delete[] device_id_ptr;
  device_id_ptr = nullptr;

  for (size_t i = 0; i < prof_handle_vec_.size(); ++i) {
    int result = ProfMgrStop(prof_handle_vec_[i]);
    if (result != 0) {
      GELOGW("ProfMgr stop return fail:%d, handle:%p", result, prof_handle_vec_[i]);
    }
  }
  vector<void *>().swap(prof_handle_vec_);
  is_load_ = false;
  recv_profiling_config_ = "";
  GELOGI("Stop Profiling success.");
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::ProfilingTaskDescInfo(
  const std::vector<TaskDescInfo> &task_desc_info, const int32_t &device_id) {
#ifdef DAVINCI_SUPPORT_PROFILING
  Msprof::Engine::Reporter *reporter = PluginImpl::GetPluginReporter();
  if (reporter == nullptr) {
    GELOGI("Profiling report is nullptr!");
    return;
  }

  std::string data;
  for (const auto &task : task_desc_info) {
    std::string model_name = task.model_name;
    std::string op_name = task.op_name;
    uint32_t block_dim = task.block_dim;
    uint32_t task_id = task.task_id;
    uint32_t stream_id = task.stream_id;
    data = model_name.append(" ").append(op_name).append(" ").append(std::to_string(block_dim)
                                                                       .append(" ")
                                                                       .append(std::to_string(task_id))
                                                                       .append(" ")
                                                                       .append(std::to_string(stream_id))
                                                                       .append("\n"));

    Msprof::Engine::ReporterData reporter_data{};
    reporter_data.deviceId = device_id;
    reporter_data.data = (unsigned char *)data.c_str();
    reporter_data.dataLen = data.size();
    int ret = memcpy_s(reporter_data.tag, MSPROF_ENGINE_MAX_TAG_LEN + 1, "task_desc_info", sizeof("task_desc_info"));
    if (ret != EOK) {
      GELOGE(ret, "Report data tag of task_desc_info memcpy error!");
      return;
    }

    ret = reporter->Report(&reporter_data);
    if (ret != SUCCESS) {
      GELOGE(ret, "Reporter data of task_desc_info fail!");
      return;
    }
  }

  data.clear();
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::ProfilingGraphDescInfo(
  const std::vector<ComputeGraphDescInfo> &compute_graph_desc_info, const int32_t &device_id) {
#ifdef DAVINCI_SUPPORT_PROFILING
  Msprof::Engine::Reporter *reporter = PluginImpl::GetPluginReporter();
  GE_IF_BOOL_EXEC(reporter == nullptr, GELOGI("Profiling report is nullptr!"); return;);

  std::string data;
  for (const auto &graph : compute_graph_desc_info) {
    data.append("model_name:")
      .append(graph.model_name)
      .append(" op_name:")
      .append(graph.op_name)
      .append(" op_type:")
      .append(graph.op_type);
    for (size_t i = 0; i < graph.input_format.size(); ++i) {
      data.append(" input_id:")
        .append(std::to_string(i))
        .append(" input_format:")
        .append(std::to_string(graph.input_format.at(i)))
        .append(" input_data_type:")
        .append(std::to_string(graph.input_data_type.at(i)))
        .append(" input_shape:\"");
      size_t input_shape_len = graph.input_shape.at(i).size();
      if (input_shape_len == 0) {
        data.append("");
      } else if (input_shape_len == 1) {
        data.append(std::to_string(graph.input_shape.at(i).at(0)));
      } else {
        for (size_t j = 0; j < input_shape_len - 1; ++j) {
          data.append(std::to_string(graph.input_shape.at(i).at(j))).append(",");
        }
        data.append(std::to_string(graph.input_shape.at(i).at(input_shape_len - 1)));
      }

      data.append("\"");
    }

    for (size_t i = 0; i < graph.output_format.size(); ++i) {
      data.append(" output_id:")
        .append(std::to_string(i))
        .append(" output_format:")
        .append(std::to_string(graph.output_format.at(i)))
        .append(" output_data_type:")
        .append(std::to_string(graph.output_data_type.at(i)))
        .append(" output_shape:\"");
      size_t output_shape_len = graph.output_shape.at(i).size();
      if (output_shape_len == 0) {
        data.append("");
      } else if (output_shape_len == 1) {
        data.append(std::to_string(graph.output_shape.at(i).at(0)));
      } else {
        for (size_t j = 0; j < output_shape_len - 1; ++j) {
          data.append(std::to_string(graph.output_shape.at(i).at(j))).append(",");
        }
        data.append(std::to_string(graph.output_shape.at(i).at(output_shape_len - 1)));
      }
      data.append("\"");
    }

    data.append("\n");

    Msprof::Engine::ReporterData reporter_data{};
    Report(device_id, data, *reporter, reporter_data);

    data.clear();
  }
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::Report(
  const int32_t &device_id, const string &data, Msprof::Engine::Reporter &reporter,
  Msprof::Engine::ReporterData &reporter_data) {
#ifdef DAVINCI_SUPPORT_PROFILING
  size_t index = data.size() / kReportMaxLen;
  if (index >= 1) {
    reporter_data.deviceId = device_id;
    int ret = memcpy_s(reporter_data.tag, MSPROF_ENGINE_MAX_TAG_LEN + 1, "graph_desc_info", sizeof("graph_desc_info"));
    GE_IF_BOOL_EXEC(ret != EOK, GELOGE(ret, "Report data tag of graph_desc_info memcpy error!"); return;);
    for (size_t i = 0; i < index; ++i) {
      reporter_data.data = (unsigned char *)data.c_str() + kReportMaxLen * i;
      reporter_data.dataLen = kReportMaxLen;
      ret = reporter.Report(&reporter_data);
      GE_IF_BOOL_EXEC(ret != SUCCESS, GELOGE(ret, "Reporter data of graph_desc_info fail!"); return;);
    }
    reporter_data.dataLen = data.size() - kReportMaxLen * index;
    if (reporter_data.dataLen != 0) {
      reporter_data.data = (unsigned char *)data.c_str() + kReportMaxLen * index;
      ret = reporter.Report(&reporter_data);
      GE_IF_BOOL_EXEC(ret != SUCCESS, GELOGE(ret, "Reporter data of graph_desc_info fail!"); return;);
    }
  } else {
    reporter_data.deviceId = device_id;
    reporter_data.data = (unsigned char *)data.c_str();
    reporter_data.dataLen = data.size();
    int ret = memcpy_s(reporter_data.tag, MSPROF_ENGINE_MAX_TAG_LEN + 1, "graph_desc_info", sizeof("graph_desc_info"));
    GE_IF_BOOL_EXEC(ret != EOK, GELOGE(ret, "Report data tag of graph_desc_info memcpy error!"); return;);

    ret = reporter.Report(&reporter_data);
    GE_IF_BOOL_EXEC(ret != SUCCESS, GELOGE(ret, "Reporter data of graph_desc_info fail!"); return;);
  }
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::PluginUnInit(const std::string &module) const {
#ifdef DAVINCI_SUPPORT_PROFILING
  int ret = Msprof::Engine::UnInit(module);
  if (ret != SUCCESS) {
    GELOGE(ret, "profiling plugin uninit failed, ret:%d", ret);
  }
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::ReportProfilingData(
  const std::vector<TaskDescInfo> &task_desc_info, const std::vector<ComputeGraphDescInfo> &compute_graph_desc_info) {
#ifdef DAVINCI_SUPPORT_PROFILING
  int32_t logic_device_id = 0;
  rtError_t rt_ret = rtGetDevice(&logic_device_id);
  if (rt_ret != RT_ERROR_NONE) {
    GELOGE(rt_ret, "runtime get logic_device_id failed, current logic_device_id:%d", logic_device_id);
    return;
  }
  GELOGI("current logic_device_id:%d", logic_device_id);
  if (!is_acl_api_mode_) {
    auto ret = std::find(device_id_.begin(), device_id_.end(), logic_device_id);
    if (ret == device_id_.end()) {
      GELOGE(FAILED, "get valid phy_device_id failed, profiling report failed.");
      return;
    }
  }
  GELOGI("start ProfilingTaskDescInfo.");
  ProfilingTaskDescInfo(task_desc_info, logic_device_id);
  GELOGI("start ProfilingGraphDescInfo.");
  ProfilingGraphDescInfo(compute_graph_desc_info, logic_device_id);
  GELOGI("Report profiling data for GE end.");
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::SetProfilingConfig(
  const std::string &profiling_cfg) {
  recv_profiling_config_ = profiling_cfg;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY uint64_t ProfilingManager::GetProfilingModule() {
  uint64_t module = PROF_MODEL_EXECUTE_MASK | PROF_RUNTIME_API_MASK | PROF_RUNTIME_TRACE_MASK |
                    PROF_SCHEDULE_TIMELINE_MASK | PROF_SCHEDULE_TRACE_MASK | PROF_TASK_TIME_MASK |
                    PROF_SUBTASK_TIME_MASK | PROF_AICPU_TRACE_MASK | PROF_AICORE_METRICS_MASK |
                    PROF_AIVECTORCORE_METRICS_MASK | PROF_MODEL_LOAD_MASK;
  return module;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY Status ProfilingManager::ProfInit(uint64_t module) {
#ifdef DAVINCI_SUPPORT_PROFILING
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t model_load_mask = module & PROF_MODEL_LOAD_MASK;

  if (model_load_mask == PROF_MODEL_LOAD_MASK) {
    // register Framework to profiling
    int32_t result = Msprof::Engine::Init(GE_PROFILING_MODULE, &engine_);
    if (result != SUCCESS) {
      GELOGE(FAILED, "Register profiling engine failed.");
      return FAILED;
    }
    int32_t device_num = -1;
    rtError_t rt_ret = rtProfilerStart(model_load_mask, device_num, nullptr);
    if (rt_ret != RT_ERROR_NONE) {
      GELOGE(FAILED, "Runtime profiler start failed.");
      return FAILED;
    }
    is_load_profiling_ = true;
    GELOGI("Prof init: model load profiling on.");
  }

  uint64_t training_trace_mask = module & PROF_TRAINING_TRACE_MASK;
  if (training_trace_mask == PROF_TRAINING_TRACE_MASK) {
    is_training_trace_ = true;
  }
  is_acl_api_mode_ = true;
  GELOGI("Prof init success.");
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY Status ProfilingManager::ProfFinalize() {
#ifdef DAVINCI_SUPPORT_PROFILING
  std::lock_guard<std::mutex> lock(mutex_);
  is_load_profiling_ = false;
  is_training_trace_ = false;
  is_acl_api_mode_ = false;

  int32_t ret = Msprof::Engine::UnInit(GE_PROFILING_MODULE);
  if (ret != SUCCESS) {
    GELOGE(ret, "Profiling plugin uninit failed, ret:%d", ret);
  }
  int32_t dev_num = -1;
  rtError_t rt_ret = rtProfilerStop(PROF_MODEL_LOAD_MASK, dev_num, nullptr);
  if (rt_ret != RT_ERROR_NONE) {
    GELOGE(FAILED, "Runtime profiler stop failed.");
    return FAILED;
  }

  for (auto device_id_module : device_id_module_map_) {
    if (device_id_module.second != 0) {
      uint32_t device_id = static_cast<uint32_t>(device_id_module.first);
      GELOGI("Prof finalize: device_id: %u, module: 0x%llx.", device_id, device_id_module.second);
      rt_ret = rtProfilerStop(device_id_module.second, 1, &device_id);
      if (rt_ret != RT_ERROR_NONE) {
        GELOGE(FAILED, "Runtime profiler stop failed.");
        return FAILED;
      }
    }
  }
  device_id_module_map_.clear();
  device_id_.clear();
  GELOGI("Prof finalize success.");
#endif
  return SUCCESS;
}

Status ProfilingManager::ProfParseDeviceId(const std::map<std::string, std::string> &config_para,
                                           vector<int32_t> &device_list) {
#ifdef DAVINCI_SUPPORT_PROFILING
  auto iter = config_para.find(kConfigDevIdList);
  if (iter != config_para.end()) {
    std::string device_id_list = iter->second;
    std::string temp;
    vector<std::string> decvice_id;
    for (uint32_t i = 0; i < device_id_list.size(); i++) {
      if (isdigit(device_id_list[i])) {
        temp.append(1, device_id_list[i]);
      } else {
        if (!temp.empty()) {
          decvice_id.emplace_back(temp);
        }
        temp.clear();
      }
    }
    if (!temp.empty()) {
      decvice_id.emplace_back(temp);
    }

    for (uint32_t i = 0; i < decvice_id.size(); i++) {
      try {
        int32_t dev_id = std::stoi(decvice_id[i]);
        device_list.push_back(dev_id);
      } catch (std::invalid_argument &) {
        GELOGE(FAILED, "Device id: %s is invalid.", decvice_id[i].c_str());
        return FAILED;
      } catch (std::out_of_range &) {
        GELOGE(FAILED, "Device id: %s is  out of range.", decvice_id[i].c_str());
      } catch (...) {
        GELOGE(FAILED, "Device id: %s cannot change to int.", decvice_id[i].c_str());
        return FAILED;
      }
    }
  } else {
    GELOGE(FAILED, "Config para not contain device id list.");
    return FAILED;
  }
#endif
  return SUCCESS;
}

Status ProfilingManager::ProfParseParam(const std::map<std::string, std::string> &config_para, int32_t &device_num,
                                        vector<int32_t> &device_list) {
#ifdef DAVINCI_SUPPORT_PROFILING
  // device num
  auto iter = config_para.find(kConfigNumsdev);
  if (iter != config_para.end()) {
    try {
      device_num = std::stoi(iter->second);
    } catch (std::invalid_argument &) {
      GELOGE(FAILED, "Device nun: %s is invalid.", iter->second.c_str());
      return FAILED;
    } catch (std::out_of_range &) {
      GELOGE(FAILED, "Device num: %s is  out of range.", iter->second.c_str());
    } catch (...) {
      GELOGE(FAILED, "Device num: %s cannot change to int.", iter->second.c_str());
      return FAILED;
    }
  } else {
    GELOGE(FAILED, "Config para not contain device num.");
    return FAILED;
  }
  // device id
  if (ProfParseDeviceId(config_para, device_list) != SUCCESS) {
    GELOGE(FAILED, "Parse config para device id failed.");
    return FAILED;
  }

  if (device_num == 0 || device_num > kMaxDeviceNum || device_num != static_cast<int32_t>(device_list.size())) {
    GELOGE(FAILED, "Config para device num: %d not equal to device list size: %d.", device_num, device_list.size());
    return FAILED;
  }
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY Status
ProfilingManager::ProfStartProfiling(uint64_t module, const std::map<std::string, std::string> &config_para) {
#ifdef DAVINCI_SUPPORT_PROFILING
  std::lock_guard<std::mutex> lock(mutex_);
  int32_t device_num = 0;
  vector<int32_t> device_list;
  if (ProfParseParam(config_para, device_num, device_list) != SUCCESS) {
    GELOGE(FAILED, "Prof start parse param failed.");
    return FAILED;
  }
  auto *device_id = new (std::nothrow) uint32_t[device_num];
  if (device_id == nullptr) {
    GELOGE(FAILED, "Prof start parse param failed.");
    return FAILED;
  }
  for (int32_t i = 0; i < device_num; i++) {
    device_id[i] = static_cast<uint32_t>(device_list[i]);
  }
  GELOGI("Runtime config param: 0x%llx, device num: %d.", module, device_num);
  rtError_t rt_ret = rtProfilerStart(module, device_num, device_id);
  if (rt_ret != RT_ERROR_NONE) {
    delete[] device_id;
    GELOGE(FAILED, "Runtime profiler config proc failed.");
    return FAILED;
  }
  delete[] device_id;
  device_id = nullptr;
  if ((module & PROF_MODEL_EXECUTE_MASK) == PROF_MODEL_EXECUTE_MASK) {
    for (int32_t i = 0; i < device_num; i++) {
      if (std::find(device_id_.begin(), device_id_.end(), device_list[i]) == device_id_.end()) {
        device_id_.push_back(device_list[i]);
      }
    }
    GELOGI("Prof start: ge execute model start profiling.");
  }
  if ((module & PROF_MODEL_LOAD_MASK) == PROF_MODEL_LOAD_MASK) {
    GELOGW("Prof start: load model module is invalid.");
  }
  UpdateDeviceIdModuleMap(kProfStart, module, device_list);
  GELOGI("Prof start profiling success.");
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY Status
ProfilingManager::ProfStopProfiling(uint64_t module, const std::map<std::string, std::string> &config_para) {
#ifdef DAVINCI_SUPPORT_PROFILING
  std::lock_guard<std::mutex> lock(mutex_);
  int32_t device_num = 0;
  vector<int32_t> device_list;
  if (ProfParseParam(config_para, device_num, device_list) != SUCCESS) {
    GELOGE(FAILED, "Prof stop parse param failed.");
    return FAILED;
  }
  auto *device_id = new (std::nothrow) uint32_t[device_num];
  if (device_id == nullptr) {
    GELOGE(FAILED, "Prof stop parse param failed.");
    return FAILED;
  }
  for (int32_t i = 0; i < device_num; i++) {
    device_id[i] = static_cast<uint32_t>(device_list[i]);
  }
  GELOGI("Prof stop: runtime config param: 0x%llx, device num: %d", module, device_num);
  rtError_t rt_ret = rtProfilerStop(module, device_num, device_id);
  if (rt_ret != RT_ERROR_NONE) {
    delete[] device_id;
    GELOGE(FAILED, "Prof stop: runtime profiler config proc failed.");
    return FAILED;
  }
  delete[] device_id;
  device_id = nullptr;
  uint64_t execute_model_mask = module & PROF_MODEL_EXECUTE_MASK;
  if (execute_model_mask == PROF_MODEL_EXECUTE_MASK) {
    for (int32_t i = 0; i < device_num; i++) {
      auto iter = std::find(device_id_.begin(), device_id_.end(), device_list[i]);
      if (iter != device_id_.end()) {
        device_id_.erase(iter);
      }
    }
    GELOGI("Prof stop: ge execute model stop profiling.");
  }
  if ((module & PROF_MODEL_LOAD_MASK) == PROF_MODEL_LOAD_MASK) {
    GELOGW("Prof stop: load model module is invalid.");
  }
  UpdateDeviceIdModuleMap(kProfStop, module, device_list);
  GELOGI("Prof stop profiling success.");
#endif
  return SUCCESS;
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY void ProfilingManager::UpdateDeviceIdModuleMap(
  string prof_type, uint64_t module, const vector<int32_t> &device_list) {
#ifdef DAVINCI_SUPPORT_PROFILING
  if (prof_type == kProfStart) {
    for (uint32_t i = 0; i < device_list.size(); i++) {
      auto iter = device_id_module_map_.find(device_list[i]);
      if (iter != device_id_module_map_.end()) {
        uint64_t prof_on_module = device_id_module_map_[device_list[i]];
        // save all profiling on module of device
        device_id_module_map_[device_list[i]] = prof_on_module | module;
      } else {
        device_id_module_map_[device_list[i]] = module;
      }
    }
  } else if (prof_type == kProfStop) {
    for (uint32_t i = 0; i < device_list.size(); i++) {
      auto iter = device_id_module_map_.find(device_list[i]);
      if (iter != device_id_module_map_.end()) {
        uint64_t prof_on_module = device_id_module_map_[device_list[i]];
        uint64_t prof_off_module = prof_on_module & module;
        uint64_t prof_on_left_module = prof_on_module & (~prof_off_module);
        // stop profiling on module of device
        device_id_module_map_[device_list[i]] = prof_on_left_module;
      }
    }
  } else {
    GELOGI("No need to update device_id module map.");
  }
#endif
}

FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY bool ProfilingManager::ProfilingModelExecuteOn() const {
  int32_t logic_device_id = 0;
  rtError_t rt_ret = rtGetDevice(&logic_device_id);
  if (rt_ret != RT_ERROR_NONE) {
    GELOGE(rt_ret, "Runtime get logic_device_id failed, current logic_device_id:%d", logic_device_id);
  }
  GELOGI("Current logic_device_id:%d", logic_device_id);

  bool execute_model_prof_on = false;
  auto iter = std::find(device_id_.begin(), device_id_.end(), logic_device_id);
  if (iter != device_id_.end()) {
    execute_model_prof_on = true;
  }
  GELOGI("Flag is_execute_profiling: %d, execute_model_prof_on: %d", is_execute_profiling_, execute_model_prof_on);
  return is_execute_profiling_ || execute_model_prof_on;
}

/**
 * @brief Profiling PluginImpl
 */
// PluginImpl static variable init
Msprof::Engine::Reporter *PluginImpl::reporter_ = nullptr;

PluginImpl::PluginImpl(const std::string &module) : module_(module) { GELOGI("Create PluginImpl\n"); }

int PluginImpl::Init(const Msprof::Engine::Reporter *reporter) {
  GELOGI("PluginImpl init");
  reporter_ = const_cast<Msprof::Engine::Reporter *>(reporter);
  return 0;
}

int PluginImpl::UnInit() {
  GELOGI("PluginImpl Uninit");
  reporter_ = nullptr;
  return 0;
}

Msprof::Engine::PluginIntf *ProfilingEngineImpl::CreatePlugin() {
  GELOGI(" Create Plugin");
  return new (std::nothrow) PluginImpl(GE_PROFILING_MODULE);
}

int ProfilingEngineImpl::ReleasePlugin(Msprof::Engine::PluginIntf *plugin) {
  if (plugin != nullptr) {
    delete plugin;
    plugin = nullptr;
  }
  return 0;
}
}  // namespace ge
