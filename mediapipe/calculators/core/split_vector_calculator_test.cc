// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mediapipe/calculators/core/split_vector_calculator.h"

#include <memory>
#include <vector>

#include "mediapipe/calculators/core/split_vector_calculator.pb.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/calculator_runner.h"
#include "mediapipe/framework/port/gmock.h"
#include "mediapipe/framework/port/gtest.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status_matchers.h"  // NOLINT
#include "mediapipe/framework/tool/validate_type.h"

namespace mediapipe {

typedef SplitVectorCalculator<std::unique_ptr<int>, true>
    MovableSplitUniqueIntPtrCalculator;
REGISTER_CALCULATOR(MovableSplitUniqueIntPtrCalculator);

class MovableSplitUniqueIntPtrCalculatorTest : public ::testing::Test {
 protected:
  void ValidateVectorOutput(std::vector<Packet>& output_packets,
                            int expected_elements, int input_begin_index) {
    ASSERT_EQ(1, output_packets.size());
    const std::vector<std::unique_ptr<int>>& output_vec =
        output_packets[0].Get<std::vector<std::unique_ptr<int>>>();
    ASSERT_EQ(expected_elements, output_vec.size());

    for (int i = 0; i < expected_elements; ++i) {
      const int expected_value = input_begin_index + i;
      const std::unique_ptr<int>& result = output_vec[i];
      ASSERT_NE(result, nullptr);
      ASSERT_EQ(expected_value, *result);
    }
  }

  void ValidateElementOutput(std::vector<Packet>& output_packets,
                             int expected_value) {
    ASSERT_EQ(1, output_packets.size());
    const std::unique_ptr<int>& result =
        output_packets[0].Get<std::unique_ptr<int>>();
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(expected_value, *result);
  }

