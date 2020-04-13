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

#ifndef GE_OP_NN_DETECT_OPS_H_
#define GE_OP_NN_DETECT_OPS_H_

#include "graph/operator_reg.h"
#include "graph/operator.h"

namespace ge {

/**
*@brief Generates bounding boxes based on "rois" and "deltas". It is a customized FasterRcnn operator.

*@par Inputs:
* Two inputs, including: \n
*@li rois: Region of interests (ROIs) generated by the region proposal network (RPN). A 2D Tensor of type float 32 with shape (N, 4). "N" indicates the number of ROIs, and the value "4" refers to "x0", "x1", "y0", and "y1".
*@li deltas: Absolute variation between the ROIs generated by the RPN and ground truth boxes. A 2D Tensor of type float32 with shape (N, 4). "N" indicates the number of errors, and 4 indicates "dx", "dy", "dw", and "dh".

*@par Attributes:
*@li means: An index of type int. Defaults to [0,0,0,0]. "deltas" = "deltas" x "stds" + "means".
*@li stds: An index of type int. Defaults to [0,0,0,0]. "deltas" = "deltas" x "stds" + "means".
*@li max_shape: Shape [h, w], specifying the size of the image transferred to the network. Used to ensure that the bbox shape after conversion does not exceed "max_shape".
*@li wh_ratio_clip: Defaults to "16/1000". The values of "dw" and "dh" fall within (-wh_ratio_clip, wh_ratio_clip).

*@par Outputs:
*bboxes: Bboxes generated based on "rois" and "deltas". Have the same format and type as "rois".
*/
REG_OP(BoundingBoxDecode)
    .INPUT(rois, TensorType({DT_FLOAT16, DT_FLOAT}))
    .INPUT(deltas, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OUTPUT(bboxes, TensorType({DT_FLOAT16, DT_FLOAT}))
    .ATTR(means, ListFloat, {0.0, 0.0, 0.0, 0.0})
    .ATTR(stds, ListFloat, {1.0, 1.0, 1.0, 1.0})
    .REQUIRED_ATTR(max_shape, ListInt)
    .ATTR(wh_ratio_clip, Float, 0.016)
    .OP_END_FACTORY_REG(BoundingBoxDecode)

/**
*@brief Computes the coordinate variations between bboxes and ground truth boxes. It is a customized FasterRcnn operator.

*@par Inputs:
* Two inputs, including: \n
*@li anchor_box: Anchor boxes. A 2D Tensor of float32 with shape (N, 4). "N" indicates the number of bounding boxes, and the value "4" refers to "x0", "x1", "y0", and "y1".
*@li ground_truth_box: Ground truth boxes. A 2D Tensor of float32 with shape (N, 4). "N" indicates the number of bounding boxes, and the value "4" refers to "x0", "x1", "y0", and "y1".

*@par Attributes:
*@li means: An index of type int. Defaults to [0,0,0,0]. "deltas" = "deltas" x "stds" + "means".
*@li stds: An index of type int. Defaults to [0,0,0,0]. "deltas" = "deltas" x "stds" + "means".

*@par Outputs:
*delats: A 2D Tensor of type float32 with shape (N, 4), specifying the variations between all anchor boxes and ground truth boxes.
*/
REG_OP(BoundingBoxEncode)
    .INPUT(anchor_box, TensorType({DT_FLOAT16, DT_FLOAT}))
    .INPUT(ground_truth_box, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OUTPUT(delats, TensorType({DT_FLOAT16, DT_FLOAT}))
    .ATTR(means, ListFloat, {0.0, 0.0, 0.0, 0.0})
    .ATTR(stds, ListFloat, {1.0, 1.0, 1.0, 1.0})
    .OP_END_FACTORY_REG(BoundingBoxEncode)

/**
*@brief Judges whether the bounding box is valid. It is a customized FasterRcnn operator.

*@par Inputs:
* Two inputs, including: \n
*@li bbox_tensor: Bounding box. A 2D Tensor of type float16 with shape (N, 4). "N" indicates the number of bounding boxes, the value "4" indicates "x0", "x1", "y0", and "y1".
*@li img_metas: Valid boundary value of the image. A 1D Tensor of type float16 with shape (16,)

*@par Outputs:
*valid_tensor: A bool with shape (N, 1), specifying whether an input anchor is in an image. "1" indicates valid, while "0" indicates invalid.

*@attention Constraints:
* 16 "img_metas" are input. The first three numbers (height, width, ratio) are valid, specifying the valid boundary (heights x ratio, weights x ratio).
*/
REG_OP(CheckValid)
    .INPUT(bbox_tensor, TensorType({DT_FLOAT16}))
    .INPUT(img_metas, TensorType({DT_FLOAT16}))
    .OUTPUT(valid_tensor, TensorType({DT_INT8}))
    .OP_END_FACTORY_REG(CheckValid)

/**
*@brief Computes the intersection over union (iou) or the intersection over foreground (iof) based on the ground-truth and predicted regions.

*@par Inputs:
* Two inputs, including: \n
*@li bboxes: Bounding boxes, a 2D Tensor of type float16 with shape (N, 4). "N" indicates the number of bounding boxes, and the value "4" refers to "x0", "x1", "y0", and "y1".
*@li gtboxes: Ground-truth boxes, a 2D Tensor of type float16 with shape (M, 4). "M" indicates the number of ground truth boxes, and the value "4" refers to "x0", "x1", "y0", and "y1".

*@par Attributes:
*mode: Computation mode, a character string with the value range of [iou, iof].

*@par Outputs:
*overlap: A 2D Tensor of type float16 with shape [M, N], specifying the IoU or IoF ratio.

*@attention Constraints:
* Only computation of float16 data is supported. To avoid overflow, the input length and width are scaled by 0.2 internally.
*/
REG_OP(Iou)
    .INPUT(bboxes, TensorType({DT_FLOAT16, DT_FLOAT}))
    .INPUT(gtboxes, TensorType({DT_FLOAT16, DT_FLOAT}))
    .OUTPUT(overlap, TensorType({DT_FLOAT16, DT_FLOAT}))
    .ATTR(mode, String, "iou")
    .OP_END_FACTORY_REG(Iou)

/**
*@brief Performs the backpropagation of ROIAlign for training scenarios.

*@par Inputs:
* Three inputs, including: \n
*@li ydiff: A 5HD gradient input of type float32.
*@li rois: ROI position. A 2D Tensor of float32 with shape (N, 5). "N" indicates the number of ROIs, the value "5" indicates the indexes of images where the ROIs are located, "x0", "x1", "y0", and "y1".
*@li rois_n: An optional input, specifying the number of valid ROIs. This parameter is reserved.

*@par Attributes:
*@li xdiff_shape: A required list of 4 ints, obtained based on the shape of "features" of ROIAlign.
*@li pooled_width: A required attribute of type int, specifying the W dimension.
*@li pooled_height: A required attribute of type int, specifying the H dimension.
*@li spatial_scale: A required attribute of type float, specifying the scaling ratio of "features" to the original image.
*@li sample_num: An optional attribute of type int, specifying the horizontal and vertical sampling frequency of each output. If this attribute is set to "0", the sampling frequency is equal to the rounded up value of "rois", which is a floating point number. Defaults to "2".

*@par Outputs:
*xdiff: Gradient added to input "features". Has the same 5HD shape as input "features".
*/
REG_OP(ROIAlignGrad)
    .INPUT(ydiff, TensorType({DT_FLOAT}))
    .INPUT(rois, TensorType({DT_FLOAT}))
    .OPTIONAL_INPUT(rois_n, TensorType({DT_INT32}))
    .OUTPUT(xdiff, TensorType({DT_FLOAT}))
    .REQUIRED_ATTR(xdiff_shape, ListInt)
    .REQUIRED_ATTR(pooled_width, Int)
    .REQUIRED_ATTR(pooled_height, Int)
    .REQUIRED_ATTR(spatial_scale, Float)
    .ATTR(sample_num, Int, 2)
    .OP_END_FACTORY_REG(ROIAlignGrad)

/**
*@brief Obtains the ROI feature matrix from the feature map. It is a customized FasterRcnn operator.

*@par Inputs:
* Three inputs, including: \n
*@li features: A 5HD Tensor of type float32.
*@li rois: ROI position. A 2D Tensor of float32 with shape (N, 5). "N" indicates the number of ROIs, the value "5" indicates the indexes of images where the ROIs are located, "x0", "x1", "y0", and "y1".
*@li rois_n: An optional input, specifying the number of valid ROIs. This parameter is reserved.

*@par Attributes:
*@li spatial_scale: A required attribute of type float, specifying the scaling ratio of "features" to the original image.
*@li pooled_height: A required attribute of type int, specifying the H dimension.
*@li pooled_width: A required attribute of type int, specifying the W dimension.
*@li sample_num: An optional attribute of type int, specifying the horizontal and vertical sampling frequency of each output. If this attribute is set to "0", the sampling frequency is equal to the rounded up value of "rois", which is a floating point number. Defaults to "2".

*@par Outputs:
*output: Outputs the feature sample of each ROI position. The format is 5HD. The axis N is the number of input ROIs. Axes H, W, and C are consistent with the values of "pooled_height", "pooled_width", and "features", respectively.
*/
REG_OP(ROIAlign)
    .INPUT(features, TensorType({DT_FLOAT}))
    .INPUT(rois, TensorType({DT_FLOAT}))
    .OPTIONAL_INPUT(rois_n, TensorType({DT_INT32}))
    .OUTPUT(output, TensorType({DT_FLOAT}))
    .REQUIRED_ATTR(spatial_scale, Float)
    .REQUIRED_ATTR(pooled_height, Int)
    .REQUIRED_ATTR(pooled_width, Int)
    .ATTR(sample_num, Int, 2)
    .OP_END_FACTORY_REG(ROIAlign)

}  // namespace ge

#endif  // GE_OP_NN_DETECT_OPS_H_
