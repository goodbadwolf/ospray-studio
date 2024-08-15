// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "PHTContext.h"
// ospray_sg
#include "sg/Frame.h"
#include "sg/Mpi.h"
#include "sg/camera/Camera.h"
#include "sg/fb/FrameBuffer.h"
#include "sg/renderer/MaterialRegistry.h"
#include "sg/scene/volume/Volume.h"
#include "sg/visitors/Commit.h"
#include "sg/visitors/PrintNodes.h"

// rkcommon
#include "rkcommon/utility/SaveImage.h"
// json
#include "PluginManager.h"
#include "sg/JSONDefs.h"

// CLI
#include <CLI11.hpp>

struct FibonacciLatticeCameraGenerator
    : public PixelHealThyselfContext::CameraGeneratorBase
{
  FibonacciLatticeCameraGenerator(box3f worldBounds_, int numSamples_)
      : CameraGeneratorBase(worldBounds_, numSamples_)
  {}

  void Reset() override
  {
    CameraGeneratorBase::Reset();
    worldDiagonal = length(worldBounds.size());
    epsilon = CalculateEpsilon(numSamples);
  }

 protected:
  CameraSample GenerateSample() override
  {
    // Based on
    // https://web.archive.org/web/20200608045615/https://extremelearning.com.au/how-to-evenly-distribute-points-on-a-sphere-more-effectively-than-the-canonical-fibonacci-lattice/

    static const float goldenRatio = 1 + sqrt(5.0f) / 2;
    float theta = 2.0f * M_PI * sampleIndex / goldenRatio;
    float phi = std::acos(
        1.0f - 2.0f * (sampleIndex + epsilon) / (numSamples - 1 + 2 * epsilon));
    float x = std::cos(theta) * std::sin(phi);
    float y = std::sin(theta) * std::sin(phi);
    float z = std::cos(phi);
    if (flipYZ) {
      std::swap(y, z);
    }

    vec3f camPos = worldBounds.center() + vec3f(x, y, z) * worldDiagonal;
    vec3f camDir =
        rkcommon::math::safe_normalize(worldBounds.center() - camPos);

    vec3f up(0.0, 1.0f, 0.0f);
    vec3f right =
        rkcommon::math::safe_normalize(rkcommon::math::cross(camDir, up));
    vec3f camUp = rkcommon::math::cross(right, camDir);

    return {camPos, camUp, camDir};
  }

  float CalculateEpsilon(int numSamples)
  {
    float epsilon;
    if (numSamples >= 600000)
      epsilon = 214;
    else if (numSamples >= 400000)
      epsilon = 75;
    else if (numSamples >= 11000)
      epsilon = 27;
    else if (numSamples >= 890)
      epsilon = 10;
    else if (numSamples >= 177)
      epsilon = 3.33f;
    else if (numSamples >= 24)
      epsilon = 1.33f;
    else
      epsilon = 0.33f;
    return epsilon;
    // Approximate calculation of epsilon based on curve fitting
    /*
    float logN = std::log(static_cast<float>(numSamples));
    return std::exp(0.009438f * std::pow(logN, 3)
        - 0.189699f * std::pow(logN, 2) + 1.831931f * logN - 4.313769f);
    */
  }

  float worldDiagonal;
  float epsilon;
};

PixelHealThyselfContext::PixelHealThyselfContext(StudioCommon &_common)
    : StudioContext(_common, StudioMode::PIXEL_HEAL_THYSELF)
{
  pluginManager = std::make_shared<PluginManager>();
  // Default saved image baseName (cmdline --image to override)
  optImageName = "ospPHT";
}

void PixelHealThyselfContext::start()
{
  std::cerr << "Pxel Heal Thyself mode started\n";

  // load plugins
  for (auto &p : studioCommon.pluginsToLoad)
    pluginManager->loadPlugin(p);
  pluginManager->main(shared_from_this());

  // set camera correctly to Id set externally via JSON or plugins:
  auto camera = frame->child("camera").nodeAs<sg::Camera>();

  if (!parseCommandLine()) {
    std::cerr
        << "Failed to parse command line args correctly or nothing to do. Exiting....\n";
    return;
  }

  updateRenderer();
  refreshScene(true);

  cameraGenerator = std::make_shared<FibonacciLatticeCameraGenerator>(
      getSceneBounds(), optNumFrames);
  cameraGenerator->zoom = optZoom;
  cameraGenerator->jitter = optJitter;
  cameraGenerator->flipYZ = optCameraGeneratorFlipYZ;
  cameraGenerator->Reset();

  while (cameraGenerator->HasNext()) {
    auto cameraSample = cameraGenerator->Next();
    camera->child("position").setValue(cameraSample.pos);
    camera->child("direction").setValue(cameraSample.dir);
    camera->child("up").setValue(cameraSample.up);

    updateCamera();
    preRender();
    renderFrame();
  }

  sg::clearAssets();
}

