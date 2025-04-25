#cmake_minimum_required(VERSION 3.2)

set_cmake_folder_context(TARGET_FOLDER_NAME)

project(LicenseLibrary VERSION ${OPENDAQ_PACKAGE_VERSION} LANGUAGES CXX)

add_subdirectory(src)