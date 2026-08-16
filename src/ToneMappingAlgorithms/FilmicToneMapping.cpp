#include "FilmicToneMapping.hpp"

#include <functional>

void FilmicToneMapping::ApplyToneMapping(Vec3f* image_data){
    std::vector<FP_PRECISION> Lw(width_ * height_);
    for(int i = 0; i < width_ * height_; i++){
        Lw[i] = 0.2126 * image_data[i].x + 0.7152 * image_data[i].y + 0.0722 * image_data[i].z;
    }
    FP_PRECISION delta = 0.00001;
    FP_PRECISION log_average_Lw = 0.0;
    for(int i = 0; i < width_ * height_; i++){
        log_average_Lw += log(delta + Lw[i]);
    }
    log_average_Lw = exp(log_average_Lw / (width_ * height_));
    for(int i = 0; i < width_ * height_; i++){
        Lw[i] = (key_value_ / log_average_Lw) * Lw[i];
    }
    // a = 0.22;
    // b = 0.30;
    // c = 0.10;
    // d = 0.20;
    // e = 0.01;
    // f = 0.30;
    // map(L) = ((L(aL + cb) + de)/(L(aL + b) + df)) – e/f;

    std::function<FP_PRECISION(FP_PRECISION)> map = [](FP_PRECISION L){
        FP_PRECISION a = 0.22;
        FP_PRECISION b = 0.30;
        FP_PRECISION c = 0.10;
        FP_PRECISION d = 0.20;
        FP_PRECISION e = 0.01;
        FP_PRECISION f = 0.30;
        return ((L * (a * L + c * b) + d * e) / (L * (a * L + b) + d * f)) - e / f;
    };

    if(burn_ > 0.0){
        FP_PRECISION l_white = 0.0;
        std::vector<FP_PRECISION> Lw_sorted = Lw;
        std::sort(Lw_sorted.begin(), Lw_sorted.end());
        int burn_index = static_cast<int>((1.0 - burn_/100.0) * Lw_sorted.size());
        l_white = Lw_sorted[burn_index];

        for(int i = 0; i < width_ * height_; i++){
            Lw[i] = map(Lw[i]) / map(l_white);
        }
    } else {
        for(int i = 0; i < width_ * height_; i++){
            Lw[i] = map(Lw[i]);
        }
    }
    
    for (int i = 0; i < width_ * height_; i++) {
        WriteTonemappedPixel(i, image_data[i], Lw[i]);
    }
}