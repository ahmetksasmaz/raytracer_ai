#include "ACESToneMapping.hpp"

void ACESToneMapping::ApplyToneMapping(Vec3f* image_data){
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
    // A = 2.51
    // B = 0.03
    // C = 2.43
    // D = 0.59
    // E = 0.14
    // map(L) = ((L(LA+B))/(L(LC+D)+E))

    std::function<FP_PRECISION(FP_PRECISION)> map = [](FP_PRECISION L){
        FP_PRECISION A = 2.51;
        FP_PRECISION B = 0.03;
        FP_PRECISION C = 2.43;
        FP_PRECISION D = 0.59;
        FP_PRECISION E = 0.14;
        return (L * (L * A + B)) / (L * (L * C + D) + E);
    };

    FP_PRECISION l_white = 0.0;
    std::vector<FP_PRECISION> Lw_sorted = Lw;
    std::sort(Lw_sorted.begin(), Lw_sorted.end());
    int burn_index = static_cast<int>((1.0 - burn_/100.0) * Lw_sorted.size());
    l_white = Lw_sorted[burn_index];

    for(int i = 0; i < width_ * height_; i++){
        Lw[i] = map(Lw[i]) / map(l_white);
    }
    
    for(int i = 0; i < width_ * height_; i++){
        FP_PRECISION r = image_data[i].x;
        FP_PRECISION g = image_data[i].y;
        FP_PRECISION b = image_data[i].z;
        FP_PRECISION L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        FP_PRECISION r_coeff = r / L;
        FP_PRECISION g_coeff = g / L;
        FP_PRECISION b_coeff = b / L;
        FP_PRECISION saturated_r = Lw[i] * pow(r_coeff, saturation_);
        FP_PRECISION saturated_g = Lw[i] * pow(g_coeff, saturation_);
        FP_PRECISION saturated_b = Lw[i] * pow(b_coeff, saturation_);
        FP_PRECISION gamma_corrected_r = 255 * pow(std::min(std::max(saturated_r, 0.0), 1.0), 1.0 / gamma_);
        FP_PRECISION gamma_corrected_g = 255 * pow(std::min(std::max(saturated_g, 0.0), 1.0), 1.0 / gamma_);
        FP_PRECISION gamma_corrected_b = 255 * pow(std::min(std::max(saturated_b, 0.0), 1.0), 1.0 / gamma_);
        tonemapped_image_data_[i * 3 + 0] = (unsigned char)(gamma_corrected_r);
        tonemapped_image_data_[i * 3 + 1] = (unsigned char)(gamma_corrected_g);
        tonemapped_image_data_[i * 3 + 2] = (unsigned char)(gamma_corrected_b);
    }
}