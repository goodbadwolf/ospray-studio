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

// std
#include <chrono>
#include <future>
#include <vector>
#include <thread> // temporary - see below
// ospcommon
#include "ospcommon/tasking/async.h"
// ospray_sg
#include "sg/common/Node.h"

namespace ospray {
  namespace job_scheduler {

    // TEMPORARY //////////////////////////////////////////////////////////////

    // NOTE: TBB segfaults when using ospcommon::taksing::* to schedule work,
    //       so use equivalent versions which always launch a std::thread for
    //       now...

    namespace detail {

      template <typename TASK_T>
      inline void schedule(TASK_T fcn)
      {
        std::thread thread(fcn);
        thread.detach();
      }

      template<typename TASK_T>
      using operator_return_t = typename std::result_of<TASK_T()>::type;

      template<typename TASK_T>
      inline auto async(TASK_T&& fcn) -> std::future<operator_return_t<TASK_T>>
      {
        using package_t = std::packaged_task<operator_return_t<TASK_T>()>;

        auto task   = new package_t(std::forward<TASK_T>(fcn));
        auto future = task->get_future();

        schedule([=](){ (*task)(); delete task; });

        return future;
      }

    } // namespace detail

    ///////////////////////////////////////////////////////////////////////////

    using Nodes = std::vector<std::shared_ptr<sg::Node>>;

    template <typename T>
    inline bool is_ready(const std::future<T> & f)
    {
      return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    template <typename JOB_T>
    inline std::future<Nodes> schedule_job(JOB_T &&job)
    {
#if 0
      return ospcommon::tasking::async(job);
#else
      return detail::async(job);
#endif
    }

  } // namespace job_scheduler
} // namespace ospray