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
            /ZI         #Enable Hot Reload per default (see: ) https://learn.microsoft.com/en-us/visualstudio/debugger/edit-and-continue-visual-cpp?view=vs-2022#BKMK_Enable_or_disable_automatic_invocation_of_Edit_and_Continue
    )
endif()

target_link_libraries(${LIB_NAME} 
    PUBLIC
        daq::opendaq
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
    PRIVATE
        $<BUILD_INTERFACE:Boost::dll>        
)

# Copy license file to build output directory
set(LICENSE_FILE ${PROJECT_SOURCE_DIR}/licenses/license.lic)

# Add license file as a dependency
set_property(
    TARGET ${LIB_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/license.lic
)

# Install the license file
install(
    FILES ${LICENSE_FILE}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#Remove 'module' from output binary file name (See: cmake\openDAQUtils.cmake to understand the next line)
set(OPENDAQ_MODULE_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})

#Add post-build step to create a signed version of the library
if(BUILD_64Bit)     #Required check as "64" is part of the file name...
    if (WIN32)
        #Please note that depending on the build configuration the output file name may or maynot contain "debug". There, we must use the wildcard copying approach...
        string(REPLACE "/" "\\" OUTPUT_DIR "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/$(Configuration)")

        #set(FULL_MODULE_OUTPUT_PATH "${OUTPUT_DIR}\\${LIB_NAME}-64-${PROJECT_VERSION_MAJOR}*${OPENDAQ_MODULE_SUFFIX}")
        set(FULL_MODULE_SIGNED_PATH "${OUTPUT_DIR}\\${LIB_NAME}-64-${PROJECT_VERSION_MAJOR}-signed${OPENDAQ_MODULE_SUFFIX}")

        set(FULL_CERT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../certs/Selfsigned-CA.pfx")
        string(REPLACE "/" "\\" FULL_CERT_PATH ${FULL_CERT_PATH})

        add_custom_command(TARGET ${LIB_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LICENSE_FILE} ${OUTPUT_DIR}

            # The following command uses xcopy to be able to copy the output file (which may contain "debug" in the name) to the signed version.
            # The standard does not work for this approach. However, even with xcopy we get a prompt: 
            #       "Specify a file name or directory name on the target (F = file, D = directory)?"
            # In oder to suppress this prompt, we use the "echo F |" command before the xcopy command. This approach might
            # fail depending on the operating system (e.g. "File" in German is "Datei").
            # (Also see: https://stackoverflow.com/questions/3018289/xcopy-file-rename-suppress-does-xxx-specify-a-file-name-message)
            COMMAND (echo F | xcopy /Y $<TARGET_FILE:${LIB_NAME}> ${FULL_MODULE_SIGNED_PATH}) | echo "Done copying ..."
            #COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${LIB_NAME}> ${FULL_MODULE_SIGNED_PATH} <- does not work!
            COMMAND ${CMAKE_COMMAND} -E echo "Start signing ${FULL_MODULE_SIGNED_PATH} with ${FULL_CERT_PATH}... "
            COMMAND signtool sign /v /n "Test Certificate Authority (CA)" /f ${FULL_CERT_PATH} /p Passw0rd123 /fd SHA256 /t http://timestamp.digicert.com ${FULL_MODULE_SIGNED_PATH}
        )
    elseif(LINUX)
        #See: "function(opendaq_set_output_lib_name" in cmake/openDAQUtils.cmake)
        #CMAKE_DEBUG_POSTFIX -> "-debug"
        #CMAKE_SHARED_MODULE_PREFIX -> "lib"
        #set(FULL_MODULE_OUTPUT_PATH "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_SHARED_MODULE_PREFIX}${LIB_NAME}-64-${PROJECT_VERSION_MAJOR}-debug${OPENDAQ_MODULE_SUFFIX}")
        set(FULL_MODULE_SIGNED_PATH "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_SHARED_MODULE_PREFIX}${LIB_NAME}-64-${PROJECT_VERSION_MAJOR}-signed${OPENDAQ_MODULE_SUFFIX}")

        add_custom_command(TARGET ${LIB_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LICENSE_FILE} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${LIB_NAME}> ${FULL_MODULE_SIGNED_PATH}
        )
    else()
        message(STATUS "Automatic signing as part of the build script has only been implemented for 64bit Windows|Linux platforms!")
    endif()
else()
        message(STATUS "Automatic signing as part of the build script has not been implemented for none 64bit builds!")
endif()