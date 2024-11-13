#pragma once

#include <chrono>
#include <iostream>
#include <mutex>

#include "Configuration.hpp"

enum class Event { kStart = 0, kEnd };

enum class Section {
  kParseXML = 0,
  kLoadScene,
  kPreprocessScene,
  kRenderScene,
  kRayTracing,
  kFiltering,
  kToneMapping,
  kExportImage
};

struct TimeLog {
  Section section_;
  Event event_;
  int camera_id_;
  int pixel_id_;
  int ray_id_;
  uint64_t timestamp_;
};

class Timer {
 public:
  void AddTimeLog(Section section, Event event, int camera_id = -1,
                  int pixel_id = -1, int ray_id = -1);
  void AnalyzeTimeLogs();
  Configuration configuration_;

 private:
  std::mutex mutex_;
  std::vector<TimeLog> time_logs_;
};

extern Timer timer;