set(APP_LICENSE license_application)

set(SRC_Headers 
    #utils.h
)

set(SRC_Cpp main.cpp
            #utils.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Headers)
add_executable(${APP_LICENSE} ${SRC_Headers}
                              ${SRC_Cpp}
)

target_include_directories(${APP_LICENSE} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
                                                 $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>
)

target_link_libraries(${APP_LICENSE} PRIVATE daq::opendaq
)

add_dependencies(${APP_LICENSE} daq::ProtectedAnalyticsModule
)

