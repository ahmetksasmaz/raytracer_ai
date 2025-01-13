#include "Scene.hpp"

void Scene::AveragingFilterAlgorithm(PixelSample* image_sampled_data, int image_width,
                                     int image_height, int sample,
                                     Vec3f* image_data) {
//   for (int i = 0; i < image_height; i++) {
//     for (int j = 0; j < image_width; j++) {
//       std::map<int, float> sum;
//       for(auto &pair : image_sampled_data[0].values_){
//         sum.insert(pair.first, 0.0f);
//       }
//       for (int k = 0; k < sample; k++) {
//         PixelSample packet = image_sampled_data[(i * image_width + j) * sample + k];
//         for(auto &pair : packet.values_){
//           sum[pair.first] += pair.second / sample;
//         }
//       }

//       image_data[i * image_width + j] = sum;
//     }
//   }
}