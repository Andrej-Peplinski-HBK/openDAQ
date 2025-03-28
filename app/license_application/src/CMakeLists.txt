set(APP_NAME license_application)

set(SRC_Headers 
    #utils.h
    #boost_dll.h
)

set(SRC_Cpp main.cpp
            #utils.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Headers)

add_executable(${APP_NAME} 
    ${SRC_Headers}
    ${SRC_Cpp}
)

target_include_directories(${APP_NAME} 
    PUBLIC 
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/>
)

target_compile_definitions(${APP_NAME} 
    PUBLIC
        BOOST_DLL_USE_STD_FS
        BOOST_ALL_NO_LIB
)

target_link_libraries(${APP_NAME} 
    PRIVATE 
        daq::opendaq
        $<BUILD_INTERFACE:Boost::dll>
)

add_dependencies(${APP_NAME} 
    daq::ProtectedAnalyticsModule
)

