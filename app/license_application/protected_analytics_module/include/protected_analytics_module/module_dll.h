#pragma once

#include <opendaq/module_exports.h>

OPENDAQ_MODULE_API daq::ErrCode demoOnlySetLicenseHash(const uint32_t hashSize, uint8_t* hashBuffer);

OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg);

OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context);
OPENDAQ_MODULE_API daq::ErrCode createProtectedAnalyticsModule(daq::IModule** module, daq::IContext* context);

//OPENDAQ_MODULE_API daq::ErrCode daqGetObjectCount(daq::SizeT* objCount);
// 👆 Not needed for now ... 👆
//Please note that you can verify exports on Windows in the VS command-prompt using:
//  dumpbin /exports "C:\HBK\dev\SourceCode\GitHub\openDAQ\build\x64\msvc-22\full\bin\Debug\ProtectedAnalyticsModule-64-3-debug.module.dll"
