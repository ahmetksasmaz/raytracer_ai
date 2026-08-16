#include "EXRExporter.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "ImageIO/ImageIO.hpp"

void EXRExporter::Export(const std::string &filename, const int width, const int height, const float *float_data) const {
    std::string extension = filename.substr(filename.find_last_of(".") + 1);

    std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    if (extension != "exr" && extension != "hdr") {
        std::cerr << "Unsupported file format '" << extension << "' for "
                  << filename << "; HDR output is EXR." << std::endl;
        return;
    }

    // Interleaved RGB in, one plane per channel out. The shared writer sorts
    // channels by name, which gives the B, G, R order EXR viewers expect
    // without this having to arrange it.
    image_io::ImagePlanes image;
    image.width = width;
    image.height = height;
    const size_t pixel_count = static_cast<size_t>(width) * height;

    const char *names[3] = {"R", "G", "B"};
    for (int c = 0; c < 3; c++) {
        std::vector<float> plane(pixel_count);
        for (size_t i = 0; i < pixel_count; i++) plane[i] = float_data[i * 3 + c];
        image.names.push_back(names[c]);
        image.planes.push_back(std::move(plane));
    }

    // Note this is now full float where it used to request half. The sensor and
    // spectral products were always full float; having the conventional output
    // alone be half meant two EXRs from the same render quantised differently.
    std::string error;
    if (!image_io::WriteMultiChannelEXR(filename, image, &error)) {
        throw std::runtime_error("Failed to save EXR image: " + error);
    }
}
