#include <algorithm>
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
char moduleFilePath[260] = {0};
constexpr const auto MAX_MODULE_FILE_PATH = sizeof(moduleFilePath) / sizeof(moduleFilePath[0]);
uint8_t expected_license_hashBuffer[32] = {0};
constexpr const auto MAX_HASH_SIZE = sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]);
boost::dll::shared_library licenseCheckerLibrary;
daq::ObjectPtr<ILicenseChecker> licenseCheckerPtr;

#ifdef WIN32

#include <wtypes.h>

BOOL WINAPI DllMain(HINSTANCE hinstance, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            GetModuleFileNameA(hinstance, moduleFilePath, MAX_MODULE_FILE_PATH);
            break;
        case DLL_PROCESS_DETACH:
            licenseCheckerPtr = nullptr;
            licenseCheckerLibrary.unload();
            break;
    }
    return TRUE;
}
#else

#include <dlfcn.h>
// See:
//  * https://www.opengate.at/blog/2020/03/dllmain/
//  * https://codeberg.org/GateNetwork/gate-blog-classroom/src/branch/main/c_cpp/dllmain_linux/
void __attribute__((constructor)) SO_init()
{
    // Determine the path to the shared library
    Dl_info info;
    if (dladdr((void*)&SO_init, &info))
    {
        std::cout << "Protected Analytics Module loaded from: " << info.dli_fname << std::endl;
        strncpy(moduleFilePath, info.dli_fname, MAX_MODULE_FILE_PATH - 1);
    }
    else
    {
        std::cerr << "Failed to retrieve module path: " << dlerror() << std::endl;
    }
}

void __attribute__((destructor)) SO_uninit()
{
    licenseCheckerPtr = nullptr;
    licenseCheckerLibrary.unload();
}
#endif

OPENDAQ_MODULE_API daq::ErrCode demoOnlySetLicenseHash(const uint32_t hashSize, uint8_t* hashBuffer)
{
    if (hashSize > MAX_HASH_SIZE)
    {
        _logger->error("Hash size is too long!");
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_INVALID_ARGUMENT, "Hash size is too long!");
    }

    std::lock_guard<std::mutex> lock(mtx);

    // First make sure that any previous hash is cleared
    std::memset(expected_license_hashBuffer, 0, sizeof(expected_license_hashBuffer));
    std::memcpy(expected_license_hashBuffer, hashBuffer, hashSize * sizeof(hashBuffer[0]));

    _logger->info(R"(Successfully initialized the hash externally.
                    This just happens in this demo application to allow users to
                    provide their own hash key stemming from a custom certificate.
                    In a real application one would 'embed' the hash key into the application though...
        )");

    return OPENDAQ_SUCCESS;
}
OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg)
{
    // Avoid double initialization of the license module
    if (licenseCheckerPtr != nullptr)
        return OPENDAQ_SUCCESS;

    std::lock_guard<std::mutex> lock(mtx);

    //Determine the file path to the license checker library
    boost::filesystem::path licenseCheckerDir;
    const auto isModuleFilePathUninitialized = std::all_of(std::cbegin(moduleFilePath), std::cend(moduleFilePath), [](auto b) { return b == 0; });
    if (!isModuleFilePathUninitialized)
    {
        licenseCheckerDir = boost::filesystem::absolute(moduleFilePath).parent_path();
        _logger->debug("checkDependencies: Using moduleFilePath {}", licenseCheckerDir.string());
    }
    else
    {
        const auto programLocation = boost::dll::program_location();
        licenseCheckerDir = boost::filesystem::absolute(programLocation.c_str()).parent_path();
        _logger->debug("checkDependencies: Using exe path {}", licenseCheckerDir.string());
    }
    
#ifdef WIN32
    const auto moduleSimpleName = "LicenseLibrary-64-3-signed.dll";
#else
    const auto moduleSimpleName = "libLicenseLibrary-64-3-signed.so";
#endif
    const auto licenseCheckerFullPath = licenseCheckerDir / moduleSimpleName;

    licenseCheckerPtr = nullptr;

    std::error_code errorCode;
    licenseCheckerLibrary = boost::dll::shared_library(licenseCheckerFullPath.c_str(), errorCode);
    if (errorCode)
    {
        auto errMsgReturn = daq::String(fmt::format("Failed to load '{0}' (details: '{1}', code: {2}, category: {3})!",
                                                   licenseCheckerFullPath.string(),
                                                   errorCode.message(),
                                                   errorCode.value(),
                                                   errorCode.category().name()));
        *errMsg = errMsgReturn.addRefAndReturn();
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_RESOLVEFAILED, errMsgReturn);  // 👈 Here it would be beneficial to have a more specific error code for a Missing License file
    }

