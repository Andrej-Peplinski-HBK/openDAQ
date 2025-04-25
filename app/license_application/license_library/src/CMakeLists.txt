set(LIB_NAME LicenseLibrary)
set(MODULE_HEADERS_DIR ../include)
set(MODULE_BINARY_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/../include)

set(SRC_Include common.h
                module_dll.h
                license_checker.h
)

set(SRC_Srcs module_dll.cpp
             license_checker.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Include)

set(MODULE_FILES ${MODULE_HEADERS_DIR}/module_dll.h
                 #${MODULE_HEADERS_DIR}/protected_analytics_module_impl.h
                 #${MODULE_HEADERS_DIR}/license_module_verification.h
                module_dll.cpp)

source_group("module" FILES ${MODULE_FILES})

add_library(${LIB_NAME} SHARED ${SRC_Include}
                               ${SRC_Srcs}                               
)

add_library(${SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${LIB_NAME} 
    PUBLIC
        BOOST_DLL_USE_STD_FS
        BOOST_ALL_NO_LIB
)

if (MSVC)
    target_compile_options(${LIB_NAME} 
        PRIVATE 
            /bigobj 
            /wd4100     #Avoid 'unreferenced formal parameter' compiler warning  (Otherwise will not compile in VS in Release build...)        
    )

    # target_link_libraries(${LIB_NAME}
    #     PRIVATE 
    #         wintrust        # Required for License Certificate verification
    #         Crypt32         # Required for License Certificate verification
    # )
endif()

target_link_libraries(${LIB_NAME} 
    PUBLIC
        daq::opendaq
    PRIVATE 
        $<BUILD_INTERFACE:Boost::dll>
)

#See: cmake\openDAQUtils.cmake to understand the next line
set(OPENDAQ_MODULE_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})
