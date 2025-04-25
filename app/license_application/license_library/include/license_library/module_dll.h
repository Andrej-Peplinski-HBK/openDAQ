#pragma once

#include <opendaq/module_exports.h>
#include <license_library/license_checker.h>

OPENDAQ_MODULE_API daq::ErrCode createLicenseChecker(daq::modules::license_library::ILicenseChecker** licenseChecker);