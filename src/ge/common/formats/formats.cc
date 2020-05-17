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

#include "common/formats/formats.h"

#include <securec.h>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "framework/common/debug/ge_log.h"
#include "framework/common/ge_inner_error_codes.h"
#include "graph/utils/type_utils.h"

namespace ge {
namespace formats {
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY Status TransFormat(const TransArgs &args, TransResult &result) {
  auto transfer = BuildFormatTransfer(args);
  if (transfer == nullptr) {
    GELOGE(UNSUPPORTED, "Failed to trans data from format %s to %s, unsupport now",
           TypeUtils::FormatToSerialString(args.src_format).c_str(),
           TypeUtils::FormatToSerialString(args.dst_format).c_str());
    return UNSUPPORTED;
  }
  if (args.data == nullptr) {
    GELOGE(PARAM_INVALID, "Invalid input null data");
    return PARAM_INVALID;
  }
  return transfer->TransFormat(args, result);
}

GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY Status TransShape(Format src_format,
                                                                 const std::vector<int64_t> &src_shape,
                                                                 DataType data_type, Format dst_format,
                                                                 std::vector<int64_t> &dst_shape) {
  formats::TransArgs args;
  args.src_format = src_format;
  args.dst_format = dst_format;
  auto transfer = BuildFormatTransfer(args);
  if (transfer == nullptr) {
    GELOGE(UNSUPPORTED, "Failed to trans data from format %s to %s, unsupport now",
           TypeUtils::FormatToSerialString(args.src_format).c_str(),
           TypeUtils::FormatToSerialString(args.dst_format).c_str());
    return UNSUPPORTED;
  }

  return transfer->TransShape(src_format, src_shape, data_type, dst_format, dst_shape);
}

GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY Status TransDataType(const CastArgs &args, TransResult &result) {
  auto transfer = BuildDataTypeTransfer(args);
  if (transfer == nullptr) {
    GELOGE(UNSUPPORTED, "Failed to trans data from datatype %s to %s, unsupport now",
           TypeUtils::DataTypeToSerialString(args.src_data_type).c_str(),
           TypeUtils::DataTypeToSerialString(args.dst_data_type).c_str());
    return UNSUPPORTED;
  }
  return transfer->TransDataType(args, result);
}

GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY bool IsTransFormatSupport(const TransArgs &args) {
  return FormatTransferExists(args);
}

GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY bool IsTransDataTypeSupport(const CastArgs &args) {
  return DataTypeTransferExists(args);
}
}  // namespace formats
}  // namespace ge
