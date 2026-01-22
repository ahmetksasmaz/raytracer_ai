#pragma once

#include <chrono>
#include <iostream>
#include <mutex>

#include "../extern/parser.h"

enum class Event { kStart = 0, kEnd };

enum class Section {
  kParseXML = 0,
  kLoadScene,
  kPreprocessScene,
  kRenderScene,
  kFiltering,
  kToneMapping,
  kExportImage
};

struct TimeLog {
  Section section_;
  Event event_;
  int camera_id_;
  uint64_t timestamp_;
};

class Timer {
 public:
  void AddTimeLog(Section section, Event event, int camera_id = -1);
  void AnalyzeTimeLogs();

 private:
  std::mutex mutex_;
  std::vector<TimeLog> time_logs_;
};

extern Timer timer;