void PixelHealThyselfContext::addToCommandLine(std::shared_ptr<CLI::App> app)
{
  static std::map<std::string, PixelHealThyselfContext::CameraGenerator>
      cameraGeneratorMap = {
          {
              "fibonacci",
              CameraGenerator::FIBONACCI,
          },
          {
              "random",
              CameraGenerator::RANDOM,
          },
      };

  app->add_option("--cameraGenerator",
         optCameraGenerator,
         "Camera sample generator to use")
      ->transform(
          CLI::CheckedTransformer(cameraGeneratorMap, CLI::ignore_case));

  app->add_flag("--cameraGeneratorFlipYZ",
      optCameraGeneratorFlipYZ,
      "Flip Y and Z axes for camera samples");

  app->add_option("--numFrames", optNumFrames, "Number of frames to generate")
      ->check(CLI::PositiveNumber);

  app->add_option("--jitter", optJitter, "Jitter amount for camera samples")
      ->check(CLI::Number);

  app->add_option("--zoom", optZoom, "Zoom amount for camera samples")
      ->check(CLI::Number);

  app->add_flag("--forceOverwrite",
      optForceOverwrite,
      "Force overwriting saved files if they exist");
}

bool PixelHealThyselfContext::parseCommandLine()
{
  int ac = studioCommon.argc;
  const char **av = studioCommon.argv;

  std::shared_ptr<CLI::App> app =
      std::make_shared<CLI::App>("OSPRay Studio Pixel Heal Thyself");

  StudioContext::addToCommandLine(app);
  PixelHealThyselfContext::addToCommandLine(app);
  try {
    app->parse(ac, av);
  } catch (const CLI::ParseError &e) {
    app->exit(e);
    return false;
  }

  if (filesToImport.size() == 0) {
    std::cout << "No files to import " << std::endl;
    return false;
  } else {
    return true;
  }
}

void PixelHealThyselfContext::updateRenderer()
{
  frame->createChild("renderer", "renderer_" + optRendererTypeStr);
  auto &r = frame->childAs<sg::Renderer>("renderer");

  r["pixelFilter"] = optPF;
  r["backgroundColor"] = optBackGroundColor;
  r["pixelSamples"] = optSPP;
  r["varianceThreshold"] = optVariance;
  if (r.hasChild("maxContribution") && maxContribution < (float)math::inf)
    r["maxContribution"].setValue(maxContribution);

  auto fSize = frame->child("windowSize").valueAs<vec2i>();
  frame->child("camera")["aspect"] = optResolution.x / (float)optResolution.y;
  frame->child("windowSize") = fSize;
  frame->currentAccum = 0;
}

void PixelHealThyselfContext::preRender()
{
  auto &frameBuffer = frame->childAs<sg::FrameBuffer>("framebuffer");
  frameBuffer["floatFormat"] = true;
  frameBuffer.commit();

  frame->child("world").createChild("materialref", "reference_to_material", 0);
  frame->child("navMode") = false;
}

void PixelHealThyselfContext::renderFrame()
{
  frame->immediatelyWait = true;

  auto &frameBuffer = frame->childAs<sg::FrameBuffer>("framebuffer");
  auto &v = frame->childAs<sg::Renderer>("renderer")["varianceThreshold"];
  auto varianceThreshold = v.valueAs<float>();
  float fbVariance{inf};

  // continue accumulation till variance threshold or accumulation limit is
  // reached
  do {
    frame->startNewFrame();
    fbVariance = frameBuffer.variance();
    std::cout << "frame " << frame->currentAccum << " ";
    std::cout << "variance " << fbVariance << std::endl;
  } while (fbVariance >= varianceThreshold && !frame->accumLimitReached());

  static int filenum = 0;
  if (!sgUsingMpi() || sgMpiRank() == 0) {
    std::string filename;
    char filenumber[8];
    if (!optForceOverwrite) {
      int filenum_ = filenum;
      do {
        std::snprintf(filenumber, 8, ".%04d.", filenum_++);
        filename = optImageName + filenumber + optImageFormat;
      } while (std::ifstream(filename.c_str()).good());
    } else {
      std::snprintf(filenumber, 8, ".%04d.", filenum);
      filename = optImageName + filenumber + optImageFormat;
    }
    filenum += 1;

    int screenshotFlags = optSaveLayersSeparately << 3 | optSaveNormal << 2
        | optSaveDepth << 1 | optSaveAlbedo;

    frame->saveFrame(filename, screenshotFlags);

    this->outputFilename = filename;
  }
}

