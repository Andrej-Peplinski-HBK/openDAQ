#include <iostream>
#include <mutex>

#include <coretypes/filesystem.h>
#include <coretypes/stringobject_factory.h>
#include <boost/dll/shared_library.hpp>

#include <protected_analytics_module/module_dll.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <protected_analytics_module/license_module_verification.h>

using namespace daq::modules::protected_analytics_module;

std::mutex mtx;
uint8_t expected_license_hashBuffer[32] = {0};

OPENDAQ_MODULE_API daq::ErrCode demoOnlySetLicenseHash(const uint32_t hashSize, uint8_t* hashBuffer)
{
    const auto MAX_HASH_SIZE = sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]);
    if(hashSize > MAX_HASH_SIZE)
    {
        std::cerr << "Hash size is too large!" << std::endl;
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mtx);

    // First make sure that any previous hash is cleared
    std::memset(expected_license_hashBuffer, 0, sizeof(expected_license_hashBuffer));
    std::memcpy(expected_license_hashBuffer, hashBuffer, hashSize*sizeof(hashBuffer[0]));

    return OPENDAQ_SUCCESS;
}
OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg)
{
    std::lock_guard<std::mutex> lock(mtx);

    const auto noneExistingString = L"C:\\DoesNotExist.dll";
    const auto regularFile = L"C:\\HBK\\dev\\SourceCode\\Certificates\\ProtectedAnalyticsModule-64-3-debug.module.dll";
    const auto temperedFile = L"C:\\HBK\\dev\\SourceCode\\Certificates\\ProtectedAnalyticsModule-64-3-debug.module - tempered.dll";
    
    const fs::path path(regularFile);

    std::error_code errorCode;
    boost::dll::shared_library moduleLibrary(path, errorCode);
    if (errorCode)
    {
        *errMsg = daq::String(fmt::format("Failed to load '{0}' (details: '{1}', code: {2}, category: {3})!",
                                          path.string(),
                                          errorCode.message(),
                                          errorCode.value(),
                                          errorCode.category().name()))
                      .addRefAndReturn();
        return OPENDAQ_ERR_RESOLVEFAILED;  // 👈 Here it would be beneficial to have a more specific error code for a Missing License file
    }

    // Check digital signature
    uint8_t* pLastHashValue = &expected_license_hashBuffer[sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]) -1];
    while (*pLastHashValue == 0)
    {
        if (--pLastHashValue == expected_license_hashBuffer)
            break;
    }
    std::vector<uint8_t> vecExpected_license_hashBuffer(expected_license_hashBuffer, pLastHashValue+1);
    
    auto retVal = CanTrustLicenseModule(moduleLibrary.location(), vecExpected_license_hashBuffer, errMsg);
    if (OPENDAQ_FAILED(retVal))
        return retVal;    
    
    // Try create the license component (now that we are sure that the module is valid)

    //const auto moduleHandle = moduleLibrary.native();

    return retVal;
}

OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context)
{
    // ToDo: !!!Refuse to create the module if the license is not valid!!!

    const auto errorCode = daq::createObject<daq::IModule, ProtectedAnalyticsModule>(module, context);
    if (OPENDAQ_SUCCEEDED(errorCode))
    {
        const auto pProtectedAnalyticsModule = dynamic_cast<ProtectedAnalyticsModule*>(*module);
        pProtectedAnalyticsModule->setLicenseComponent(nullptr);
    }

    return errorCode;
}
OPENDAQ_MODULE_API daq::ErrCode createProtectedAnalyticsModule(daq::IModule** module, daq::IContext* context)
{
    return createModule(module, context);
}

#ifdef OPENDAQ_TRACK_SHARED_LIB_OBJECT_COUNT
std::atomic<std::size_t> daq::daqSharedLibObjectCount(0);  // Required otherwise we get a linker error 👉 LNK2001: unresolved external
                                                           // symbol "struct std::atomic<unsigned __int64> daq::daqSharedLibObjectCount"

//  OPENDAQ_MODULE_API daq::ErrCode daqGetObjectCount(daq::SizeT* count)
// {
//      *count = daq::daqSharedLibObjectCount;
//      return OPENDAQ_SUCCESS;
//  }
#endif
