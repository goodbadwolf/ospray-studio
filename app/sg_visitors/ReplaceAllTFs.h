// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "sg/common/Renderable.h"
#include "sg/visitor/Visitor.h"

namespace ospray {
  namespace sg {

    struct ReplaceAllTFs : public Visitor
    {
      ReplaceAllTFs(std::shared_ptr<sg::Node> master_tf);

      bool operator()(Node &node, TraversalContext &ctx) override;

     private:
      // Data //
      std::shared_ptr<sg::Node> master_tf;
    };

    // Inlined definitions ////////////////////////////////////////////////////

    inline ReplaceAllTFs::ReplaceAllTFs(std::shared_ptr<sg::Node> tf)
        : master_tf(tf)
    {
    }

    inline bool ReplaceAllTFs::operator()(Node &node, TraversalContext &)
    {
      auto tf = node.tryNodeAs<TransferFunction>();
      if (tf.get() != nullptr) {
        auto &parent = tf->parent();
        parent.setChild("transferFunction", master_tf);
        return false;
      } else {
        return true;
      }
    }

  }  // namespace sg
}  // namespace ospray