void PixelHealThyselfContext::refreshScene(bool resetCam)
{
  if (frameAccumLimit)
    frame->accumLimit = frameAccumLimit;
  else if (optVariance)
    frame->accumLimit = 0;
  else
    frame->accumLimit = 1;
  // Check that the frame contains a world, if not create one
  auto world = frame->hasChild("world") ? frame->childNodeAs<sg::Node>("world")
                                        : sg::createNode("world", "world");
  if (optSceneConfig == "dynamic")
    world->child("dynamicScene").setValue(true);
  else if (optSceneConfig == "compact")
    world->child("compactMode").setValue(true);
  else if (optSceneConfig == "robust")
    world->child("robustMode").setValue(true);
  world->createChild(
      "materialref", "reference_to_material", defaultMaterialIdx);

  if (!filesToImport.empty())
    importFiles(world);

  if (world->isModified()) {
    // Cancel any in-progress frame as world->render() will modify live device
    // parameters
    frame->cancelFrame();
    frame->waitOnFrame();
    world->render();
  }

  frame->add(world);

  auto &frameBuffer = frame->childAs<sg::FrameBuffer>("framebuffer");
  frameBuffer.resetAccumulation();

  frame->child("windowSize") = optResolution;
}

void PixelHealThyselfContext::updateCamera()
{
  frame->currentAccum = 0;
}

void PixelHealThyselfContext::importFiles(sg::NodePtr world)
{
  importedModels = createNode("importXfm", "transform");
  frame->child("world").add(importedModels);

  for (auto file : filesToImport) {
    try {
      rkcommon::FileName fileName(file);
      if (fileName.ext() == "sg") {
        importScene(shared_from_this(), fileName);
        sgScene = true;
      } else {
        std::cout << "Importing: " << file << std::endl;

        auto importer = sg::getImporter(world, file);
        if (importer) {
          if (volumeParams->children().size() > 0) {
            std::cout << "Using command-line volume parameters ..."
                      << std::endl;
            auto vp = importer->getVolumeParams();
            for (auto &c : volumeParams->children()) {
              vp->remove(c.first);
              vp->add(c.second);
            }
          }
          // Could be any type of importer.  Need to pass the
          // MaterialRegistry, importer will use what it needs.
          importer->setFb(frame->childAs<sg::FrameBuffer>("framebuffer"));
          importer->setMaterialRegistry(baseMaterialRegistry);
          importer->setLightsManager(lightsManager);
          importer->setArguments(studioCommon.argc, (char **)studioCommon.argv);
          importer->setScheduler(scheduler);
          importer->setAnimationList(animationManager->getAnimations());
          if (optInstanceConfig == "dynamic")
            importer->setInstanceConfiguration(
                sg::InstanceConfiguration::DYNAMIC);
          else if (optInstanceConfig == "compact")
            importer->setInstanceConfiguration(
                sg::InstanceConfiguration::COMPACT);
          else if (optInstanceConfig == "robust")
            importer->setInstanceConfiguration(
                sg::InstanceConfiguration::ROBUST);

          importer->importScene();
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Failed to open file '" << file << "'!\n";
      std::cerr << "   " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Failed to open file '" << file << "'!\n";
    }

    if (!optDoAsyncTasking) {
      for (;;) {
        size_t numTasksExecuted = 0;

        numTasksExecuted += scheduler->background()->executeAllTasksSync();
        numTasksExecuted += scheduler->ospray()->executeAllTasksSync();
        numTasksExecuted += scheduler->studio()->executeAllTasksSync();

        if (numTasksExecuted == 0) {
          break;
        }
      }
    }
  }

  for (;;) {
    size_t numTasksExecuted = 0;

    if (optDoAsyncTasking) {
      numTasksExecuted += scheduler->background()->executeAllTasksAsync();

      if (numTasksExecuted == 0) {
        if (scheduler->background()->wait() > 0) {
          continue;
        }
      }
    } else {
      numTasksExecuted += scheduler->background()->executeAllTasksSync();
    }

    numTasksExecuted += scheduler->ospray()->executeAllTasksSync();
    numTasksExecuted += scheduler->studio()->executeAllTasksSync();

    if (numTasksExecuted == 0) {
      break;
    }
  }

  filesToImport.clear();

  // Initializes time range for newly imported models
  animationManager->init();
}
