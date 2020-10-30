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

#ifndef GE_GRAPH_PASSES_FOLDING_KERNEL_REFORMAT_KERNEL_H_
#define GE_GRAPH_PASSES_FOLDING_KERNEL_REFORMAT_KERNEL_H_

#include <vector>

#include "inc/kernel.h"

namespace ge {
class ReFormatKernel : public Kernel {
 public:
  Status Compute(const OpDescPtr attr, const std::vector<ConstGeTensorPtr> &input,
                 std::vector<GeTensorPtr> &v_output) override;

 private:
  Status ValidateInput(const OpDescPtr &attr, const std::vector<ConstGeTensorPtr> &input) const;
};
}  // namespace ge

#endif  // GE_GRAPH_PASSES_FOLDING_KERNEL_REFORMAT_KERNEL_H_
