#pragma once

#include <opendaq/module_exports.h>


/**
 * @brief Sets the license hash for demonstration purposes only.
 * 
 * This function is used to set a license hash by providing a buffer and its size.
 * It is intended for demonstration purposes and should not be used in production.
 * 
 * @param hashSize The size of the hash buffer in bytes.
 * @param hashBuffer A pointer to the buffer containing the license hash.
 *                   The buffer must be pre-allocated and have a size equal to or greater than `hashSize`.
 * 
 * @return Returns a daq::ErrCode indicating the success or failure of the operation.
 * @remark Alternatively, you can use the 'DEBUG_SET_LICENSE_MODULE_HASH' environment variable to set the license hash.
 */
OPENDAQ_MODULE_API daq::ErrCode demoOnlySetLicenseHash(const uint32_t hashSize, uint8_t* hashBuffer);

/**
 * @brief Checks the dependencies of the module and initialize itself. It is therefore mandatory to call this function before using the module.
 * 
 * This function is called by the OpenDAQ's module manager to verify the dependencies of the module.
 * It checks if the license library exists and if its digital signature is valid (on Windows only).
 * 
 * @param errMsg Out-parameter that may contain an error message if the verification fails.
 * 
 * @return Returns a daq::ErrCode indicating the success or failure of the operation.
 */
OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg);

/**
 * @brief Creates an instance of the module and initializes it with the provided context.
 * 
 * @param[out] module Pointer to a pointer where the created module instance will be stored.
 *                    The caller is responsible for managing the lifetime of the created module.
 * @param[in] context Pointer to the context object that provides necessary resources and 
 *                    configurations for the module.
 * 
 * @return Returns an error code of type daq::ErrCode indicating the success or failure of the operation.
 */
OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context);

/**
 * @brief Same as 'createModule' but with a more descriptive name.
 */
OPENDAQ_MODULE_API daq::ErrCode createProtectedAnalyticsModule(daq::IModule** module, daq::IContext* context);

//Please note that you can verify exports on Windows in the VS command-prompt using:
//  dumpbin /exports <path-to-dll>
