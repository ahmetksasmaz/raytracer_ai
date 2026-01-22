#include "Timer.hpp"

void Timer::AddTimeLog(Section section, Event event, int camera_id) {
  TimeLog time_log;
  time_log.section_ = section;
  time_log.event_ = event;
  time_log.camera_id_ = camera_id;
  time_log.timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

  std::lock_guard<std::mutex> lock(mutex_);
  time_logs_.push_back(time_log);
}

void Timer::AnalyzeTimeLogs() {
  std::sort(time_logs_.begin(), time_logs_.end(),
            [](const TimeLog& a, const TimeLog& b) {
              return a.timestamp_ < b.timestamp_;
            });
  std::sort(time_logs_.begin(), time_logs_.end(),
            [](const TimeLog& a, const TimeLog& b) {
              return a.camera_id_ < b.camera_id_;
            });
  std::sort(
      time_logs_.begin(), time_logs_.end(),
      [](const TimeLog& a, const TimeLog& b) { return a.event_ < b.event_; });
  std::sort(time_logs_.begin(), time_logs_.end(),
            [](const TimeLog& a, const TimeLog& b) {
              return a.section_ < b.section_;
            });

  std::pair<uint64_t, uint64_t> parse_scene_file_time_pair = {0, 0};
  std::pair<uint64_t, uint64_t> load_scene_time_pair = {0, 0};
  std::pair<uint64_t, uint64_t> preprocess_scene_time_pair = {0, 0};
  std::vector<std::pair<uint64_t, uint64_t>> render_scene_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> filtering_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> tone_mapping_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> export_image_time_pair;

  for (size_t i = 0; i < time_logs_.size(); i++) {
    if (time_logs_[i].section_ == Section::kParseXML) {
      switch (time_logs_[i].event_) {
        case Event::kStart:
          parse_scene_file_time_pair.first = time_logs_[i].timestamp_;
          break;
        case Event::kEnd:
          parse_scene_file_time_pair.second = time_logs_[i].timestamp_;
          break;
      }
    } else if (time_logs_[i].section_ == Section::kLoadScene) {
      switch (time_logs_[i].event_) {
        case Event::kStart:
          load_scene_time_pair.first = time_logs_[i].timestamp_;
          break;
        case Event::kEnd:
          load_scene_time_pair.second = time_logs_[i].timestamp_;
          break;
      }
    } else if (time_logs_[i].section_ == Section::kPreprocessScene) {
      switch (time_logs_[i].event_) {
        case Event::kStart:
          preprocess_scene_time_pair.first = time_logs_[i].timestamp_;
          break;
        case Event::kEnd:
          preprocess_scene_time_pair.second = time_logs_[i].timestamp_;
          break;
      }
    } else if (time_logs_[i].section_ == Section::kRenderScene) {
      if (time_logs_[i].camera_id_ >= 0) {
        if (render_scene_time_pair.size() <= static_cast<size_t>(time_logs_[i].camera_id_)) {
          render_scene_time_pair.resize(time_logs_[i].camera_id_ + 1);
        }
        switch (time_logs_[i].event_) {
          case Event::kStart:
            render_scene_time_pair[time_logs_[i].camera_id_].first =
                time_logs_[i].timestamp_;
            break;
          case Event::kEnd:
            render_scene_time_pair[time_logs_[i].camera_id_].second =
                time_logs_[i].timestamp_;
            break;
        }
      }
    } else if (time_logs_[i].section_ == Section::kFiltering) {
      if (time_logs_[i].camera_id_ >= 0) {
        if (filtering_time_pair.size() <= static_cast<size_t>(time_logs_[i].camera_id_)) {
          filtering_time_pair.resize(time_logs_[i].camera_id_ + 1);
        }
        switch (time_logs_[i].event_) {
          case Event::kStart:
            filtering_time_pair[time_logs_[i].camera_id_].first =
                time_logs_[i].timestamp_;
            break;
          case Event::kEnd:
            filtering_time_pair[time_logs_[i].camera_id_].second =
                time_logs_[i].timestamp_;
            break;
        }
      }
    } else if (time_logs_[i].section_ == Section::kToneMapping) {
      if (time_logs_[i].camera_id_ >= 0) {
        if (tone_mapping_time_pair.size() <= static_cast<size_t>(time_logs_[i].camera_id_)) {
          tone_mapping_time_pair.resize(time_logs_[i].camera_id_ + 1);
        }
        switch (time_logs_[i].event_) {
          case Event::kStart:
            tone_mapping_time_pair[time_logs_[i].camera_id_].first =
                time_logs_[i].timestamp_;
            break;
          case Event::kEnd:
            tone_mapping_time_pair[time_logs_[i].camera_id_].second =
                time_logs_[i].timestamp_;
            break;
        }
      }
    } else if (time_logs_[i].section_ == Section::kExportImage) {
      if (time_logs_[i].camera_id_ >= 0) {
        if (export_image_time_pair.size() <= static_cast<size_t>(time_logs_[i].camera_id_)) {
          export_image_time_pair.resize(time_logs_[i].camera_id_ + 1);
        }
        switch (time_logs_[i].event_) {
          case Event::kStart:
            export_image_time_pair[time_logs_[i].camera_id_].first =
                time_logs_[i].timestamp_;
            break;
          case Event::kEnd:
            export_image_time_pair[time_logs_[i].camera_id_].second =
                time_logs_[i].timestamp_;
            break;
        }
      }
    }
  }

  uint64_t parse_scene_file_time = parse_scene_file_time_pair.second - parse_scene_file_time_pair.first;
  uint64_t load_scene_time = load_scene_time_pair.second - load_scene_time_pair.first;
  uint64_t preprocess_scene_time = preprocess_scene_time_pair.second - preprocess_scene_time_pair.first;

  std::vector<uint64_t> render_scene_time;
  std::vector<uint64_t> filtering_time;
  std::vector<uint64_t> tone_mapping_time;
  std::vector<uint64_t> export_image_time;

  for (size_t i = 0; i < render_scene_time_pair.size(); i++) {
    render_scene_time.push_back(render_scene_time_pair[i].second -
                                render_scene_time_pair[i].first);
  }
  for (size_t i = 0; i < filtering_time_pair.size(); i++) {
    filtering_time.push_back(filtering_time_pair[i].second -
                             filtering_time_pair[i].first);
  }
  for (size_t i = 0; i < tone_mapping_time_pair.size(); i++) {
    tone_mapping_time.push_back(tone_mapping_time_pair[i].second -
                                tone_mapping_time_pair[i].first);
  }
  for (size_t i = 0; i < export_image_time_pair.size(); i++) {
    export_image_time.push_back(export_image_time_pair[i].second -
                                export_image_time_pair[i].first);
  }

  FP_PRECISION mean_render_scene_time = 0.0;
  FP_PRECISION mean_filtering_time = 0.0;
  FP_PRECISION mean_tone_mapping_time = 0.0;
  FP_PRECISION mean_export_image_time = 0.0;

  for (size_t i = 0; i < render_scene_time.size(); i++) {
    mean_render_scene_time += render_scene_time[i];
  }
  if (!render_scene_time.empty()) mean_render_scene_time /= render_scene_time.size();

  for (size_t i = 0; i < filtering_time.size(); i++) {
    mean_filtering_time += filtering_time[i];
  }
  if (!filtering_time.empty()) mean_filtering_time /= filtering_time.size();

  for (size_t i = 0; i < tone_mapping_time.size(); i++) {
    mean_tone_mapping_time += tone_mapping_time[i];
  }
  if (!tone_mapping_time.empty()) mean_tone_mapping_time /= tone_mapping_time.size();

  for (size_t i = 0; i < export_image_time.size(); i++) {
    mean_export_image_time += export_image_time[i];
  }
  if (!export_image_time.empty()) mean_export_image_time /= export_image_time.size();

  std::cout << "\n=== Timing Results ===" << std::endl;
  std::cout << "Parse Scene File: " << parse_scene_file_time << " ms" << std::endl;
  std::cout << "Load Scene: " << load_scene_time << " ms" << std::endl;
  std::cout << "Preprocess Scene: " << preprocess_scene_time << " ms" << std::endl;
  std::cout << "Render Scene: " << mean_render_scene_time << " ms" << std::endl;
  std::cout << "Filtering: " << mean_filtering_time << " ms" << std::endl;
  std::cout << "Tone Mapping: " << mean_tone_mapping_time << " ms" << std::endl;
  std::cout << "Export Image: " << mean_export_image_time << " ms" << std::endl;
}