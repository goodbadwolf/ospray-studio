// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../../Node.h"

namespace ospray {
namespace sg {

struct OSPSG_INTERFACE TransferFunction
    : public OSPNode<cpp::TransferFunction, NodeType::TRANSFER_FUNCTION>
{
  TransferFunction(const std::string &osp_type);
  ~TransferFunction() override = default;

  void setColorPointsAndOpacityPoints(const std::vector<vec4f> &colorPoints,
      const std::vector<vec2f> &opacityPoints);

  void setColorsAndOpacties(
      const std::vector<vec3f> &colors, const std::vector<float> &opacities);

  std::vector<vec4f> colorPoints;
  std::vector<vec2f> opacityPoints;
  std::vector<vec3f> colors;
  std::vector<float> opacities;

 protected:
  inline void initOpacities()
  {
    // Initialize to a simple ramp
    auto numSamples = colors.size();
    opacities.resize(numSamples);
    for (auto i = 0; i < numSamples; i++)
      opacities.at(i) = (float)i / (numSamples - 1);
  }
};

} // namespace sg
} // namespace ospray
