cmake_minimum_required(VERSION 3.16)
project(raytracer_ai CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Every executable lands in build/bin.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(RT_BUILD_TESTS "Build the self-check tools (imgdiff, spectraltest, sensortest)" ON)

# -fno-finite-math-only is required: plain -ffast-math implies
# -ffinite-math-only, which lets the compiler assume NaN and Inf never occur and
# fold every std::isfinite() check to true. Verified on this toolchain --
# without it, isfinite(0.0/0.0) returns 1 and isnan(0.0/0.0) returns 0, so the
# NaN guards throughout the path tracer and tone mappers do nothing at all.
set(RT_FAST_MATH_FLAGS
  -ffast-math -fno-finite-math-only
  -funroll-loops -ftree-vectorize -fomit-frame-pointer)

add_library(rt_flags INTERFACE)
target_compile_options(rt_flags INTERFACE
  $<$<CONFIG:Release>:-O3;-march=native;-mtune=native;${RT_FAST_MATH_FLAGS}>
  $<$<CONFIG:Debug>:-O0;-g>
  -w)
target_include_directories(rt_flags INTERFACE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/extern
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include/BRDFs
  ${CMAKE_SOURCE_DIR}/include/Core
  ${CMAKE_SOURCE_DIR}/include/ImageIO
  ${CMAKE_SOURCE_DIR}/include/ISP
  ${CMAKE_SOURCE_DIR}/include/LightSources
  ${CMAKE_SOURCE_DIR}/include/Materials
  ${CMAKE_SOURCE_DIR}/include/Objects
  ${CMAKE_SOURCE_DIR}/include/Pipeline
  ${CMAKE_SOURCE_DIR}/include/Sensor
  ${CMAKE_SOURCE_DIR}/include/Textures
  ${CMAKE_SOURCE_DIR}/include/ToneMappingAlgorithms)

# --- rt_imageio --------------------------------------------------------------
# The ONE translation unit that instantiates tinyexr and stb. Both are
# single-header libraries whose *_IMPLEMENTATION macro may appear in exactly one
# TU per binary; before the split that was satisfied by accident across three
# files, which is why imgdiff had to be its own build. Centralising it here is
# what makes a dozen executables possible.
add_library(rt_imageio STATIC src/ImageIO/ImageIO.cpp)
target_link_libraries(rt_imageio PUBLIC rt_flags)

# --- rt_spectral -------------------------------------------------------------
# The spectral core and the measured library. No scene, no renderer.
add_library(rt_spectral STATIC src/SpectrumLibrary.cpp)
target_link_libraries(rt_spectral PUBLIC rt_flags)

# --- rt_pipeline -------------------------------------------------------------
# Argument parsing and the plane<->buffer conversions every stage shares.
add_library(rt_pipeline STATIC src/Pipeline/Stage.cpp)
target_link_libraries(rt_pipeline PUBLIC rt_flags rt_imageio rt_spectral)

# --- rt_sensor ---------------------------------------------------------------
# The sensor model, its config format and its colour characterisation.
add_library(rt_sensor STATIC
  src/Sensor/SensorConfig.cpp
  src/Sensor/SensorCalibration.cpp)
target_link_libraries(rt_sensor PUBLIC rt_flags rt_spectral)

# --- rt_isp ------------------------------------------------------------------
add_library(rt_isp STATIC src/ISP/ISP.cpp)
target_link_libraries(rt_isp PUBLIC rt_flags rt_sensor)

# --- rt_render ---------------------------------------------------------------
# The ray tracer proper. Everything under src/ that is not a stage program, a
# library above, or main().
file(GLOB RT_RENDER_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/Objects/*.cpp
  ${CMAKE_SOURCE_DIR}/src/SchedulingAlgorithms/*.cpp
  ${CMAKE_SOURCE_DIR}/src/ToneMappingAlgorithms/*.cpp
  ${CMAKE_SOURCE_DIR}/src/TracingAlgorithms/*.cpp)
list(REMOVE_ITEM RT_RENDER_SOURCES
  ${CMAKE_SOURCE_DIR}/src/RayTracer.cpp
  ${CMAKE_SOURCE_DIR}/src/SpectrumLibrary.cpp)

file(GLOB RT_EXTERN_SOURCES ${CMAKE_SOURCE_DIR}/extern/*.cpp)

add_library(rt_render STATIC ${RT_RENDER_SOURCES} ${RT_EXTERN_SOURCES})
target_link_libraries(rt_render PUBLIC rt_flags rt_imageio rt_spectral rt_pipeline)
find_package(Threads REQUIRED)
target_link_libraries(rt_render PUBLIC Threads::Threads)

# --- Executables -------------------------------------------------------------
add_executable(raytracer src/RayTracer.cpp)
target_link_libraries(raytracer PRIVATE rt_render)

# The sensor stages: spectral radiance in, a RAW Bayer mosaic out. One program
# per physical step, each reading a file and writing a file.
set(RT_SENSOR_STAGES
  sensor_irradiance   # radiance   -> photons
  sensor_cfa          # photons    -> electrons, 3 channels
  sensor_mosaic       # 3 channels -> Bayer mosaic
  sensor_noise        # shot, dark, read
  sensor_saturate     # full-well clamp
  sensor_adc          # gain + quantisation -> RAW
  sensor_ccm          # sensor curves -> white balance gains + colour matrices
  sensor_illum_chroma) # illumination cube -> per-pixel r/g, b/g ground truth

foreach(stage ${RT_SENSOR_STAGES})
  add_executable(${stage} src/apps/${stage}.cpp)
  target_link_libraries(${stage} PRIVATE rt_pipeline rt_sensor)
endforeach()

# The ISP stages: a RAW in, a viewable sRGB image out.
set(RT_ISP_STAGES
  isp_blacklevel      # DN         -> linear [0,1]
  isp_demosaic        # mosaic     -> sensorRGB
  isp_whitebalance    # sensorRGB  -> neutral-corrected sensorRGB
  isp_colormatrix     # sensorRGB  -> CIE XYZ
  isp_srgb            # CIE XYZ    -> 8-bit sRGB
  raw_preview)        # RAW        -> viewable, uncorrected sensor space

foreach(stage ${RT_ISP_STAGES})
  add_executable(${stage} src/apps/${stage}.cpp)
  target_link_libraries(${stage} PRIVATE rt_pipeline rt_isp rt_sensor)
endforeach()

# --- Self-check tools --------------------------------------------------------
# Each has its own main(), so each is its own target.
if(RT_BUILD_TESTS)
  add_executable(imgdiff tools/imgdiff.cpp)
  target_link_libraries(imgdiff PRIVATE rt_imageio)

  add_executable(spectraltest tools/spectraltest.cpp)
  target_link_libraries(spectraltest PRIVATE rt_flags)

  add_executable(sensortest tools/sensortest.cpp)
  target_link_libraries(sensortest PRIVATE rt_flags)

  enable_testing()
  add_test(NAME suite
           COMMAND ${CMAKE_SOURCE_DIR}/tests/run_tests.sh
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
