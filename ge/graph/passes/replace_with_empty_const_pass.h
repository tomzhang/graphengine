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

#ifndef GE_GRAPH_PASSES_REPLACE_WITH_EMPTY_CONST_PASS_H_
#define GE_GRAPH_PASSES_REPLACE_WITH_EMPTY_CONST_PASS_H_

#include "graph/passes/base_pass.h"

namespace ge {
class ReplaceWithEmptyConstPass : public BaseNodePass {
 public:
  Status Run(NodePtr &node) override;

 private:
  Status ReplaceWithEmptyConst(NodePtr &node_to_replace);
  Status InsertEmptyConst(const GeTensorDesc &out_desc, NodePtr &const_node, ComputeGraphPtr &graph);
  bool IsEmptyTenor(const GeShape &shape) const;
  std::string GetDimStr(const GeShape &shape);
};
}  // namespace ge
#endif  // GE_GRAPH_PASSES_REPLACE_WITH_EMPTY_CONST_PASS_H_
