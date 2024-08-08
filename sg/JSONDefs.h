// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <json.hpp>

#include "Node.h"
#include "importer/Importer.h"
#include "scene/lights/LightsManager.h"
#include "scene/transfer_function/TransferFunction.h"

#include "ArcballCamera.h"

// This file contains definitions of `to_json` and `from_json` for custom types
// used within Studio. These methods allow us to easily serialize and
// deserialize SG nodes to JSON.

// default nlohmann::json will sort map alphabetically. this leaves it as-is
using JSON = nlohmann::ordered_json;

///////////////////////////////////////////////////////////////////////
// Forward declarations ///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

inline void to_json(JSON &j, const CameraState &cs);
inline void from_json(const JSON &j, CameraState &cs);

// rkcommon type declarations /////////////////////////////////////////

namespace rkcommon {
namespace containers {
void to_json(
    JSON &j, const FlatMap<std::string, ospray::sg::NodePtr> &fm);
void from_json(
    const JSON &j, FlatMap<std::string, ospray::sg::NodePtr> &fm);
} // namespace containers
namespace math {
inline void to_json(JSON &j, const LinearSpace2f &as);
inline void from_json(const JSON &j, LinearSpace2f &as);
inline void to_json(JSON &j, const AffineSpace3f &as);
inline void from_json(const JSON &j, AffineSpace3f &as);
inline void to_json(JSON &j, const quaternionf &q);
inline void from_json(const JSON &j, quaternionf &q);
} // namespace math
namespace utility {
void to_json(JSON &j, const Any &a);
void from_json(const JSON &j, Any &a);
} // namespace utility
} // namespace rkcommon

///////////////////////////////////////////////////////////////////////
// SG types ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

