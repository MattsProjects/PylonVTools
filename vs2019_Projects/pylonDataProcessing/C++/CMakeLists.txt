cmake_minimum_required(VERSION 3.12)

# Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Setup project and summary
include(FeatureSummary)
project(PylonDataProcessingSamples)

# Force C++ 17 standard, no fall back, don´t use compiler extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Try to find needed packages
find_package(pylon REQUIRED)
find_package(pylonDataProcessing REQUIRED)

# Add samples subfolders
add_subdirectory(barcode)
add_subdirectory(camera)
add_subdirectory(compositedatatypes)

# Output summary
feature_summary(INCLUDE_QUIET_PACKAGES WHAT
  PACKAGES_FOUND PACKAGES_NOT_FOUND
  ENABLED_FEATURES DISABLED_FEATURES
)
