/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef _WIN32
#include <unistd.h>
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "tensorflow/contrib/lite/builtin_op_data.h"
#include "tensorflow/contrib/lite/context.h"
#include "tensorflow/contrib/lite/kernels/internal/optimized/depthwiseconv_float.h"
#include "tensorflow/contrib/lite/kernels/internal/optimized/depthwiseconv_uint8.h"
#include "tensorflow/contrib/lite/kernels/internal/quantization_util.h"
#include "tensorflow/contrib/lite/kernels/internal/reference/depthwiseconv_float.h"
#include "tensorflow/contrib/lite/kernels/internal/reference/depthwiseconv_uint8.h"
#include "tensorflow/contrib/lite/kernels/internal/tensor.h"
#include "tensorflow/contrib/lite/kernels/kernel_util.h"
#include "tensorflow/contrib/lite/kernels/op_macros.h"
#include "tensorflow/contrib/lite/kernels/padding.h"

namespace tflite {
namespace ops {
namespace builtin {
namespace depthwise_conv {

constexpr int kInputTensor = 0;
constexpr int kFilterTensor = 1;
constexpr int kBiasTensor = 2;
constexpr int kOutputTensor = 0;

// This file has three implementation of DepthwiseConv.
enum KernelType {
  kReference,
  kGenericOptimized,  // Neon-free
  kNeonOptimized,
};

struct OpData {
  TfLitePaddingValues padding;
  // The scaling factor from input to output (aka the 'real multiplier') can
  // be represented as a fixed point multiplier plus a left shift.
  int32_t output_multiplier;
  int output_shift;
  // The range of the fused activation layer. For example for kNone and
  // uint8_t these would be 0 and 255.
  int32_t output_activation_min;
  int32_t output_activation_max;
};

void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  // This is a builtin op, so we don't use the contents in 'buffer', if any.
  // Instead, we allocate a new object to carry information from Prepare() to
  // Eval().
  return new OpData;
}

void Free(TfLiteContext* context, void* buffer) {
  delete reinterpret_cast<OpData*>(buffer);
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  auto* params =
      reinterpret_cast<TfLiteDepthwiseConvParams*>(node->builtin_data);
  OpData* data = reinterpret_cast<OpData*>(node->user_data);

  // TODO(ahentz): use could use GetOptionalInputTensor() here, but we need to
  // decide whether we are OK with optional tensors being completely absent, as
  // opposed to having -1 as their index.
  bool hasBias = NumInputs(node) == 3;

  TF_LITE_ENSURE(context, hasBias || NumInputs(node) == 2);
  TfLiteTensor* input = GetInput(context, node, kInputTensor);
  TfLiteTensor* filter = GetInput(context, node, kFilterTensor);
  TfLiteTensor* bias = nullptr;

  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

  TF_LITE_ENSURE_EQ(context, NumDimensions(input), 4);
  TF_LITE_ENSURE_EQ(context, NumDimensions(filter), 4);

  // The parameter 'depth_multiplier' is redundant, so we check here to make
  // sure it is consistent with the given dimensions.
  TF_LITE_ENSURE_EQ(context,
                    params->depth_multiplier * SizeOfDimension(input, 3),
                    SizeOfDimension(filter, 3));

  const TfLiteType data_type = input->type;
  TF_LITE_ENSURE(context,
                 data_type == kTfLiteFloat32 || data_type == kTfLiteUInt8);
  TF_LITE_ENSURE_EQ(context, output->type, data_type);
  TF_LITE_ENSURE_EQ(context, filter->type, data_type);

  if (hasBias) {
    bias = GetInput(context, node, kBiasTensor);
    if (data_type == kTfLiteUInt8) {
      TF_LITE_ENSURE_EQ(context, bias->type, kTfLiteInt32);
      TF_LITE_ENSURE_EQ(context, bias->params.zero_point, 0);
    } else {
      TF_LITE_ENSURE_EQ(context, bias->type, data_type);
    }
    TF_LITE_ENSURE_EQ(context, NumDimensions(bias), 1);
    TF_LITE_ENSURE_EQ(context, SizeOfDimension(filter, 3),
                      SizeOfDimension(bias, 0));
  }

  int channels_out = SizeOfDimension(filter, 3);
  int width = SizeOfDimension(input, 2);
  int height = SizeOfDimension(input, 1);
  int filter_width = SizeOfDimension(filter, 2);
  int filter_height = SizeOfDimension(filter, 1);
  int batches = SizeOfDimension(input, 0);

  // Matching GetWindowedOutputSize in TensorFlow.
  auto padding = params->padding;
  auto compute_out_size = [padding](int imageSize, int filterSize,
                                    int stride) -> int {
    return padding == kTfLitePaddingSame
               ? (imageSize + stride - 1) / stride
               : padding == kTfLitePaddingValid
                     ? (imageSize - filterSize + stride) / stride
                     : 0;
  };

  int out_width = compute_out_size(width, filter_width, params->stride_width);
  int out_height =
      compute_out_size(height, filter_height, params->stride_height);

  data->padding.height = ComputePadding(params->stride_height, 1, height,
                                        filter_height, out_height);
  data->padding.width =
      ComputePadding(params->stride_width, 1, width, filter_width, out_width);

  // Note that quantized inference requires that all tensors have their
  // parameters set. This is usually done during quantized training.
  if (data_type != kTfLiteFloat32) {
    double real_multiplier = 0.0;
    TF_LITE_ENSURE_STATUS(GetQuantizedConvolutionMultipler(
        context, input, filter, bias, output, &real_multiplier));
    QuantizeMultiplierSmallerThanOne(real_multiplier, &data->output_multiplier,
                                     &data->output_shift);
    CalculateActivationRangeUint8(params->activation, output,
                                  &data->output_activation_min,
                                  &data->output_activation_max);
  }

  TfLiteIntArray* outputSize = TfLiteIntArrayCreate(4);
  outputSize->data[0] = batches;
  outputSize->data[1] = out_height;
  outputSize->data[2] = out_width;
  outputSize->data[3] = channels_out;
  return context->ResizeTensor(context, output, outputSize);
}

template <KernelType kernel_type>
void EvalFloat(TfLiteContext* context, TfLiteNode* node,
               TfLiteDepthwiseConvParams* params, OpData* data,
               TfLiteTensor* input, TfLiteTensor* filter, TfLiteTensor* bias,
               TfLiteTensor* output) {
  float output_activation_min, output_activation_max;
  CalculateActivationRangeFloat(params->activation, &output_activation_min,
                                &output_activation_max);

  void (*depthwise_conv)(const float*, const Dims<4>&, const float*,
                         const Dims<4>&, const float*, const Dims<4>&, int, int,
                         int, int, int, float, float, float*, const Dims<4>&);
  if (kernel_type == kReference) {
    depthwise_conv = &reference_ops::DepthwiseConv;
  } else {
    depthwise_conv = &optimized_ops::DepthwiseConv;
  }

  depthwise_conv(
      GetTensorData<float>(input), GetTensorDims(input),
      GetTensorData<float>(filter), GetTensorDims(filter),
      GetTensorData<float>(bias), GetTensorDims(bias), params->stride_width,
      params->stride_height, data->padding.width, data->padding.height,
      params->depth_multiplier, output_activation_min, output_activation_max,
      GetTensorData<float>(output), GetTensorDims(output));
}

template <KernelType kernel_type>
void EvalQuantized(TfLiteContext* context, TfLiteNode* node,
                   TfLiteDepthwiseConvParams* params, OpData* data,
                   TfLiteTensor* input, TfLiteTensor* filter,
                   TfLiteTensor* bias, TfLiteTensor* output) {
  auto input_offset = -input->params.zero_point;
  auto filter_offset = -filter->params.zero_point;
  auto output_offset = output->params.zero_point;

  void (*depthwise_conv)(const uint8*, const Dims<4>&, int32, const uint8*,
                         const Dims<4>&, int32, const int32*, const Dims<4>&,
                         int, int, int, int, int, int32, int32, int, int32,
                         int32, uint8*, const Dims<4>&);
  if (kernel_type == kReference) {
    depthwise_conv = &reference_ops::DepthwiseConv;
  } else {
    depthwise_conv = &optimized_ops::DepthwiseConv;
  }

  depthwise_conv(
      GetTensorData<uint8_t>(input), GetTensorDims(input), input_offset,
      GetTensorData<uint8_t>(filter), GetTensorDims(filter), filter_offset,
      GetTensorData<int32_t>(bias), GetTensorDims(bias), params->stride_width,
      params->stride_height, data->padding.width, data->padding.height,
      params->depth_multiplier, output_offset, data->output_multiplier,
      data->output_shift, data->output_activation_min,
      data->output_activation_max, GetTensorData<uint8_t>(output),
      GetTensorDims(output));
}

template <KernelType kernel_type>
TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  auto* params =
      reinterpret_cast<TfLiteDepthwiseConvParams*>(node->builtin_data);
  OpData* data = reinterpret_cast<OpData*>(node->user_data);

  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);
  TfLiteTensor* input = GetInput(context, node, kInputTensor);
  TfLiteTensor* filter = GetInput(context, node, kFilterTensor);
  TfLiteTensor* bias =
      (NumInputs(node) == 3) ? GetInput(context, node, kBiasTensor) : nullptr;