namespace ospray {
namespace sg {

inline void to_json(JSON &j, const Node &n)
{
  // Don't export these nodes, they must be regenerated and can't be imported.
  if ((n.type() == NodeType::GENERIC && n.name() == "handles")
      || (n.type() == NodeType::PARAMETER && n.subType() == "Data")
      || (n.type() == NodeType::GEOMETRY))
    return;

  // XXX would be nice to have GEOMETRY node for isVisible and isClippingGeometry properties

  j = JSON{{"name", n.name()},
      {"type", NodeTypeToString[n.type()]},
      {"subType", n.subType()}};

  // Don't export the node descriptions to JSON.  They take a lot of space, yet
  // provide little value. (20-25% of a .sg file for descriptions)
  //if (n.description() != "<no description>")
  //  j["description"] = n.description();

  // we only want the importer and its root transform, not the hierarchy of
  // geometry under it
  if (n.type() == NodeType::IMPORTER) {
    j["filename"] = n.nodeAs<const Importer>()->getFileName().str();
    for (auto &child : n.children()) {
      if (child.second->type() == NodeType::TRANSFORM) {
        j["children"] = {*child.second};
        break;
      }
    }
    return;
  }

  if (n.type() == NodeType::PARAMETER)
    j["sgOnly"] = n.sgOnly();

  if (n.value().valid() && (n.type() == NodeType::PARAMETER
      || n.type() == NodeType::TRANSFORM)) {
    j["value"] = n.value();
    if (n.hasMinMax())
      j["minMax"] = JSON{n.min(), n.max()};
  }

  if (n.type() == NodeType::TRANSFER_FUNCTION) {
    auto tf = n.nodeAs<const TransferFunction>();

    std::vector<float> colorPointsX, colorPointsR, colorPointsG, colorPointsB;
    std::vector<float> opacityPointsX, opacityPointsO;
    std::for_each(tf->colorPoints.begin(),
        tf->colorPoints.end(),
        [&colorPointsX, &colorPointsR, &colorPointsG, &colorPointsB](
            const vec4f &v) {
          colorPointsX.push_back(v.x);
          colorPointsR.push_back(v.y);
          colorPointsG.push_back(v.z);
          colorPointsB.push_back(v.w);
        });
    std::for_each(tf->opacityPoints.begin(),
        tf->opacityPoints.end(),
        [&opacityPointsX, &opacityPointsO](const vec2f &v) {
          opacityPointsX.push_back(v.x);
          opacityPointsO.push_back(v.y);
        });
    std::vector<float> colorsR, colorsG, colorsB, opacities;
    std::for_each(tf->colors.begin(),
        tf->colors.end(),
        [&colorsR, &colorsG, &colorsB](const vec3f &v) {
          colorsR.push_back(v.x);
          colorsG.push_back(v.y);
          colorsB.push_back(v.z);
        });
    opacities = tf->opacities;

#define ADD_TF_DATA(DATA) j["value"][#DATA] = DATA;
    ADD_TF_DATA(colorPointsX);
    ADD_TF_DATA(colorPointsR);
    ADD_TF_DATA(colorPointsG);
    ADD_TF_DATA(colorPointsB);
    ADD_TF_DATA(opacityPointsX);
    ADD_TF_DATA(opacityPointsO);
    ADD_TF_DATA(colorsR);
    ADD_TF_DATA(colorsG);
    ADD_TF_DATA(colorsB);
    ADD_TF_DATA(opacities);
#undef ADD_TF_DATA
  }

  if (n.hasChildren())
    j["children"] = n.children();
}

inline void from_json(const JSON &, Node &) {}

// Helper for converting ambiguous json types to stronger "subType"
#define CONVERT_TYPE(X)                                                        \
  {                                                                            \
    if (subType == #X) {                                                       \
      n->setValue(X(n->valueAs<int>()));                                       \
      if (n->hasMinMax())                                                      \
        n->setMinMax(X(n->minAs<int>()), X(n->maxAs<int>()));                  \
    }                                                                          \
  }

// Helper for correcting OSP* enum types when not supplied by json
#define FIX_SUBTYPE(X, OSPSUBTYPE)                                             \
  {                                                                            \
    if (j["name"] == #X)                                                       \
      subType = #OSPSUBTYPE;                                                   \
  }

inline OSPSG_INTERFACE NodePtr createNodeFromJSON(const JSON &j)
{
  NodePtr n = nullptr;

  // This is a generated value and can't be imported
  if (j["name"] == "handles")
    return nullptr;

  // XXX Textures import needs to be handled correctly.  Skip for now.
  if (j["subType"] == "texture_2d")
    return nullptr;

  // If the json doesn't contain a valid value, just ignore the node
  // :^) was used as a sentinel for unhandled types.
  if ((j.contains("value") && j["value"].is_string()
          && j["value"].get<std::string>() == ":^)"))
    return nullptr;

  // Original json subType, may need to be corrected
  std::string subType = j["subType"];

  // Handle pre-OSPRay3 Studio sg files without strict OSP* enum types
  // These are the common troublemakers in older sg files.
  FIX_SUBTYPE(intensityQuantity, OSPIntensityQuantity);
  FIX_SUBTYPE(shutterType, OSPShutterType);
  FIX_SUBTYPE(stereoMode, OSPStereoMode);
  FIX_SUBTYPE(filter, OSPTextureFilter);
  FIX_SUBTYPE(filter, OSPTextureWrapMode);
  FIX_SUBTYPE(format, OSPTextureFormat);

  if (j["type"] == "TRANSFER_FUNCTION" && j.contains("value")) {
    n = createNode(j["name"], subType);
    auto tf = n->nodeAs<TransferFunction>();
    auto &value = j["value"];

    bool hasColorPoints = value.contains("colorPointsX")
        && value.contains("colorPointsR") && value.contains("colorPointsG")
        && value.contains("colorPointsB");
    bool hasOpacityPoints =
        value.contains("opacityPointsX") && value.contains("opacityPointsO");

    bool hasColors = value.contains("colorsR") && value.contains("colorsG")
        && value.contains("colorsB");
    bool hasOpacities = value.contains("opacities");

    if (hasColorPoints && hasOpacityPoints) {
#define GET_TF_DATA(DATA) DATA = value[#DATA].get<std::vector<float>>();
      std::vector<float> colorPointsX, colorPointsR, colorPointsG, colorPointsB;
      std::vector<float> opacityPointsX, opacityPointsO;
      std::vector<float> colorsR, colorsG, colorsB, opacities;
      GET_TF_DATA(colorPointsX);
      GET_TF_DATA(colorPointsR);
      GET_TF_DATA(colorPointsG);
      GET_TF_DATA(colorPointsB);
      GET_TF_DATA(opacityPointsX);
      GET_TF_DATA(opacityPointsO);
      GET_TF_DATA(colorsR);
      GET_TF_DATA(colorsG);
      GET_TF_DATA(colorsB);
      GET_TF_DATA(opacities);
#undef GET_TF_DATA

      std::vector<vec4f> colorPoints;
      std::vector<vec2f> opacityPoints;
      for (size_t i = 0; i < colorPointsX.size(); i++)
        colorPoints.push_back(vec4f(colorPointsX[i],
            colorPointsR[i],
            colorPointsG[i],
            colorPointsB[i]));
      for (size_t i = 0; i < opacityPointsX.size(); i++)
        opacityPoints.push_back(vec2f(opacityPointsX[i], opacityPointsO[i]));

      std::vector<vec3f> colors;
      for (size_t i = 0; i < colorsR.size(); i++)
        colors.push_back(vec3f(colorsR[i], colorsG[i], colorsB[i]));

      tf->setColorPointsAndOpacityPoints(colorPoints, opacityPoints);
      tf->setColorsAndOpacties(colors, opacities);
    }
  } else if (j.contains("value")) {
    Any value;

    // Stored in scene file as basic JSON objects.  Rather than trying to infer
    // their subType in generic from_json(), it's easier to handle type here.
    if (j["subType"] == "transform") {
      value = j["value"].get<rkcommon::math::AffineSpace3f>();
    } else if (j["subType"] == "quaternionf") {
      value = j["value"].get<rkcommon::math::quaternionf>();
    } else if (j["subType"] == "linear2f") {
      value = j["value"].get<rkcommon::math::LinearSpace2f>();
    } else {
      // Any other data type
      value = j["value"].get<Any>();
    }

    // Create node with optional description
    if (j.contains("description"))
      n = createNode(j["name"], subType, j["description"], value);
    else
      n = createNode(j["name"], subType, value);

    if (j.contains("sgOnly") && j["sgOnly"].get<bool>())
      n->setSGOnly();

    if (j.contains("minMax")) {
      auto minMax = j["minMax"].get<std::vector<Any>>();
      n->setMinMax(minMax[0], minMax[1]);
    }

    // JSON doesn't distinguish the following types.  They are primitive type
    // "int" and need conversion based on subType.

    // OSPRay enum types
    CONVERT_TYPE(OSPAMRMethod);
    CONVERT_TYPE(OSPCurveBasis);
    CONVERT_TYPE(OSPCurveType);
    CONVERT_TYPE(OSPDataType);
    CONVERT_TYPE(OSPDeviceProperty);
    CONVERT_TYPE(OSPError);
    CONVERT_TYPE(OSPFrameBufferChannel);
    CONVERT_TYPE(OSPFrameBufferFormat);
    CONVERT_TYPE(OSPIntensityQuantity);
    CONVERT_TYPE(OSPLogLevel);
    CONVERT_TYPE(OSPPixelFilterType);
    CONVERT_TYPE(OSPShutterType);
    CONVERT_TYPE(OSPStereoMode);
    CONVERT_TYPE(OSPSubdivisionMode);
    CONVERT_TYPE(OSPSyncEvent);
    CONVERT_TYPE(OSPTextureFilter);
    CONVERT_TYPE(OSPTextureWrapMode);
    CONVERT_TYPE(OSPTextureFormat);
    CONVERT_TYPE(OSPUnstructuredCellType);
    CONVERT_TYPE(OSPVolumeFilter);
    CONVERT_TYPE(OSPVolumeFormat);

    // integer to uint8_t
    using uchar = uint8_t;
    CONVERT_TYPE(uchar);

    // vec2f to range1f - in json, these two are identical
    if (j["subType"] == "range1f") {
      vec2f v(n->valueAs<vec2f>());
      n->setValue(math::range1f(v[0], v[1]));
      if (n->hasMinMax())
        n->setMinMax(range1f(n->minAs<float>()), range1f(n->maxAs<float>()));
    }
  } else {
    n = createNode(j["name"], subType);
  }

  if (n != nullptr) {
    // the default ambient light might not exist in this scene
    // the loop below will add it if it does exist
    if (n && n->type() == NodeType::LIGHTS)
      n->nodeAs<LightsManager>()->removeLight("ambient");

    if (j.contains("children")) {
      for (auto &jChild : j["children"]) {
        auto child = createNodeFromJSON(jChild);
        if (!child)
          continue;
        if (n->type() == NodeType::LIGHTS)
          n->nodeAs<LightsManager>()->addLight(child);
        else if (n->type() == NodeType::MATERIAL)
          n->nodeAs<Material>()->add(child);
        else
          n->add(child);
      }
    }
  }

  return n;
}

} // namespace sg
} // namespace ospray

///////////////////////////////////////////////////////////////////////
// rkcommon type definitions //////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

namespace rkcommon {
namespace containers {

inline void to_json(
    JSON &j, const FlatMap<std::string, ospray::sg::NodePtr> &fm)
{
  for (const auto e : fm) {
    JSON jnew = *(e.second);
    if (!jnew.is_null())
      j.push_back(jnew);
  }
}

inline void from_json(
    const JSON &, FlatMap<std::string, ospray::sg::NodePtr> &)
{}

} // namespace containers

namespace math {

inline void to_json(JSON &j, const vec2ui &v)
{
  j = {v.x, v.y};
}

inline void from_json(const JSON &j, vec2ui &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
}

inline void to_json(JSON &j, const vec2i &v)
{
  j = {v.x, v.y};
}

inline void from_json(const JSON &j, vec2i &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
}

inline void to_json(JSON &j, const vec2f &v)
{
  j = {v.x, v.y};
}

inline void to_json(JSON &j, const range1f &v)
{
  j = {v.lower, v.upper};
}

inline void from_json(const JSON &j, vec2f &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
}

inline void to_json(JSON &j, const vec3i &v)
{
  j = {v.x, v.y, v.z};
}

inline void from_json(const JSON &j, vec3i &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
}

inline void to_json(JSON &j, const vec3f &v)
{
  j = {v.x, v.y, v.z};
}

inline void from_json(const JSON &j, vec3f &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
}

inline void to_json(JSON &j, const vec4i &v)
{
  j = {v.x, v.y, v.z, v.w};
}

inline void from_json(const JSON &j, vec4i &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
  j.at(2).get_to(v.w);
}

inline void to_json(JSON &j, const vec4f &v)
{
  j = {v.x, v.y, v.z, v.w};
}

inline void from_json(const JSON &j, vec4f &v)
{
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
  j.at(3).get_to(v.w);
}

inline void to_json(JSON &j, const LinearSpace2f &ls)
{
  j = JSON{{"x", ls.vx}, {"y", ls.vy}};
}

inline void from_json(const JSON &j, LinearSpace2f &ls)
{
  j.at("x").get_to(ls.vx);
  j.at("y").get_to(ls.vy);
}


inline void to_json(JSON &j, const LinearSpace3f &ls)
{
  j = JSON{{"x", ls.vx}, {"y", ls.vy}, {"z", ls.vz}};
}

inline void from_json(const JSON &j, LinearSpace3f &ls)
{
  j.at("x").get_to(ls.vx);
  j.at("y").get_to(ls.vy);
  j.at("z").get_to(ls.vz);
}

inline void to_json(JSON &j, const AffineSpace3f &as)
{
  j = JSON{{"linear", as.l}, {"affine", as.p}};
}

inline void from_json(const JSON &j, AffineSpace3f &as)
{
  if (j.contains("linear") && j.contains("affine")) {
    j.at("linear").get_to(as.l);
    j.at("affine").get_to(as.p);
  } else {
    std::vector<float> xfm;
    for (auto &val : j)
      xfm.push_back(val.get<float>());
    as = rkcommon::math::affine3f(&xfm[0], &xfm[4], &xfm[8], &xfm[12]);
  }
}

inline void to_json(JSON &j, const quaternionf &q)
{
  j = JSON{{"r", q.r}, {"i", q.i}, {"j", q.j}, {"k", q.k}};
}

inline void from_json(const JSON &j, quaternionf &q)
{
  j.at("r").get_to(q.r);
  j.at("i").get_to(q.i);
  j.at("j").get_to(q.j);
  j.at("k").get_to(q.k);
}

} // namespace math

namespace utility {

template <typename T>
inline void captureType(JSON &j, const Any &a)
{
  if (a.is<T>())
    j = a.get<T>();
}

inline void to_json(JSON &j, const Any &a)
{
  j = {}; // start with empty
  captureType<int>(j, a);
  captureType<bool>(j, a);
  captureType<uint8_t>(j, a);
  captureType<uint32_t>(j, a);
  captureType<float>(j, a);
  captureType<std::string>(j, a);
  captureType<math::vec2ui>(j, a);
  captureType<math::vec2i>(j, a);
  captureType<math::vec2f>(j, a);
  captureType<math::range1f>(j, a);
  captureType<math::vec3i>(j, a);
  captureType<math::vec3f>(j, a);
  captureType<math::vec4f>(j, a);
  captureType<math::LinearSpace2f>(j, a);
  captureType<math::AffineSpace3f>(j, a);
  captureType<math::quaternionf>(j, a);

  // OSPRay enum types
  captureType<OSPAMRMethod>(j, a);
  captureType<OSPCurveBasis>(j, a);
  captureType<OSPCurveType>(j, a);
  captureType<OSPDataType>(j, a);
  captureType<OSPDeviceProperty>(j, a);
  captureType<OSPError>(j, a);
  captureType<OSPFrameBufferChannel>(j, a);
  captureType<OSPFrameBufferFormat>(j, a);
  captureType<OSPIntensityQuantity>(j, a);
  captureType<OSPLogLevel>(j, a);
  captureType<OSPPixelFilterType>(j, a);
  captureType<OSPShutterType>(j, a);
  captureType<OSPStereoMode>(j, a);
  captureType<OSPSubdivisionMode>(j, a);
  captureType<OSPSyncEvent>(j, a);
  captureType<OSPTextureFilter>(j, a);
  captureType<OSPTextureWrapMode>(j, a);
  captureType<OSPTextureFormat>(j, a);
  captureType<OSPUnstructuredCellType>(j, a);
  captureType<OSPVolumeFilter>(j, a);
  captureType<OSPVolumeFormat>(j, a);

  if (j.is_null()) {
    std::cerr << "JSONDefs.h: :^) strikes back!!!" << std::endl;
    j = ":^)";
  }
}

inline void from_json(const JSON &j, Any &a)
{
  if (j.is_primitive()) { // string, number , bool, null, or binary, basic types
                          // This will also include OSPRay enums, as ints
    if (j.is_null())
      return;
    else if (j.is_boolean())
      a = j.get<bool>();
    else if (j.is_number_integer())
      a = j.get<int>();
    else if (j.is_number_float())
      a = j.get<float>();
    else if (j.is_string())
      a = j.get<std::string>();
    else
      std::cout << "unhandled primitive type in json" << std::endl;
  } else if (j.is_structured()) { // array or object
    if (j.is_array() && j.size() == 2) {
      if (j[0].is_number_float())
        a = j.get<math::vec2f>();
      else
        a = j.get<math::vec2i>();
    } else if (j.is_array() && j.size() == 3) {
      if (j[0].is_number_float())
        a = j.get<math::vec3f>();
      else
        a = j.get<math::vec3i>();
    } else if (j.is_array() && j.size() == 4) {
      if (j[0].is_number_float())
        a = j.get<math::vec4f>();
      else
        a = j.get<math::vec4i>();
    } else if (j.is_object()) {
      std::cout << "cannot load object types from json" << std::endl;
      for (auto &jI : j.items()) {
        auto &key = jI.key();
        auto &val = jI.value();
        std::cout << ": " << key << ":" << val << std::endl;
      }
    } else
      std::cout << "unhandled structured type in json " << std::endl;
  } else { // something is wrong
    std::cout << "unidentified type in json" << std::endl;
  }
}

} // namespace utility

} // namespace rkcommon

///////////////////////////////////////////////////////////////////////
// Global namespace type definitions //////////////////////////////////
///////////////////////////////////////////////////////////////////////

inline void to_json(JSON &j, const CameraState &cs)
{
  j = JSON{{"centerTranslation", cs.centerTranslation},
      {"translation", cs.translation},
      {"rotation", cs.rotation},
      {"cameraToWorld", cs.cameraToWorld}};
}

inline void from_json(const JSON &j, CameraState &cs)
{
  j.at("centerTranslation").get_to(cs.centerTranslation);
  j.at("translation").get_to(cs.translation);
  j.at("rotation").get_to(cs.rotation);
  if (j.find("cameraToWorld") != j.end())
    j.at("cameraToWorld").get_to(cs.cameraToWorld);
}
