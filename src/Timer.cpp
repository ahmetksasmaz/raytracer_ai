#include "Timer.hpp"

void Timer::AddTimeLog(Section section, Event event, int camera_id,
                       int pixel_id, int ray_id) {
  TimeLog time_log;
  time_log.section_ = section;
  time_log.event_ = event;
  time_log.camera_id_ = camera_id;
  time_log.pixel_id_ = pixel_id;
  time_log.ray_id_ = ray_id;
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
  std::sort(
      time_logs_.begin(), time_logs_.end(),
      [](const TimeLog& a, const TimeLog& b) { return a.ray_id_ < b.ray_id_; });
  std::sort(time_logs_.begin(), time_logs_.end(),
            [](const TimeLog& a, const TimeLog& b) {
              return a.pixel_id_ < b.pixel_id_;
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

  std::pair<uint64_t, uint64_t> parse_xml_time_pair;
  std::pair<uint64_t, uint64_t> load_scene_time_pair;
  std::pair<uint64_t, uint64_t> preprocess_scene_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> render_scene_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> tone_mapping_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> filtering_time_pair;
  std::vector<std::pair<uint64_t, uint64_t>> export_image_time_pair;
  std::vector<std::vector<std::vector<std::pair<uint64_t, uint64_t>>>>
      ray_tracing_time_pair;

  for (int i = 0; i < time_logs_.size(); i++) {
    if (time_logs_[i].section_ == Section::kParseXML) {
      switch (time_logs_[i].event_) {
        case Event::kStart:
          parse_xml_time_pair.first = time_logs_[i].timestamp_;
          break;
        case Event::kEnd:
          parse_xml_time_pair.second = time_logs_[i].timestamp_;
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
      if (render_scene_time_pair.size() <= time_logs_[i].camera_id_) {
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
    } else if (time_logs_[i].section_ == Section::kRayTracing) {
      if (ray_tracing_time_pair.size() <= time_logs_[i].camera_id_) {
        ray_tracing_time_pair.resize(time_logs_[i].camera_id_ + 1);
      }
      if (ray_tracing_time_pair[time_logs_[i].camera_id_].size() <=
          time_logs_[i].pixel_id_) {
        ray_tracing_time_pair[time_logs_[i].camera_id_].resize(
            time_logs_[i].pixel_id_ + 1);
      }
      if (ray_tracing_time_pair[time_logs_[i].camera_id_]
                               [time_logs_[i].pixel_id_]
                                   .size() <= time_logs_[i].ray_id_) {
        ray_tracing_time_pair[time_logs_[i].camera_id_][time_logs_[i].pixel_id_]
            .resize(time_logs_[i].ray_id_ + 1);
      }
      switch (time_logs_[i].event_) {
        case Event::kStart:
          ray_tracing_time_pair[time_logs_[i].camera_id_]
                               [time_logs_[i].pixel_id_][time_logs_[i].ray_id_]
                                   .first = time_logs_[i].timestamp_;
          break;
        case Event::kEnd:
          ray_tracing_time_pair[time_logs_[i].camera_id_]
                               [time_logs_[i].pixel_id_][time_logs_[i].ray_id_]
                                   .second = time_logs_[i].timestamp_;
          break;
      }
    } else if (time_logs_[i].section_ == Section::kFiltering) {
      if (filtering_time_pair.size() <= time_logs_[i].camera_id_) {
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

    else if (time_logs_[i].section_ == Section::kToneMapping) {
      if (tone_mapping_time_pair.size() <= time_logs_[i].camera_id_) {
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
    } else if (time_logs_[i].section_ == Section::kExportImage) {
      if (export_image_time_pair.size() <= time_logs_[i].camera_id_) {
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

  uint64_t parse_xml_time;
  uint64_t load_scene_time;
  uint64_t preprocess_scene_time;
  std::vector<uint64_t> render_scene_time;
  std::vector<uint64_t> filtering_time;
  std::vector<uint64_t> tone_mapping_time;
  std::vector<uint64_t> export_image_time;
  std::vector<std::vector<std::vector<uint64_t>>> ray_tracing_time;

  parse_xml_time = parse_xml_time_pair.second - parse_xml_time_pair.first;
  load_scene_time = load_scene_time_pair.second - load_scene_time_pair.first;
  preprocess_scene_time =
      preprocess_scene_time_pair.second - preprocess_scene_time_pair.first;
  for (int i = 0; i < render_scene_time_pair.size(); i++) {
    render_scene_time.push_back(render_scene_time_pair[i].second -
                                render_scene_time_pair[i].first);
  }
  for (int i = 0; i < filtering_time_pair.size(); i++) {
    filtering_time.push_back(filtering_time_pair[i].second -
                             filtering_time_pair[i].first);
  }
  for (int i = 0; i < tone_mapping_time_pair.size(); i++) {
    tone_mapping_time.push_back(tone_mapping_time_pair[i].second -
                                tone_mapping_time_pair[i].first);
  }
  for (int i = 0; i < export_image_time_pair.size(); i++) {
    export_image_time.push_back(export_image_time_pair[i].second -
                                export_image_time_pair[i].first);
  }
  for (int i = 0; i < ray_tracing_time_pair.size(); i++) {
    ray_tracing_time.push_back(std::vector<std::vector<uint64_t>>());
    for (int j = 0; j < ray_tracing_time_pair[i].size(); j++) {
      ray_tracing_time[i].push_back(std::vector<uint64_t>());
      for (int k = 0; k < ray_tracing_time_pair[i][j].size(); k++) {
        ray_tracing_time[i][j].push_back(ray_tracing_time_pair[i][j][k].second -
                                         ray_tracing_time_pair[i][j][k].first);
      }
    }
  }

  float mean_render_scene_time = 0.0;
  float std_dev_render_scene_time = 0.0;
  float mean_filtering_time = 0.0;
  float std_dev_filtering_time = 0.0;
  float mean_tone_mapping_time = 0.0;
  float std_dev_tone_mapping_time = 0.0;
  float mean_export_image_time = 0.0;
  float std_dev_export_image_time = 0.0;

  for (int i = 0; i < render_scene_time.size(); i++) {
    mean_render_scene_time += render_scene_time[i];
  }
  mean_render_scene_time /= render_scene_time.size();
  for (int i = 0; i < render_scene_time.size(); i++) {
    std_dev_render_scene_time +=
        (render_scene_time[i] - mean_render_scene_time) *
        (render_scene_time[i] - mean_render_scene_time);
  }
  std_dev_render_scene_time =
      sqrt(std_dev_render_scene_time / render_scene_time.size());

  for (int i = 0; i < filtering_time.size(); i++) {
    mean_filtering_time += filtering_time[i];
  }
  mean_filtering_time /= filtering_time.size();
  for (int i = 0; i < filtering_time.size(); i++) {
    std_dev_filtering_time += (filtering_time[i] - mean_filtering_time) *
                              (filtering_time[i] - mean_filtering_time);
  }
  std_dev_filtering_time = sqrt(std_dev_filtering_time / filtering_time.size());

  for (int i = 0; i < tone_mapping_time.size(); i++) {
    mean_tone_mapping_time += tone_mapping_time[i];
  }
  mean_tone_mapping_time /= tone_mapping_time.size();
  for (int i = 0; i < tone_mapping_time.size(); i++) {
    std_dev_tone_mapping_time +=
        (tone_mapping_time[i] - mean_tone_mapping_time) *
        (tone_mapping_time[i] - mean_tone_mapping_time);
  }
  std_dev_tone_mapping_time =
      sqrt(std_dev_tone_mapping_time / tone_mapping_time.size());

  for (int i = 0; i < export_image_time.size(); i++) {
    mean_export_image_time += export_image_time[i];
  }
  mean_export_image_time /= export_image_time.size();
  for (int i = 0; i < export_image_time.size(); i++) {
    std_dev_export_image_time +=
        (export_image_time[i] - mean_export_image_time) *
        (export_image_time[i] - mean_export_image_time);
  }
  std_dev_export_image_time =
      sqrt(std_dev_export_image_time / export_image_time.size());

  float mean_ray_tracing_time = 0.0;
  float std_dev_ray_tracing_time = 0.0;
  float mean_ray_tracing_time_per_pixel = 0.0;
  float std_dev_ray_tracing_time_per_pixel = 0.0;

  uint64_t ray_count = 0;
  for (int i = 0; i < ray_tracing_time.size(); i++) {
    for (int j = 0; j < ray_tracing_time[i].size(); j++) {
      for (int k = 0; k < ray_tracing_time[i][j].size(); k++) {
        mean_ray_tracing_time += ray_tracing_time[i][j][k];
        ray_count++;
      }
    }
  }
  mean_ray_tracing_time /= ray_count;

  for (int i = 0; i < ray_tracing_time.size(); i++) {
    for (int j = 0; j < ray_tracing_time[i].size(); j++) {
      for (int k = 0; k < ray_tracing_time[i][j].size(); k++) {
        std_dev_ray_tracing_time +=
            (ray_tracing_time[i][j][k] - mean_ray_tracing_time) *
            (ray_tracing_time[i][j][k] - mean_ray_tracing_time);
      }
    }
  }
  std_dev_ray_tracing_time = sqrt(std_dev_ray_tracing_time / ray_count);

  uint64_t pixel_count = 0;
  for (int i = 0; i < ray_tracing_time.size(); i++) {
    for (int j = 0; j < ray_tracing_time[i].size(); j++) {
      float temp = 0.0;
      for (int k = 0; k < ray_tracing_time[i][j].size(); k++) {
        temp += ray_tracing_time[i][j][k];
      }
      mean_ray_tracing_time_per_pixel += temp;
      pixel_count++;
    }
  }
  mean_ray_tracing_time_per_pixel /= pixel_count;

  for (int i = 0; i < ray_tracing_time.size(); i++) {
    for (int j = 0; j < ray_tracing_time[i].size(); j++) {
      float temp = 0.0;
      for (int k = 0; k < ray_tracing_time[i][j].size(); k++) {
        temp += ray_tracing_time[i][j][k];
      }
      std_dev_ray_tracing_time_per_pixel +=
          (temp - mean_ray_tracing_time_per_pixel) *
          (temp - mean_ray_tracing_time_per_pixel);
    }
  }
  std_dev_ray_tracing_time_per_pixel =
      sqrt(std_dev_ray_tracing_time_per_pixel / pixel_count);

  std::cout << "Parse XML: " << parse_xml_time << " ms" << std::endl;
  std::cout << "Load Scene: " << load_scene_time << " ms" << std::endl;
  std::cout << "Preprocess Scene: " << preprocess_scene_time << " ms"
            << std::endl;
  std::cout << "Render Scene: " << mean_render_scene_time << " ms - "
            << std_dev_render_scene_time << " ms" << std::endl;
  std::cout << "Filtering: " << mean_filtering_time << " ms - "
            << std_dev_filtering_time << " ms" << std::endl;
  std::cout << "Tone Mapping: " << mean_tone_mapping_time << " ms - "
            << std_dev_tone_mapping_time << " ms" << std::endl;
  std::cout << "Export Image: " << mean_export_image_time << " ms - "
            << std_dev_export_image_time << " ms" << std::endl;
  std::cout << "Ray Tracing: " << mean_ray_tracing_time << " ms - "
            << std_dev_ray_tracing_time << " ms" << std::endl;
  std::cout << "Ray Tracing per Pixel: " << mean_ray_tracing_time_per_pixel
            << " ms - " << std_dev_ray_tracing_time_per_pixel << " ms"
            << std::endl;
}