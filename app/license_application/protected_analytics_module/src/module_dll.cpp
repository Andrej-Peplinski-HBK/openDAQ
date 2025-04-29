#include <iostream>
#include <mutex>

#include <coretypes/filesystem.h>
#include <coretypes/stringobject_factory.h>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>

#include <license_library/license_checker.h>
#include <protected_analytics_module/module_dll.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <protected_analytics_module/license_module_verification.h>

using namespace daq::modules::protected_analytics_module;
using namespace daq::modules::license_library;

std::mutex mtx;
uint8_t expected_license_hashBuffer[32] = {0};

#ifdef __linux__
//See:
// * https://www.opengate.at/blog/2020/03/dllmain/
// * https://codeberg.org/GateNetwork/gate-blog-classroom/src/branch/main/c_cpp/dllmain_linux/
void __attribute__((constructor)) SO_init()
{
  /* do some global initialization */
  printf("ProtectedAnalytics: SO_init\n");
}

void __attribute__((destructor)) SO_uninit()
{
  /* do some global cleanup */
  printf("ProtectedAnalytics: SO_uninit\n");
}
#endif

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

    const auto programLocation = boost::dll::program_location();
    const auto exeParentDir = boost::filesystem::absolute(programLocation.c_str()).parent_path();    
    const auto fullLicPath = exeParentDir / "LicenseLibrary-64-3-signed.dll";   //Please note that under linux the module is called "libLicenseLibrary-64-3-debug.so" - which means that we subsequently fail to load this license library...

    std::error_code errorCode;
    boost::dll::shared_library moduleLibrary(fullLicPath.c_str(), errorCode);
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
    {
        const auto fctName = "createLicenseChecker";
        if (!moduleLibrary.has(fctName))
        {
            *errMsg = daq::String(fmt::format("License '{0}' is not a valid module (details: '{1}' export is missing)!",
                                              fullLicPath.string(),
                                              fctName))
                          .addRefAndReturn();
            return OPENDAQ_ERR_RESOLVEFAILED;
        }

        using CreateLicenseCheckerFunc = daq::ErrCode (*)(ILicenseChecker**);
        CreateLicenseCheckerFunc createLicenseChecker = moduleLibrary.get<daq::ErrCode(ILicenseChecker**)>(fctName);

        daq::ObjectPtr<ILicenseChecker> licenseCheckerPtr;
        const auto errCode = createLicenseChecker(&licenseCheckerPtr);
        if (OPENDAQ_SUCCEEDED(errCode))
        {
            auto feature = daq::String("fft");
            daq::SizeT overallCountInitial = 0;
            daq::SizeT remainingCountInitial = 0;

            if (OPENDAQ_SUCCEEDED(licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCountInitial, &remainingCountInitial)) &&
                remainingCountInitial > 0)
            {
                licenseCheckerPtr->checkOut(feature, remainingCountInitial);

                daq::SizeT overallCount2 = 0;
                daq::SizeT remainingCount2 = 0;
                licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCount2, &remainingCount2);

                assert(overallCountInitial == overallCount2);
                assert(remainingCount2 == 0);

                licenseCheckerPtr->checkIn(feature, remainingCountInitial);

                daq::SizeT overallCount3 = 0;
                daq::SizeT remainingCount3 = 0;
                licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCount3, &remainingCount3);

                assert(overallCountInitial == overallCount3);
                assert(overallCountInitial == remainingCount3);
            }
        }
    }
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
std::atomic<std::size_t> daq::daqSharedLibObjectCount(0);  // Required otherwise we get a linker error 👉 LNK2001: unresolved external symbol "struct std::atomic<unsigned __int64> daq::daqSharedLibObjectCount"

//  OPENDAQ_MODULE_API daq::ErrCode daqGetObjectCount(daq::SizeT* count)
// {
//      *count = daq::daqSharedLibObjectCount;
//      return OPENDAQ_SUCCESS;
//  }
#endif