  void ValidateCombinedVectorOutput(std::vector<Packet>& output_packets,
                                    int expected_elements,
                                    std::vector<int>& input_begin_indices,
                                    std::vector<int>& input_end_indices) {
    ASSERT_EQ(1, output_packets.size());
    ASSERT_EQ(input_begin_indices.size(), input_end_indices.size());
    const std::vector<std::unique_ptr<int>>& output_vector =
        output_packets[0].Get<std::vector<std::unique_ptr<int>>>();
    ASSERT_EQ(expected_elements, output_vector.size());
    const int num_ranges = input_begin_indices.size();

    int element_id = 0;
    for (int range_id = 0; range_id < num_ranges; ++range_id) {
      for (int i = input_begin_indices[range_id];
           i < input_end_indices[range_id]; ++i) {
        const int expected_value = i;
        const std::unique_ptr<int>& result = output_vector[element_id];
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(expected_value, *result);
        ++element_id;
      }
    }
  }
};

TEST_F(MovableSplitUniqueIntPtrCalculatorTest, InvalidOverlappingRangesTest) {
  CalculatorGraphConfig graph_config =
      mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(
          R"pb(
            input_stream: "input_vector"
            node {
              calculator: "MovableSplitUniqueIntPtrCalculator"
              input_stream: "input_vector"
              output_stream: "range_0"
              options {
                [mediapipe.SplitVectorCalculatorOptions.ext] {
                  ranges: { begin: 0 end: 3 }
                  ranges: { begin: 1 end: 4 }
                }
              }
            }
          )pb");

  CalculatorGraph graph;
  ASSERT_FALSE(graph.Initialize(graph_config).ok());
}

TEST_F(MovableSplitUniqueIntPtrCalculatorTest, SmokeTest) {
  CalculatorGraphConfig graph_config =
      mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(
          R"pb(
            input_stream: "input_vector"
            node {
              calculator: "MovableSplitUniqueIntPtrCalculator"
              input_stream: "input_vector"
              output_stream: "range_0"
              output_stream: "range_1"
              output_stream: "range_2"
              options {
                [mediapipe.SplitVectorCalculatorOptions.ext] {
                  ranges: { begin: 0 end: 1 }
                  ranges: { begin: 1 end: 4 }
                  ranges: { begin: 4 end: 5 }
                }
              }
            }
          )pb");

  std::vector<Packet> range_0_packets;
  tool::AddVectorSink("range_0", &graph_config, &range_0_packets);
  std::vector<Packet> range_1_packets;
  tool::AddVectorSink("range_1", &graph_config, &range_1_packets);
  std::vector<Packet> range_2_packets;
  tool::AddVectorSink("range_2", &graph_config, &range_2_packets);

  CalculatorGraph graph;
  MP_ASSERT_OK(graph.Initialize(graph_config));
  MP_ASSERT_OK(graph.StartRun({}));

  std::unique_ptr<std::vector<std::unique_ptr<int>>> input_vector =
      absl::make_unique<std::vector<std::unique_ptr<int>>>(6);
  for (int i = 0; i < 6; ++i) {
    input_vector->at(i) = absl::make_unique<int>(i);
  }

  MP_ASSERT_OK(graph.AddPacketToInputStream(
      "input_vector", Adopt(input_vector.release()).At(Timestamp(1))));

  MP_ASSERT_OK(graph.WaitUntilIdle());
  MP_ASSERT_OK(graph.CloseAllPacketSources());
  MP_ASSERT_OK(graph.WaitUntilDone());

  ValidateVectorOutput(range_0_packets, /*expected_elements=*/1,
                       /*input_begin_index=*/0);
  ValidateVectorOutput(range_1_packets, /*expected_elements=*/3,
                       /*input_begin_index=*/1);
  ValidateVectorOutput(range_2_packets, /*expected_elements=*/1,
                       /*input_begin_index=*/4);
}

TEST_F(MovableSplitUniqueIntPtrCalculatorTest, SmokeTestElementOnly) {
  CalculatorGraphConfig graph_config =
      mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(
          R"pb(
            input_stream: "input_vector"
            node {
              calculator: "MovableSplitUniqueIntPtrCalculator"
              input_stream: "input_vector"
              output_stream: "range_0"
              output_stream: "range_1"
              output_stream: "range_2"
              options {
                [mediapipe.SplitVectorCalculatorOptions.ext] {
                  ranges: { begin: 0 end: 1 }
                  ranges: { begin: 2 end: 3 }
                  ranges: { begin: 4 end: 5 }
                  element_only: true
                }
              }
            }
          )pb");

  std::vector<Packet> range_0_packets;
  tool::AddVectorSink("range_0", &graph_config, &range_0_packets);
  std::vector<Packet> range_1_packets;
  tool::AddVectorSink("range_1", &graph_config, &range_1_packets);
  std::vector<Packet> range_2_packets;
  tool::AddVectorSink("range_2", &graph_config, &range_2_packets);

  CalculatorGraph graph;
  MP_ASSERT_OK(graph.Initialize(graph_config));
  MP_ASSERT_OK(graph.StartRun({}));

  std::unique_ptr<std::vector<std::unique_ptr<int>>> input_vector =
      absl::make_unique<std::vector<std::unique_ptr<int>>>(6);
  for (int i = 0; i < 6; ++i) {
    input_vector->at(i) = absl::make_unique<int>(i);
  }

  MP_ASSERT_OK(graph.AddPacketToInputStream(
      "input_vector", Adopt(input_vector.release()).At(Timestamp(1))));

  MP_ASSERT_OK(graph.WaitUntilIdle());
  MP_ASSERT_OK(graph.CloseAllPacketSources());
  MP_ASSERT_OK(graph.WaitUntilDone());

  ValidateElementOutput(range_0_packets, /*expected_value=*/0);
  ValidateElementOutput(range_1_packets, /*expected_value=*/2);
  ValidateElementOutput(range_2_packets, /*expected_value=*/4);
}

TEST_F(MovableSplitUniqueIntPtrCalculatorTest, SmokeTestCombiningOutputs) {
  CalculatorGraphConfig graph_config =
      mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(
          R"pb(
            input_stream: "input_vector"
            node {
              calculator: "MovableSplitUniqueIntPtrCalculator"
              input_stream: "input_vector"
              output_stream: "range_0"
              options {
                [mediapipe.SplitVectorCalculatorOptions.ext] {
                  ranges: { begin: 0 end: 1 }
                  ranges: { begin: 2 end: 3 }
                  ranges: { begin: 4 end: 5 }
                  combine_outputs: true
                }
              }
            }
          )pb");

  std::vector<Packet> range_0_packets;
  tool::AddVectorSink("range_0", &graph_config, &range_0_packets);

  CalculatorGraph graph;
  MP_ASSERT_OK(graph.Initialize(graph_config));
  MP_ASSERT_OK(graph.StartRun({}));

  std::unique_ptr<std::vector<std::unique_ptr<int>>> input_vector =
      absl::make_unique<std::vector<std::unique_ptr<int>>>(6);
  for (int i = 0; i < 6; ++i) {
    input_vector->at(i) = absl::make_unique<int>(i);
  }

  MP_ASSERT_OK(graph.AddPacketToInputStream(
      "input_vector", Adopt(input_vector.release()).At(Timestamp(1))));

  MP_ASSERT_OK(graph.WaitUntilIdle());
  MP_ASSERT_OK(graph.CloseAllPacketSources());
  MP_ASSERT_OK(graph.WaitUntilDone());

  std::vector<int> input_begin_indices = {0, 2, 4};
  std::vector<int> input_end_indices = {1, 3, 5};
  ValidateCombinedVectorOutput(range_0_packets, /*expected_elements=*/3,
                               input_begin_indices, input_end_indices);
}

}  // namespace mediapipe
