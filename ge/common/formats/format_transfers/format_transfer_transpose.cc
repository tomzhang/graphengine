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

#include "common/formats/format_transfers/format_transfer_transpose.h"

#include <securec.h>
#include <memory>

#include "common/formats/utils/formats_trans_utils.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/debug/log.h"
#include "graph/utils/type_utils.h"

namespace ge {
namespace formats {
namespace {
std::map<Format, std::map<Format, std::vector<int64_t>>> perm_args{
  {FORMAT_NCHW,
   {{FORMAT_NHWC, std::vector<int64_t>({0, 2, 3, 1})},
    {FORMAT_HWCN, std::vector<int64_t>({2, 3, 1, 0})},
    {FORMAT_CHWN, std::vector<int64_t>({1, 2, 3, 0})}}},
  {FORMAT_NHWC,
   {{FORMAT_NCHW, std::vector<int64_t>({0, 3, 1, 2})},
    {FORMAT_CHWN, std::vector<int64_t>({3, 1, 2, 0})},
    {FORMAT_HWCN, std::vector<int64_t>({1, 2, 3, 0})}}},
  {FORMAT_HWCN,
   {{FORMAT_NCHW, std::vector<int64_t>({3, 2, 0, 1})},
    {FORMAT_NHWC, std::vector<int64_t>({3, 0, 1, 2})},
    {FORMAT_CHWN, std::vector<int64_t>({2, 0, 1, 3})}}},
  {FORMAT_CHWN,
   {{FORMAT_NCHW, std::vector<int64_t>({3, 0, 1, 2})},
    {FORMAT_NHWC, std::vector<int64_t>({3, 1, 2, 0})},
    {FORMAT_HWCN, std::vector<int64_t>({1, 2, 0, 3})}}},
};

bool IsShapeArgValid(const std::vector<int64_t> &src_shape, const std::vector<int64_t> &perm_arg) {
  if (src_shape.empty()) {
    GELOGE(PARAM_INVALID, "Failed to transpose, empty src shape");
    return false;
  }
  for (auto dim : src_shape) {
    if (dim < 0) {
      GELOGE(PARAM_INVALID, "Failed to transpose, negative dim in src shape %s", ShapeToString(src_shape).c_str());
      return false;
    }
  }
  if (perm_arg.size() != src_shape.size()) {
    GELOGE(PARAM_INVALID,
           "Failed to transpose, the size of src shape(%zu) and"
           " perm arg(%zu) are different",
           src_shape.size(), perm_arg.size());
    return false;
  }

  std::vector<int64_t> exists(perm_arg.size());
  for (auto perm : perm_arg) {
    if (perm < 0 || static_cast<size_t>(perm) >= perm_arg.size() || ++exists[perm] > 1) {
      GELOGE(PARAM_INVALID, "Failed to transpose, duplicated perm arg %ld, perm arg %s", perm,
             JoinToString(perm_arg).c_str());
      return false;
    }
  }
  return true;
}
bool IsTransposeArgValid(const uint8_t *src, const std::vector<int64_t> &src_shape, DataType src_data_type,
                         const std::vector<int64_t> &perm_arg) {
  if (src == nullptr) {
    GELOGE(PARAM_INVALID, "Failed to transpose, the src is null");
    return false;
  }
  if (GetSizeByDataType(src_data_type) < 0) {
    GELOGE(UNSUPPORTED, "Failed to transpose, the data type %s is not support",
           TypeUtils::DataTypeToSerialString(src_data_type).c_str());
    return false;
  }
  return IsShapeArgValid(src_shape, perm_arg);
}

std::vector<int64_t> GenHeads(const std::vector<int64_t> &shape) {
  std::vector<int64_t> heads(shape.size());
  bool first = true;
  for (auto i = static_cast<int64_t>(shape.size() - 1); i >= 0; --i) {
    if (first) {
      heads[i] = 1;
      first = false;
    } else {
      heads[i] = shape[i + 1] * heads[i + 1];
    }
  }
  return heads;
}

int64_t GenOffset(const std::vector<int64_t> &offsets, const std::vector<int64_t> &indexes) {
  int64_t offset = 0;
  for (size_t i = 0; i < indexes.size(); ++i) {
    offset += offsets[i] * indexes[i];
  }
  return offset;
}

void AddOne(const std::vector<int64_t> &shape, std::vector<int64_t> &indexes) {
  size_t i = indexes.size() - 1;
  indexes[i]++;
  while (i > 0) {
    if (indexes[i] >= shape[i]) {
      indexes[i] = 0;
      indexes[i - 1]++;
      --i;
    } else {
      break;
    }
  }
}

std::vector<int64_t> TransShapeByPerm(const std::vector<int64_t> &src_shape, const std::vector<int64_t> &perm_arg) {
  std::vector<int64_t> dst_shape(src_shape.size());
  for (size_t i = 0; i < perm_arg.size(); ++i) {
    dst_shape[i] = src_shape[perm_arg[i]];
  }
  return dst_shape;
}
}  // namespace

Status Transpose(const uint8_t *src, const std::vector<int64_t> &src_shape, DataType src_data_type,
                 const std::vector<int64_t> &perm_arg, TransResult &result) {
  if (!IsTransposeArgValid(src, src_shape, src_data_type, perm_arg)) {
    return PARAM_INVALID;
  }

  auto dst_shape = TransShapeByPerm(src_shape, perm_arg);
  auto src_origin_ordered_heads = GenHeads(src_shape);
  auto src_heads = TransShapeByPerm(src_origin_ordered_heads, perm_arg);

  int64_t dst_ele_num = GetItemNumByShape(dst_shape);
  int64_t data_size = GetSizeByDataType(src_data_type);
  int64_t dst_size = data_size * dst_ele_num;

  GELOGD("Begin to transpose, src shape %s, perm arg %s, dst shape %s, data type %s", JoinToString(src_shape).c_str(),
         JoinToString(perm_arg).c_str(), JoinToString(dst_shape).c_str(),
         TypeUtils::DataTypeToSerialString(src_data_type).c_str());
  if (dst_ele_num == 0) {
    result.length = static_cast<size_t>(dst_size);
    return SUCCESS;
  }

  std::shared_ptr<uint8_t> dst(new (std::nothrow) uint8_t[dst_size], std::default_delete<uint8_t[]>());
  int64_t dst_index = 0;
  std::vector<int64_t> dst_indexes(dst_shape.size());
  while (dst_index < dst_ele_num) {
    auto src_offset = GenOffset(src_heads, dst_indexes) * data_size;
    auto dst_offset_bytes = dst_index * data_size;
    auto protected_size = dst_size - dst_offset_bytes < static_cast<int64_t>(SECUREC_MEM_MAX_LEN)
                            ? dst_size - dst_offset_bytes
                            : static_cast<int64_t>(SECUREC_MEM_MAX_LEN);
    auto ret = memcpy_s(dst.get() + dst_offset_bytes, static_cast<size_t>(protected_size), src + src_offset,
                        static_cast<size_t>(data_size));
    if (ret != EOK) {
      GELOGE(INTERNAL_ERROR,
             "Failed to transpose, src shape %s, perm arg %s, dst shape %s, "
             "failed to write to dst offset %ld, current dim offset %s",
             ShapeToString(src_shape).c_str(), ShapeToString(perm_arg).c_str(), ShapeToString(dst_shape).c_str(),
             dst_offset_bytes, ShapeToString(dst_indexes).c_str());
      return INTERNAL_ERROR;
    }
    AddOne(dst_shape, dst_indexes);
    ++dst_index;
  }

  result.data = dst;
  result.length = static_cast<size_t>(dst_size);
  return SUCCESS;
}

Status TransposeWithShapeCheck(const uint8_t *data, const std::vector<int64_t> &src_shape,
                               const std::vector<int64_t> &dst_shape, DataType src_data_type,
                               const std::vector<int64_t> &perm_arg, TransResult &result) {
  if (!IsTransposeArgValid(data, src_shape, src_data_type, perm_arg)) {
    return PARAM_INVALID;
  }
  auto expected_shape = TransShapeByPerm(src_shape, perm_arg);
  if (dst_shape != expected_shape) {
    GELOGE(PARAM_INVALID, "Failed to trans axis for perm_arg %s, invalid dst shape %s, expect %s",
           ShapeToString(perm_arg).c_str(), ShapeToString(dst_shape).c_str(), ShapeToString(expected_shape).c_str());
    return PARAM_INVALID;
  }

  return Transpose(data, src_shape, src_data_type, perm_arg, result);
}

Status GetPermByForamt(Format src_format, Format dst_format, std::vector<int64_t> &perm) {
  auto dst_iter = perm_args.find(src_format);
  if (dst_iter == perm_args.end()) {
    GELOGE(UNSUPPORTED, "Failed to trans shape, do not support transpose from format %s to %s",
           TypeUtils::FormatToSerialString(src_format).c_str(), TypeUtils::FormatToSerialString(dst_format).c_str());
    return UNSUPPORTED;
  }
  auto iter = dst_iter->second.find(dst_format);
  if (iter == dst_iter->second.end()) {
    GELOGE(UNSUPPORTED, "Failed to trans shape, do not support transpose from format %s to %s",
           TypeUtils::FormatToSerialString(src_format).c_str(), TypeUtils::FormatToSerialString(dst_format).c_str());
    return UNSUPPORTED;
  }
  perm = iter->second;
  return SUCCESS;
}

Status FormatTransferTranspose::TransFormat(const TransArgs &args, TransResult &result) {
  std::vector<int64_t> expected_shape;
  auto ret = TransShape(args.src_format, args.src_shape, args.src_data_type, args.dst_format, expected_shape);
  if (ret != SUCCESS) {
    return ret;
  }
  if (args.dst_shape != expected_shape) {
    GELOGE(PARAM_INVALID, "Failed to trans format from %s to %s, invalid dst shape %s, expect %s",
           TypeUtils::FormatToSerialString(args.src_format).c_str(),
           TypeUtils::FormatToSerialString(args.dst_format).c_str(), ShapeToString(args.dst_shape).c_str(),
           ShapeToString(expected_shape).c_str());
    return PARAM_INVALID;
  }

  return Transpose(args.data, args.src_shape, args.src_data_type, perm_args[args.src_format][args.dst_format], result);
}

Status FormatTransferTranspose::TransShape(Format src_format, const std::vector<int64_t> &src_shape, DataType data_type,
                                           Format dst_format, std::vector<int64_t> &dst_shape) {
  std::vector<int64_t> perm_arg;
  GE_CHK_STATUS_RET_NOLOG(GetPermByForamt(src_format, dst_format, perm_arg));
  if (!IsShapeArgValid(src_shape, perm_arg)) {
    return PARAM_INVALID;
  }
  dst_shape = TransShapeByPerm(src_shape, perm_arg);
  return SUCCESS;
}

REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NCHW, FORMAT_NHWC)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NCHW, FORMAT_HWCN)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NCHW, FORMAT_CHWN)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NHWC, FORMAT_NCHW)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NHWC, FORMAT_CHWN)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_NHWC, FORMAT_HWCN)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_HWCN, FORMAT_NCHW)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_HWCN, FORMAT_NHWC)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_HWCN, FORMAT_CHWN)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_CHWN, FORMAT_NCHW)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_CHWN, FORMAT_NHWC)
REGISTER_FORMAT_TRANSFER(FormatTransferTranspose, FORMAT_CHWN, FORMAT_HWCN)
}  // namespace formats
}  // namespace ge
