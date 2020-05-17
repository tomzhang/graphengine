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

#include "graph/load/new_model_manager/cpu_queue_schedule.h"
#include "common/debug/ge_log.h"

namespace {
const uint32_t kCoreDim = 1;  // for rtCpuKernelLaunch
const char *const kCpuTaskModelEnqueue = "modelEnqueue";
const char *const kCpuTaskPrepareInput = "modelPrepareInput";
const char *const kCpuTaskWaitEndGraph = "modelWaitEndGraph";
const char *const kCpuTaskPrepareOutput = "modelPrepareOutput";
const char *const kCpuTaskModelDequeue = "modelDequeue";
const char *const kCpuTaskModelRepeat = "modelRepeat";
}  // namespace

namespace ge {
CpuTaskInfo::CpuTaskInfo(rtStream_t stream) : args_(nullptr), args_size_(0) { stream_ = stream; }

CpuTaskInfo::~CpuTaskInfo() {
  if (args_ == nullptr) {
    return;
  }

  rtError_t status = rtFree(args_);
  if (status != RT_ERROR_NONE) {
    GELOGW("Call rt free failed, status: 0x%x", status);
  }
  args_ = nullptr;
}
///
/// @ingroup ge
/// @brief definiteness queue schedule, bind input queue to task.
/// @param [in] queue_id: input queue id from user.
/// @param [out] in_mbuf: input mbuf addr for input data.
/// @return: 0 for success / others for failed
///
Status CpuTaskModelDequeue::Init(uint32_t queue_id, uintptr_t &in_mbuf) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  args_size_ = sizeof(MbufQueueInfo) + sizeof(uintptr_t);  // sizeof(uintptr_t) for save in_mbuf.
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  in_mbuf = reinterpret_cast<uintptr_t>(args_) + sizeof(MbufQueueInfo);
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  MbufQueueInfo queue_info;
  queue_info.queue_id = queue_id;
  queue_info.in_mbuf = in_mbuf;  // Placeholder, input mbuf addr will save to this place.
  status = rtMemcpy(args_, args_size_, &queue_info, sizeof(MbufQueueInfo), RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskModelDequeue::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskModelDequeue, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch ModelDequeue failed, status: 0x%X", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch model dequeue task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, bind output queue to task.
/// @param [in] addr: NetOutput Op input tensor address.
/// @param [in] size: NetOutput Op input tensor size.
/// @param [in] in_mbuf: input mbuf addr for input data.
/// @return: 0 for success / others for failed
///
Status CpuTaskPrepareInput::Init(uintptr_t addr, uint32_t size, uintptr_t in_mbuf) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  args_size_ = sizeof(PrepareInputInfo);
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  PrepareInputInfo prepare;
  prepare.in_mbuf = in_mbuf;
  prepare.mbuf_offset = 0;
  prepare.data_size = size;
  prepare.data_addr = addr;
  status = rtMemcpy(args_, args_size_, &prepare, args_size_, RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskPrepareInput::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskPrepareInput, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch PrepareInput failed, status: 0x%X", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch prepare input task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, bind output queue to task.
/// @param [in] addr: NetOutput Op input tensor address.
/// @param [in] size: NetOutput Op input tensor size.
/// @param [in] in_mbuf: input mbuf addr for input data.
/// @param [out] out_mbuf: output mbuf addr for output data.
/// @return: 0 for success / others for failed
///
Status CpuTaskPrepareOutput::Init(uintptr_t addr, uint32_t size, uintptr_t in_mbuf, uintptr_t &out_mbuf) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  args_size_ = sizeof(PrepareOutputInfo) + sizeof(uintptr_t);  // sizeof(uintptr_t) for save out_mbuf.
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  out_mbuf = reinterpret_cast<uintptr_t>(args_) + sizeof(PrepareOutputInfo);
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  // Get NetOutput Input address and bind to queue.
  PrepareOutputInfo prepare;
  prepare.data_size = size;
  prepare.data_addr = addr;
  prepare.in_mbuf = in_mbuf;
  prepare.out_mbuf = out_mbuf;  // Placeholder, output mbuf addr will save to this place.
  status = rtMemcpy(args_, args_size_, &prepare, sizeof(PrepareOutputInfo), RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskPrepareOutput::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskPrepareOutput, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch PrepareOutput failed, status: 0x%X", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch prepare output task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, bind output queue to task.
/// @param [in] queue_id: output queue id from user.
/// @param [in] out_mbuf: mbuf for output data.
/// @return: 0 for success / others for failed
///
Status CpuTaskModelEnqueue::Init(uint32_t queue_id, uintptr_t out_mbuf) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  // Get NetOutput Input address and bind to queue.
  args_size_ = sizeof(MbufQueueInfo);
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  MbufQueueInfo queue_info;
  queue_info.queue_id = queue_id;
  queue_info.in_mbuf = out_mbuf;
  status = rtMemcpy(args_, args_size_, &queue_info, args_size_, RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskModelEnqueue::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskModelEnqueue, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch ModelEnqueue failed, status: 0x%X", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch model enqueue task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, active entry stream.
/// @param [in] stream: stream to be active.
/// @return: 0 for success / others for failed
///
Status CpuTaskActiveEntry::Init(rtStream_t stream) {
  if (stream == nullptr) {
    GELOGE(FAILED, "Task active stream not valid");
    return FAILED;
  }

  active_stream_ = stream;
  return SUCCESS;
}

Status CpuTaskActiveEntry::Distribute() {
  if ((active_stream_ == nullptr) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t ret = rtStreamActive(active_stream_, stream_);
  if (ret != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt StreamActive failed, ret: 0x%X", ret);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch wait end task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, wait for end graph.
/// @param [in] model_id: model id for wait end graph.
/// @return: 0 for success / others for failed
///
Status CpuTaskWaitEndGraph::Init(uint32_t model_id) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  args_size_ = sizeof(model_id);
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  status = rtMemcpy(args_, args_size_, &model_id, args_size_, RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskWaitEndGraph::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskWaitEndGraph, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch WaitEndGraph failed, status: 0x%X", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch wait end task success.");
  return SUCCESS;
}

///
/// @ingroup ge
/// @brief definiteness queue schedule, repeat run model.
/// @param [in] model_id: model id for repeat run.
/// @return: 0 for success / others for failed
///
Status CpuTaskModelRepeat::Init(uint32_t model_id) {
  if ((args_ != nullptr) || (args_size_ > 0)) {
    GELOGE(FAILED, "Task already initialized, size: %u", args_size_);
    return FAILED;
  }

  args_size_ = sizeof(model_id);
  rtError_t status = rtMalloc(&args_, args_size_, RT_MEMORY_HBM);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt malloc failed, status: 0x%x", status);
    return RT_FAILED;
  }
  GE_PRINT_DYNAMIC_MEMORY(rtMalloc, "args data.", args_size_)

  status = rtMemcpy(args_, args_size_, &model_id, args_size_, RT_MEMCPY_HOST_TO_DEVICE);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt memcpy failed, status: 0x%x", status);
    return RT_FAILED;
  }

  return SUCCESS;
}

Status CpuTaskModelRepeat::Distribute() {
  if ((args_ == nullptr) || (args_size_ == 0) || (stream_ == nullptr)) {
    GELOGE(FAILED, "Task not initialized, distribute failed, size: %u", args_size_);
    return FAILED;
  }

  rtError_t status = rtCpuKernelLaunch(nullptr, kCpuTaskModelRepeat, kCoreDim, args_, args_size_, nullptr, stream_);
  if (status != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "Call rt CpuKernelLaunch ModelRepeat failed, status: 0x%x", status);
    return RT_FAILED;
  }

  GELOGI("Cpu kernel launch repeat task success.");
  return SUCCESS;
}
}  // namespace ge
