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

#ifndef GE_HYBRID_EXECUTOR_EXECUTOR_SUBGRAPH_EXECUTOR_H_
#define GE_HYBRID_EXECUTOR_EXECUTOR_SUBGRAPH_EXECUTOR_H_

#include <vector>

#include "common/blocking_queue.h"
#include "common/thread_pool.h"
#include "hybrid/executor/subgraph_context.h"
#include "hybrid/executor/node_state.h"
#include "hybrid/executor/hybrid_execution_context.h"
#include "hybrid/executor/worker/shape_inference_engine.h"
#include "hybrid/model/graph_item.h"
#include "hybrid/node_executor/task_context.h"

namespace ge {
namespace hybrid {
// Executor for executing a subgraph
class SubgraphExecutor {
 public:
  SubgraphExecutor(const GraphItem *graph_item, GraphExecutionContext *context, bool force_infer_shape = false);
  ~SubgraphExecutor();

  /**
   * Execute subgraph async, output tensor address(not data) and output tensor descriptions are
   * valid after this method returned
   * @param inputs          input tensors
   * @param input_desc      input tensor descriptions
   * @return SUCCESS on success, error code otherwise
   */
  Status ExecuteAsync(const std::vector<TensorValue> &inputs, const std::vector<ConstGeTensorDescPtr> &input_desc);

  /**
   * Execute subgraph async, output tensor address(not data) and output tensor descriptions are
   * valid after this method returned
   * @param task_context    instance of TaskContext
   * @return SUCCESS on success, error code otherwise
   */
  Status ExecuteAsync(TaskContext &task_context);

  /**
   * Synchronize all tasks in the subgraph. output tensor data are valid after this method returned
   * @return SUCCESS on success, error code otherwise
   */
  Status Synchronize();

  /**
   * Get output tensors
   * @param outputs         output tensors
   * @return SUCCESS on success, error code otherwise
   */
  Status GetOutputs(std::vector<TensorValue> &outputs);

  /**
   * Get output tensors and output tensor descriptions
   * @param outputs         output tensors
   * @param output_desc     output tensor descriptions
   * @return SUCCESS on success, error code otherwise
   */
  Status GetOutputs(std::vector<TensorValue> &outputs, std::vector<ConstGeTensorDescPtr> &output_desc);

 private:
  static Status PrepareForExecution(GraphExecutionContext *ctx, NodeState &node_state);
  static Status InferShape(ShapeInferenceEngine *shape_inference_engine, NodeState &node_state);
  Status Init(const std::vector<TensorValue> &inputs,
              const std::vector<ConstGeTensorDescPtr> &input_desc);
  Status InitInputsForUnknownShape(const std::vector<TensorValue> &inputs,
                                   const std::vector<ConstGeTensorDescPtr> &input_desc);
  Status InitInputsForKnownShape(const std::vector<TensorValue> &inputs);
  Status ExecuteAsyncForKnownShape(const std::vector<TensorValue> &inputs);
  Status ScheduleTasks();
  Status PrepareNodes();
  Status LaunchTasks();
  Status SetOutputsToParentNode(TaskContext &task_context);

  const GraphItem *graph_item_;
  GraphExecutionContext *context_;
  std::unique_ptr<SubgraphContext> subgraph_context_;
  bool force_infer_shape_;
  ThreadPool pre_run_pool_;
  BlockingQueue<NodeState *> ready_queue_;
  std::unique_ptr<ShapeInferenceEngine> shape_inference_engine_;
  std::shared_ptr<TaskContext> known_shape_task_context_;
};
}  // namespace hybrid
}  // namespace ge
#endif // GE_HYBRID_EXECUTOR_EXECUTOR_SUBGRAPH_EXECUTOR_H_