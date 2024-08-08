// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Generator.h"

namespace ospray {
  namespace sg {

  Generator::Generator()
  {
    createChild("parameters");
  }

  NodeType Generator::type() const
  {
    return NodeType::GENERATOR;
  }

  void Generator::preCommit()
  {
    // Re-run generator on parameter changes in the UI
    if (child("parameters").isModified())
      generateData();
  }

  void Generator::postCommit()
  {
  }

  void Generator::generateData()
  {
  }

  sg::TransferFunction &Generator::getOrCreateTransferFunctionNode(
      const std::string &subType)
  {
    const std::string tfName = "transferFunction";
    auto &xfm = child("xfm");
    std::shared_ptr<TransferFunction> tfPtr = nullptr;
    if (xfm.hasChild(tfName)) {
      tfPtr = xfm.childNodeAs<TransferFunction>(tfName);
    } else {
      tfPtr = xfm.createChildAs<TransferFunction>(tfName, subType)
                  .nodeAs<TransferFunction>();
    }
    return *tfPtr;
  }

  }  // namespace sg
} // namespace ospray
