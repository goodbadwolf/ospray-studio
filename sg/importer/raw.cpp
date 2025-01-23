// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Importer.h"
// ospcommon
#include "../scene/volume/Structured.h"
#include "../scene/volume/StructuredSpherical.h"
#include "rkcommon/os/FileName.h"

namespace ospray {
namespace sg {

static std::unordered_map<std::string, OSPDataType> const rawVolumeVoxelType = {
    {"float32", OSP_FLOAT},
    {"float64", OSP_DOUBLE},
    {"int8", OSP_CHAR},
    {"int16", OSP_SHORT},
    {"int32", OSP_INT},
    {"uint8", OSP_UCHAR},
    {"uint16", OSP_USHORT},
    {"uint32", OSP_UINT}};

struct RawImporter : public Importer
{
  RawImporter() = default;
  ~RawImporter() override = default;

  void importScene() override;
};

OSP_REGISTER_SG_NODE_NAME(RawImporter, importer_raw);

// rawImporter definitions /////////////////////////////////////////////

void RawImporter::importScene()
{
  using namespace std::string_literals;

  // Keep this object alive for the duration of any lambdas
  auto self = shared_from_this();

  auto loadDataCallback = [&, self](SchedulerPtr scheduler) {
    // Create a root Transform/Instance off the Importer, then place the volume
    // under this.
    auto rootName = fileName.name() + "_rootXfm";
    auto nodeName = fileName.name() + "_volume";

    auto last = fileName.base().find_last_of(".");
    auto volumeTypeExt = fileName.base().substr(last, fileName.base().length());

    auto rootNode = createNode(rootName, "transform");
    NodePtr volume;

    bool isSpherical = volumeTypeExt == ".spherical";

    if (isSpherical) {
      volume = createNode(nodeName, "structuredSpherical");
    } else {
      volume = createNode(nodeName, "structuredRegular");
    }

    for (auto &c : volumeParams->children()) {
      // Need to make a copy of the volume parameter here. If multiple threads
      // are using the same VolumeParams children objects, then because of the
      // book keeping involved with a node remembering its parents, multiple
      // threads could modify the parents vector within a Node object.
      //
      // Example: Threads "foo" and "bar" are running at the same time.
      // First, Foo adds a VolumeParams child to its own Volume object. Foo
      // recognizes that it will need to resize the Node::properties::parents
      // vector. Foo allocates a new parents vector. Next, Bar follows the
      // same process and allocates a new parents vector. Foo deallocates the
      // old pointer and so does Bar, leading to a double-free.
      //
      // The actual reason this happens is because although the
      // Importer::volumeParams object is newly created each time
      // Importer::getImporter() is called, the children of each of the
      // separate Importer::volumeParams objects are all references to the
      // exact same Node object.

      // Preferably this code would be something like:
      //   volume->add(createNodeLike(c.second))
      auto &p = c.second;
      volume->createChild(p->name(), p->subType(), p->description(), p->value());
    }

    std::cout << "Attempting to parse dimensions from " << fileName.str()
              << std::endl;
    // The file name is expected to be in the format:
    //   <name>_XxYxZ_<type>.raw
    // where X, Y, and Z are the dimensions of the volume
    auto name = fileName.base();
    // Last underscore before the extension
    auto lastUnderscore = name.find_last_of("_");
    if (lastUnderscore == std::string::npos) {
      throw std::runtime_error("Invalid file name: " + name);
    }
    // Second to last underscore before the extension
    auto secondToLastUnderscore = name.find_last_of("_", lastUnderscore - 1);

    // Everything between the second to last underscore and the last underscore
    // is the dimensions string
    auto dimensionsStr = name.substr(
        secondToLastUnderscore + 1, lastUnderscore - lastUnderscore - 1);
    std::cout << "Dimensions string: " << dimensionsStr << std::endl;
    vec3i dimensions;
    try {
      auto dimensionsSplit = utility::split(dimensionsStr, "x");
      if (dimensionsSplit.size() != 3) {
        throw std::runtime_error("Invalid dimensions string: " + dimensionsStr);
      }
      dimensions.x = std::stoi(dimensionsSplit[0]);
      dimensions.y = std::stoi(dimensionsSplit[1]);
      dimensions.z = std::stoi(dimensionsSplit[2]);
    } catch (const std::exception &e) {
      throw std::runtime_error(
          "Failed to parse dimensions from " + dimensionsStr + ": " + e.what());
    }
    std::cout << "Parsed dimensions: " << dimensions << std::endl;
    volume->createChild("dimensions", "vec3i", dimensions);

    // Everything between the last underscore and the extension is the voxel
    // type
    auto extensionStart = name.find_last_of(".");
    auto voxelTypeStr =
        name.substr(lastUnderscore + 1, extensionStart - lastUnderscore - 1);
    std::cout << "Voxel type: " << voxelTypeStr << std::endl;
    auto voxelType = rawVolumeVoxelType.find(voxelTypeStr)->second;
    volume->createChild("voxelType", "OSPDataType", voxelType);

    if (isSpherical) {
      auto sphericalVolume =
          std::static_pointer_cast<StructuredSpherical>(volume);
      sphericalVolume->load(fileName);
    } else {
      auto structuredVolume = std::static_pointer_cast<StructuredVolume>(volume);
      structuredVolume->load(fileName);
    }

    auto &tf =
        getOrCreateTransferFunctionNode(volume, "transfer_function_turbo");
    auto valueRange = volume->child("value").valueAs<range1f>();
    tf.child("value") = valueRange;

    rootNode->add(volume);

    auto addToSceneCallback = [&, self, rootNode](SchedulerPtr scheduler) {
      // Finally, add node hierarchy to importer parent
      add(rootNode);
    }; // addToSceneCallback

    if (scheduler) {
      auto name = "add raw volume from "s + fileName.str() + " to scene"s;
      scheduler->ospray()->push(name, addToSceneCallback);
    } else {
      addToSceneCallback(nullptr);
    }
  }; // loadDataCallback

  if (scheduler) {
    auto name = "load raw volume from "s + fileName.str();
    scheduler->background()->push(name, loadDataCallback);
  } else {
    loadDataCallback(nullptr);
  }
}

} // namespace sg
} // namespace ospray
