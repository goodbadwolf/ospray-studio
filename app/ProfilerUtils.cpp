#include "ProfilerUtils.h"

#include <iostream>

ProfilerEvent::ProfilerEvent(const std::string &prefix) : prefix(prefix) {}

void ProfilerEvent::Start()
{
  this->startTime = std::chrono::system_clock::now();
}

void ProfilerEvent::PrintSummary()
{
  std::chrono::duration<double> elapsed_seconds =
      this->endTime - this->startTime;
  auto prefix = this->prefix.empty() ? "" : (this->prefix + " > ");
  auto timeInMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_seconds);
  std::cout << prefix << "Elapsed time: " << timeInMs.count() << " ms\n";
}

void ProfilerEvent::End()
{
  this->endTime = std::chrono::system_clock::now();
}