#pragma region Check digital signature
    // Check if the 'expected_license_hashBuffer' has already been set or else fall back to environment variable
    const auto isLicenseBufferUninitialized = std::all_of(std::cbegin(expected_license_hashBuffer), std::cend(expected_license_hashBuffer), [](auto b) { return b == 0; });
    if (isLicenseBufferUninitialized)
    {
        const auto envLicenseHash = std::getenv("DEBUG_SET_LICENSE_MODULE_HASH");
        if (envLicenseHash == nullptr)
        {
            _logger->error("checkDependencies: License Library has not been set through demoOnlySetLicenseHash() and environment variable 'DEBUG_SET_LICENSE_MODULE_HASH'!");
            *errMsg = daq::String("License Library has not been set through demoOnlySetLicenseHash() nor environment variable 'DEBUG_SET_LICENSE_MODULE_HASH'!").addRefAndReturn();
            return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_INVALID_OPERATION, "License Library has not been set through demoOnlySetLicenseHash() nor environment variable 'DEBUG_SET_LICENSE_MODULE_HASH'!");
        }

        std::string hash(envLicenseHash);
        
        const auto hashVectorSize = hash.size() / 2;
        if (hashVectorSize <= MAX_HASH_SIZE)
        {
            for (size_t j = 0; j < hash.size(); j += 2)
            {
                const auto hashValue = static_cast<uint8_t>(std::stoi(hash.substr(j, 2), nullptr, 16));
                if (hashValue == 0)
                {
                    _logger->error("checkDependencies: Environment variable 'DEBUG_SET_LICENSE_MODULE_HASH' ({}) must not contain 0s!", hash);
                    *errMsg = daq::String("Environment variable 'DEBUG_SET_LICENSE_MODULE_HASH must not contain 0s!").addRefAndReturn();
                    return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_INVALIDVALUE, "Environment variable 'DEBUG_SET_LICENSE_MODULE_HASH' must not contain 0s!");
                }

                expected_license_hashBuffer[j / 2] = hashValue;
            }

            _logger->info("checkDependencies: Successfully set hash through environment variable 'DEBUG_SET_LICENSE_MODULE_HASH' ({})", hash);
        }
        else
        {
            _logger->error("checkDependencies: Environment variable 'DEBUG_SET_LICENSE_MODULE_HASH' ({}) is too long!", hash);
        }
    }
    else
    {
        _logger->debug("checkDependencies: Successfully set hash through demoOnlySetLicenseHash()");
    }

    auto pLastHashValue = &expected_license_hashBuffer[sizeof(expected_license_hashBuffer) / sizeof(expected_license_hashBuffer[0]) - 1];
    while (*pLastHashValue == 0)
    {
        if (--pLastHashValue == expected_license_hashBuffer)
            break;
    }
    std::vector<uint8_t> vecExpected_license_hashBuffer(expected_license_hashBuffer, pLastHashValue + 1);

    auto retVal = CanTrustLicenseModule(licenseCheckerLibrary.location(), vecExpected_license_hashBuffer, errMsg);
    if (OPENDAQ_FAILED(retVal))
        return DAQ_MAKE_ERROR_INFO(retVal, "Failed to verify the digital signature of the license library");
#pragma endregion

    // Try create the license component (now that we are sure that the module is valid)
    {
        const auto fctName = "createLicenseChecker";
        if (!licenseCheckerLibrary.has(fctName))
        {
            licenseCheckerLibrary.unload();

            auto errMsgReturn = daq::String(fmt::format("License library '{0}' is not a valid module (details: '{1}' export is missing)!", licenseCheckerFullPath.string(), fctName));
            *errMsg = errMsgReturn.addRefAndReturn();
            return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_RESOLVEFAILED, errMsgReturn);
        }

        using CreateLicenseCheckerFunc = daq::ErrCode (*)(ILicenseChecker**);
        CreateLicenseCheckerFunc createLicenseChecker = licenseCheckerLibrary.get<daq::ErrCode(ILicenseChecker**)>(fctName);

        retVal = createLicenseChecker(&licenseCheckerPtr);
        if (OPENDAQ_FAILED(retVal))
        {
            licenseCheckerLibrary.unload();

            auto errMsgReturn = daq::String(fmt::format("License library '{0}' is not a valid module (details: Call to '{1}' failed)!", licenseCheckerFullPath.string(), fctName));
            *errMsg = errMsgReturn.addRefAndReturn();
            return DAQ_MAKE_ERROR_INFO(retVal, errMsgReturn);
        }
    }

    return retVal;
}

OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context)
{
    // !!!Refuse to create the module if the license is not valid!!!
    if (licenseCheckerPtr == nullptr)
    {
        _logger->error("Please call the 'checkDependencies' function first to verify that this module can actually be loaded and used!");
        return DAQ_MAKE_ERROR_INFO(OPENDAQ_ERR_CREATE_FAILED, "'checkDependencies' function has either not been called or has failed!");
    }

    const auto errorCode = daq::createObject<daq::IModule, ProtectedAnalyticsModule>(module, context);
    if (OPENDAQ_SUCCEEDED(errorCode))
    {
        const auto pProtectedAnalyticsModule = dynamic_cast<ProtectedAnalyticsModule*>(*module);
        // Please note that we pass in a non-reference counted pointer of the license checker because
        // the life time of the returned 'pProtectedAnalyticsModule' pointer MUST be shorter
        // than the unloading of the module. And during the unloading of this module we ensure
        // that the license checker is disposed (see: DllMain).
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
