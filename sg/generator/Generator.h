// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../Node.h"
#include "sg/renderer/MaterialRegistry.h"
#include "sg/scene/transfer_function/TransferFunction.h"

namespace ospray {
namespace sg {

struct OSPSG_INTERFACE Generator : public Node
{
  Generator();
  virtual ~Generator() = default;

  NodeType type() const override;

  virtual void preCommit() override;
  virtual void postCommit() override;

  virtual void generateData();

  inline void setMaterialRegistry(
      std::shared_ptr<sg::MaterialRegistry> _registry)
  {
    materialRegistry = _registry;
  }

 protected:
  std::shared_ptr<sg::MaterialRegistry> materialRegistry = nullptr;

  virtual sg::TransferFunction &getOrCreateTransferFunctionNode(
      const std::string &subType);
};

} // namespace sg
} // namespace ospray
