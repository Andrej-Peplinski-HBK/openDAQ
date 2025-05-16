#pragma once

#include <coretypes/common.h>               // For: daq::ErrCode
#include <coretypes/filesystem.h>           // For: fs::path
#include <coretypes/stringobject_factory.h>  // For: daq::IString

/// @brief Verifies that the license module specified by @path corresponds to the expected license hash.
/// @param path The path to the license module.
/// @param expected_license_hashBuffer The expected license hash (in this demo we pass it in from the outside - but in a real environment this value would either be hardcode or read from a embedded resource)
/// @param errMsg Out-parameter that may contain an error message if the verification fails.
/// @return Returns error code (you can use the `OPENDAQ_FAILED` macro to check for success)
daq::ErrCode CanTrustLicenseModule(const fs::path& path, const std::vector<uint8_t>& expected_license_hashBuffer, daq::IString** errMsg);
