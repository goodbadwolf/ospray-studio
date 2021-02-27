// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Renderer.h"

namespace ospray {
namespace sg {

struct OSPSG_INTERFACE PathTracer : public Renderer
{
  PathTracer();
  virtual ~PathTracer() override = default;
};

OSP_REGISTER_SG_NODE_NAME(PathTracer, renderer_pathtracer);

// PathTracer definitions ///////////////////////////////////////////////////

PathTracer::PathTracer() : Renderer("pathtracer")
{
  createChild("lightSamples",
      "int",
      "number of random light samples per path vertex",
      -1);
  createChild("roulettePathLength",
      "int",
      "ray recursion depth at which to start roulette termination",
      5);
  createChild("maxContribution",
      "float",
      "clamped value for samples accumulated into the framebuffer",
      1e6f);
  createChild("geometryLights",
      "bool",
      "whether geometries with an emissive material illuminate the scene",
      true);

  child("lightSamples").setMinMax(-1, 1000);
  child("roulettePathLength").setMinMax(0, 1000);
  child("maxContribution").setMinMax(0.f, 1e6f);
}

} // namespace sg
} // namespace ospray
