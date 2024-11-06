// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ospStudio.h"

// Plugin
#include <chrono>
#include <random>
#include "sg/importer/Importer.h"

using namespace rkcommon::math;
using namespace ospray::sg;
using rkcommon::make_unique;

class PixelHealThyselfContext : public StudioContext
{
 public:
  enum class CameraGenerator
  {
    FIBONACCI,
    RANDOM,
  };

  struct CameraGeneratorBase
  {
    struct CameraSample
    {
      vec3f pos;
      vec3f up;
      vec3f dir;
    };

    CameraGeneratorBase(box3f worldBounds_, int numSamples_)
        : worldBounds(worldBounds_),
          numSamples(numSamples_),
          startFrame(0),
          endFrame(-1),
          flipYZ(false),
          zoom(0.0f),
          jitter(0.0f) {};

    virtual ~CameraGeneratorBase() = default;

    virtual void Reset()
    {
      sampleIndex = std::min(std::max(0, startFrame), numSamples - 1);
    }

    virtual bool HasNext()
    {
      int endFrame = this->endFrame != -1 ? this->endFrame : numSamples;
      endFrame = std::min(endFrame, numSamples);
      return sampleIndex < endFrame;
    }

    CameraSample Next()
    {
      CameraSample sample = GenerateSample();
      sample = ApplyTransforms(sample);
      Advance();
      return sample;
    }

    box3f worldBounds;
    int numSamples{32};
    int startFrame{0};
    int endFrame{-1};
    bool flipYZ{false};
    float zoom{0.0f};
    float jitter{0.0f};

   protected:
    virtual CameraSample GenerateSample() = 0;

    virtual CameraSample ApplyTransforms(const CameraSample &sample)
    {
      return ApplyZoom(ApplyJitter(sample));
    }

    virtual CameraSample ApplyZoom(const CameraSample &sample)
    {
      CameraSample result = sample;
      if (zoom != 0.0f) {
        result.pos += zoom * result.dir;
      }
      return result;
    }

    virtual CameraSample ApplyJitter(const CameraSample &sample)
    {
      if (jitter == 0.0f) {
        return sample;
      }

      static std::random_device rd;
      static std::mt19937 gen(rd());
      static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

      vec3f jitterVec{dis(gen), dis(gen), dis(gen)};
      jitterVec = normalize(jitterVec);
      jitterVec *= jitter * worldBounds.size();
      CameraSample result = sample;
      result.pos += jitterVec;
      return result;
    }

    virtual void Advance()
    {
      sampleIndex++;
    }

    int sampleIndex{0};
  };

  PixelHealThyselfContext(StudioCommon &studioCommon);
  ~PixelHealThyselfContext() {}

  void start() override;
  void addToCommandLine(std::shared_ptr<CLI::App> app) override;
  bool parseCommandLine() override;
  void importFiles(sg::NodePtr world) override;
  void refreshScene(bool resetCam) override;
  void updateCamera() override;
  void selectCamera() override {};
  void loadCamJson() override {};

 protected:
  void updateRenderer();
  void preRender();
  void renderFrame();
  void saveFrame();

  std::shared_ptr<sg::FrameBuffer> framebuffer = nullptr;

  bool optForceOverwrite{false};
  CameraGenerator optCameraGenerator{CameraGenerator::FIBONACCI};
  bool optCameraGeneratorFlipYZ;
  int optNumFrames{32};
  int optStartFrame{0};
  int optEndFrame{-1};
  float optJitter{0.0f};
  float optZoom{0.0f};
  std::string optOutputPath;

  NodePtr importedModels;
  std::shared_ptr<CameraGeneratorBase> cameraGenerator;

 private:
  vec3f camPos;
  vec3f camUp;
  vec3f camDir;
};
