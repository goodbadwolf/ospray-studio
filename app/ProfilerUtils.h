#pragma once

#include <chrono>
#include <string>

struct ProfilerEvent
{
 public:
  ProfilerEvent(const std::string &prefix = "");
  ~ProfilerEvent() = default;

  void Start();
  void End();
  void PrintSummary();

 protected:
  std::string prefix;
  decltype(std::chrono::system_clock::now()) startTime;
  decltype(std::chrono::system_clock::now()) endTime;
};

#define PROFILE_START(name)                                                    \
  ProfilerEvent name##_event(#name);                                           \
  name##_event.Start()

#define PROFILE_END(name)                                                      \
  name##_event.End();                                                          \
  name##_event.PrintSummary()
