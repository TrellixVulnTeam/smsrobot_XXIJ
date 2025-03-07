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
#include "tensorflow/contrib/lite/kernels/activation_functor.h"
#include "tensorflow/contrib/lite/kernels/internal/kernel_utils.h"
#include "tensorflow/contrib/lite/kernels/op_macros.h"

namespace tflite {
namespace ops {
namespace builtin {
namespace unidirectional_sequence_rnn {

constexpr int kInputTensor = 0;
constexpr int kWeightsTensor = 1;
constexpr int kRecurrentWeightsTensor = 2;
constexpr int kBiasTensor = 3;
constexpr int kHiddenStateTensor = 0;
constexpr int kOutputTensor = 1;

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  // Check we have all the inputs and outputs we need.
  TF_LITE_ENSURE_EQ(context, node->inputs->size, 4);
  TF_LITE_ENSURE_EQ(context, node->outputs->size, 2);

  TfLiteTensor* input = &context->tensors[node->inputs->data[kInputTensor]];
  TfLiteTensor* input_weights =
      &context->tensors[node->inputs->data[kWeightsTensor]];
  TfLiteTensor* recurrent_weights =
      &context->tensors[node->inputs->data[kRecurrentWeightsTensor]];
  TfLiteTensor* bias = &context->tensors[node->inputs->data[kBiasTensor]];

  // Check all the parameters of tensor match within themselves and match the
  // input configuration.
  auto* params = reinterpret_cast<TfLiteSequenceRNNParams*>(node->builtin_data);
  const bool time_major = params->time_major;
  const int batch_size =
      (time_major) ? input->dims->data[1] : input->dims->data[0];
  const int max_time =
      (time_major) ? input->dims->data[0] : input->dims->data[1];
  const int num_units = input_weights->dims->data[0];
  TF_LITE_ASSERT_EQ(input->dims->data[2], input_weights->dims->data[1]);
  TF_LITE_ASSERT_EQ(input_weights->dims->data[0], bias->dims->data[0]);
  TF_LITE_ASSERT_EQ(recurrent_weights->dims->data[0], bias->dims->data[0]);
  TF_LITE_ASSERT_EQ(recurrent_weights->dims->data[1], bias->dims->data[0]);

  TfLiteTensor* hidden_state =
      &context->tensors[node->outputs->data[kHiddenStateTensor]];
  TfLiteTensor* output = &context->tensors[node->outputs->data[kOutputTensor]];

  // Resize state.
  TfLiteIntArray* hidden_state_size_array = TfLiteIntArrayCreate(2);
  hidden_state_size_array->data[0] = batch_size;
  hidden_state_size_array->data[1] = num_units;
  TF_LITE_ENSURE_OK(context, context->ResizeTensor(context, hidden_state,
                                                   hidden_state_size_array));

  // Mark hidden state as a persistent tensor.
  hidden_state->allocation_type = kTfLiteArenaRwPersistent;

  // Resize output.
  TfLiteIntArray* output_size_array = TfLiteIntArrayCreate(3);
  output_size_array->data[0] = (time_major) ? max_time : batch_size;
  output_size_array->data[1] = (time_major) ? batch_size : max_time;
  output_size_array->data[2] = num_units;
  TF_LITE_ENSURE_OK(context,
                    context->ResizeTensor(context, output, output_size_array));

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  auto* params = reinterpret_cast<TfLiteSequenceRNNParams*>(node->builtin_data);

  TfLiteTensor* input = &context->tensors[node->inputs->data[kInputTensor]];
  TfLiteTensor* input_weights =
      &context->tensors[node->inputs->data[kWeightsTensor]];
  TfLiteTensor* recurrent_weights =
      &context->tensors[node->inputs->data[kRecurrentWeightsTensor]];
  TfLiteTensor* bias = &context->tensors[node->inputs->data[kBiasTensor]];
  TfLiteTensor* hidden_state =
      &context->tensors[node->outputs->data[kHiddenStateTensor]];
  TfLiteTensor* output = &context->tensors[node->outputs->data[kOutputTensor]];

  // Initialize the pointer bias.
  const float* bias_ptr = bias->data.f;

  const bool time_major = params->time_major;
  const int batch_size =
      (time_major) ? input->dims->data[1] : input->dims->data[0];
  const int max_time =
      (time_major) ? input->dims->data[0] : input->dims->data[1];
  const int num_units = input_weights->dims->data[0];
  const int input_size = input->dims->data[2];

  // Initialize input_weights and recurrent_weights.
  const float* input_weights_ptr = input_weights->data.f;
  const float* recurrent_weights_ptr = recurrent_weights->data.f;

  if (time_major) {
    // Initialize the pointer to hidden state.
    float* hidden_state_ptr_batch = hidden_state->data.f;
    // Unroll the sequence and use batch batch operations for efficiency.
    for (int s = 0; s < max_time; s++) {
      // Initialize the pointer to input and output.
      const float* input_ptr_batch =
          input->data.f + s * input_size * batch_size;
      float* output_ptr_batch = output->data.f + s * num_units * batch_size;

      kernel_utils::RnnBatchStep(input_ptr_batch, input_weights_ptr,
                                 recurrent_weights_ptr, bias_ptr, input_size,
                                 num_units, batch_size, params->activation,
                                 hidden_state_ptr_batch, output_ptr_batch);
    }
  } else {
    // For each batch
    for (int b = 0; b < batch_size; b++) {
      // Initialize the pointer to hidden state.
      float* hidden_state_ptr_batch = hidden_state->data.f + b * num_units;
      for (int s = 0; s < max_time; s++) {
        // Initialize the pointer to input and output.
        const float* input_ptr_batch =
            input->data.f + b * input_size * max_time + s * input_size;
        float* output_ptr_batch =
            output->data.f + b * num_units * max_time + s * num_units;

        kernel_utils::RnnBatchStep(
            input_ptr_batch, input_weights_ptr, recurrent_weights_ptr, bias_ptr,
            input_size, num_units, /*batch_size=*/1, params->activation,
            hidden_state_ptr_batch, output_ptr_batch);
      }
    }
  }
  return kTfLiteOk;
}

}  // namespace unidirectional_sequence_rnn

TfLiteRegistration* Register_UNIDIRECTIONAL_SEQUENCE_RNN() {
  static TfLiteRegistration r = {/*init=*/nullptr, /*free=*/nullptr,
                                 unidirectional_sequence_rnn::Prepare,
                                 unidirectional_sequence_rnn::Eval};
  return &r;
}

}  // namespace builtin
}  // namespace ops
}  // namespace tflite
