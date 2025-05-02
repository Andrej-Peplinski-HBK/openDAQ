#include <iostream>
#include <mutex>

#include <coretypes/filesystem.h>
#include <coretypes/stringobject_factory.h>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/filesystem.hpp>

#include <license_library/license_checker.h>
#include <protected_analytics_module/license_module_verification.h>
#include <protected_analytics_module/module_dll.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using namespace daq::modules::protected_analytics_module;
using namespace daq::modules::license_library;

std::shared_ptr<spdlog::logger> _logger = spdlog::stdout_color_mt("LicenseCheckerModule");
std::mutex mtx;
uint8_t expected_license_hashBuffer[32] = {0};
boost::dll::shared_library licenseCheckerLibrary;
daq::ObjectPtr<ILicenseChecker> licenseCheckerPtr;

#ifdef WIN32

#include <wtypes.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstance*/, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
    switch (fdwReason)
    {
        // case DLL_PROCESS_ATTACH:
        //     break;
        case DLL_PROCESS_DETACH:
            licenseCheckerPtr = nullptr;
            licenseCheckerLibrary.unload();
            break;
    }
    return TRUE;
}
#else
// See:
//  * https://www.opengate.at/blog/2020/03/dllmain/
//  * https://codeberg.org/GateNetwork/gate-blog-classroom/src/branch/main/c_cpp/dllmain_linux/
void __attribute__((constructor)) SO_init()
{
    /* do some global initialization */
}

void __attribute__((destructor)) SO_uninit()
{
    licenseCheckerPtr = nullptr;
    licenseCheckerLibrary.unload();
}
#endif

OPENDAQ_MODULE_API daq::ErrCode demoOnlySetLicenseHash(const uint32_t hashSize, uint8_t* hashBuffer)
{
    constexpr const auto MAX_HASH_SIZE = sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]);
    if (hashSize > MAX_HASH_SIZE)
    {
        _logger->error("Hash size is too large!");
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mtx);

    // First make sure that any previous hash is cleared
    std::memset(expected_license_hashBuffer, 0, sizeof(expected_license_hashBuffer));
    std::memcpy(expected_license_hashBuffer, hashBuffer, hashSize * sizeof(hashBuffer[0]));

    _logger->info("Successfully initialized the hash externally.");
    _logger->info("This just happens in this demo application to allow users to");
    _logger->info("provide their own hash key stemming from a custom certificate.");
    _logger->info("In a real application one would 'embed' the hash key into the application though...");

    return OPENDAQ_SUCCESS;
}
OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg)
{
    std::lock_guard<std::mutex> lock(mtx);

    const auto programLocation = boost::dll::program_location();
    const auto exeParentDir = boost::filesystem::absolute(programLocation.c_str()).parent_path();
#ifdef WIN32
    const auto moduleSimpleName = "LicenseLibrary-64-3-signed.dll";
#else
    const auto moduleSimpleName = "libLicenseLibrary-64-3-signed.so";
#endif
    const auto fullLicPath = exeParentDir / moduleSimpleName;
        

    licenseCheckerPtr = nullptr;

    std::error_code errorCode;
    licenseCheckerLibrary = boost::dll::shared_library(fullLicPath.c_str(), errorCode);
    if (errorCode)
    {
        *errMsg = daq::String(fmt::format("Failed to load '{0}' (details: '{1}', code: {2}, category: {3})!",
                                          fullLicPath.string(),
                                          errorCode.message(),
                                          errorCode.value(),
                                          errorCode.category().name()))
                      .addRefAndReturn();
        return OPENDAQ_ERR_RESOLVEFAILED;  // 👈 Here it would be beneficial to have a more specific error code for a Missing License file
    }

    // Check digital signature
    auto pLastHashValue = &expected_license_hashBuffer[sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]) - 1];
    while (*pLastHashValue == 0)
    {
        if (--pLastHashValue == expected_license_hashBuffer)
            break;
    }
    std::vector<uint8_t> vecExpected_license_hashBuffer(expected_license_hashBuffer, pLastHashValue + 1);

    auto retVal = CanTrustLicenseModule(licenseCheckerLibrary.location(), vecExpected_license_hashBuffer, errMsg);
    if (OPENDAQ_FAILED(retVal))
        return retVal;

    // Try create the license component (now that we are sure that the module is valid)
    {
        const auto fctName = "createLicenseChecker";
        if (!licenseCheckerLibrary.has(fctName))
        {
            licenseCheckerLibrary.unload();
            *errMsg =
                daq::String(
                    fmt::format("License '{0}' is not a valid module (details: '{1}' export is missing)!", fullLicPath.string(), fctName))
                    .addRefAndReturn();
            return OPENDAQ_ERR_RESOLVEFAILED;
        }

        using CreateLicenseCheckerFunc = daq::ErrCode (*)(ILicenseChecker**);
        CreateLicenseCheckerFunc createLicenseChecker = licenseCheckerLibrary.get<daq::ErrCode(ILicenseChecker**)>(fctName);

        retVal = createLicenseChecker(&licenseCheckerPtr);
        if (OPENDAQ_FAILED(retVal))
        {
            licenseCheckerLibrary.unload();
            *errMsg =
                daq::String(
                    fmt::format("License '{0}' is not a valid module (details: Call to '{1}' failed)!", fullLicPath.string(), fctName))
                    .addRefAndReturn();
            return retVal;
        }
    }

    return retVal;
}

OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context)
{
    // ToDo: !!!Refuse to create the module if the license is not valid!!!
    if (licenseCheckerPtr == nullptr)
    {
        _logger->error("Please call the 'checkDependencies' function first to verify that this module can actually be loaded and used!");
        return OPENDAQ_ERR_CREATE_FAILED;
    }

    const auto errorCode = daq::createObject<daq::IModule, ProtectedAnalyticsModule>(module, context);
    if (OPENDAQ_SUCCEEDED(errorCode))
    {
        const auto pProtectedAnalyticsModule = dynamic_cast<ProtectedAnalyticsModule*>(*module);
        // Please note that we pass in a non-reference counted pointer of the license checker because
        // the life time of 'pProtectedAnalyticsModule' MUST be shorter than the unloading of the module
        // and we ensure that the license checker is unloaded when the module is unloaded (see: DllMain)
        pProtectedAnalyticsModule->setLicenseComponent(licenseCheckerPtr);
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
