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

#include "../common/util/AsyncRenderEngine.h"

#include "imgui3D.h"

#include "sg/SceneGraph.h"
#include "sg/Renderer.h"

#include "../jobs/JobScheduler.h"

#include <list>

namespace ospray {

  /*! mini scene graph viewer widget. \internal Note that all handling
    of camera is almost exactly similar to the code in volView;
    might make sense to move that into a common class! */
  class OSPRAY_IMGUI3D_INTERFACE ImGuiViewer
    : public ospray::imgui3D::ImGui3DWidget
  {
  public:

    ImGuiViewer(const std::shared_ptr<sg::Root> &scenegraph);

    ~ImGuiViewer();

    void setInitialSearchBoxText(const std::string &text);

  protected:

    enum PickMode { PICK_CAMERA, PICK_NODE };

    void mouseButton(int button, int action, int mods);
    void reshape(const ospcommon::vec2i &newSize) override;
    void keypress(char key) override;

    void resetView();
    void resetDefaultView();
    void printViewport();
    void saveScreenshot(const std::string &basename);
    void toggleRenderingPaused();

    void display() override;

    void processFinishedJobs();
    void clearFinishedJobs();

    void buildGui() override;

    void guiMainMenu();
    void guiMainMenuFile();
    void guiMainMenuView();
    void guiMainMenuCamera();
    void guiMainMenuHelp();

    void guiJobStatusControlPanel();
    void guiRenderStats();
    void guiFindNode();
    void guiSGWindow();
    void guiAbout();

    void guiImportData();
    void guiGenerateData();

    void guiSingleNode(const std::string &baseText,
                       std::shared_ptr<sg::Node> node);
    void guiNodeContextMenu(const std::string &name,
                            std::shared_ptr<sg::Node> node);

    void guiSGTree(const std::string &name, std::shared_ptr<sg::Node> node);

    void guiSearchSGNodes();

    void setCurrentDeviceParameter(const std::string &param, int value);

    // Data ///////////////////////////////////////////////////////////////////

    // Windows to be shown/hidden //

    bool showWindowAbout{false};
    bool showWindowFindNode{false};
    bool showWindowGenerateData{false};
    bool showWindowImGuiDemo{false};
    bool showWindowJobStatusControlPanel{true};
    bool showWindowImportData{false};
    bool showWindowRenderStatistics{false};
    bool showWindowSceneGraph{false};

    // Experimental items //

    std::list<std::unique_ptr<job_scheduler::Job>> jobsInProgress;
    job_scheduler::Nodes loadedNodes;

    // Not-yet-categorized data //

    double lastFrameFPS;
    double lastGUITime;
    double lastDisplayTime;
    double lastTotalTime;
    float lastVariance;

    ospcommon::vec2i windowSize;
    imgui3D::ImGui3DWidget::ViewPort originalView;

    std::shared_ptr<sg::Root> scenegraph;
    std::shared_ptr<sg::Renderer> renderer;

    std::string nodeNameForSearch;
    std::vector<std::shared_ptr<sg::Node>> collectedNodesFromSearch;

    AsyncRenderEngine renderEngine;
    std::vector<uint32_t> pixelBuffer;

    bool useDynamicLoadBalancer{false};
    int  numPreAllocatedTiles{4};

    PickMode lastPickQueryType {PICK_CAMERA};
  };

}// namespace ospray