  // TODO(aselle): Consider whether float conv and quantized conv should be
  // separate ops to avoid dispatch overhead here.
  switch (input->type) {  // Already know in/out types are same.
    case kTfLiteFloat32:
      EvalFloat<kernel_type>(context, node, params, data, input, filter, bias,
                             output);
      break;
    case kTfLiteUInt8:
      EvalQuantized<kernel_type>(context, node, params, data, input, filter,
                                 bias, output);
      break;
    default:
      context->ReportError(context, "Type not currently supported.");
      return kTfLiteError;
  }
  return kTfLiteOk;
}

}  // namespace depthwise_conv

TfLiteRegistration* Register_DEPTHWISE_CONVOLUTION_REF() {
  static TfLiteRegistration r = {
      depthwise_conv::Init, depthwise_conv::Free, depthwise_conv::Prepare,
      depthwise_conv::Eval<depthwise_conv::kReference>};
  return &r;
}

TfLiteRegistration* Register_DEPTHWISE_CONVOLUTION_GENERIC_OPT() {
  static TfLiteRegistration r = {
      depthwise_conv::Init, depthwise_conv::Free, depthwise_conv::Prepare,
      depthwise_conv::Eval<depthwise_conv::kGenericOptimized>};
  return &r;
}

TfLiteRegistration* Register_DEPTHWISE_CONVOLUTION_NEON_OPT() {
  static TfLiteRegistration r = {
      depthwise_conv::Init, depthwise_conv::Free, depthwise_conv::Prepare,
      depthwise_conv::Eval<depthwise_conv::kNeonOptimized>};
  return &r;
}

TfLiteRegistration* Register_DEPTHWISE_CONV_2D() {
#ifdef USE_NEON
  return Register_DEPTHWISE_CONVOLUTION_NEON_OPT();
#else
  return Register_DEPTHWISE_CONVOLUTION_GENERIC_OPT();
#endif
}

}  // namespace builtin
}  // namespace ops
}  // namespace tflite
