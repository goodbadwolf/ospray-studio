// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TransferFunction.h"

namespace ospray {
namespace sg {

TransferFunction::TransferFunction(const std::string &osp_type)
{
  setValue(cpp::TransferFunction(osp_type));

  createChild("value", "range1f", range1f(0.f, 1.f));

  setColorsAndOpacties({vec3f(0.f), vec3f(1.f)}, {0.f, 1.f});
  setColorPointsAndOpacityPoints(
      {vec4f(0.0f), vec4f(1.0f)}, {vec2f(0.0f), vec2f(1.0f)});
}

void TransferFunction::setColorsAndOpacties(
    const std::vector<vec3f> &colors, const std::vector<float> &opacities)
{
  this->colors = colors;
  this->opacities = opacities;
  createChildData("color", this->colors);
  createChildData("opacity", this->opacities);
}

void TransferFunction::setColorPointsAndOpacityPoints(
    const std::vector<vec4f> &colorPoints,
    const std::vector<vec2f> &opacityPoints)

{
  this->colorPoints = colorPoints;
  this->opacityPoints = opacityPoints;
  createChildData("colorPoints", this->colorPoints);
  createChildData("opacityPoints", this->opacityPoints);
}

} // namespace sg
} // namespace ospray
