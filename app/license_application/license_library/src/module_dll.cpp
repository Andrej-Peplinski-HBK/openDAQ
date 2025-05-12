#include <license_library/module_dll.h>

// Reference counter for all objects that reside inside this module and that is used to
// verify that no clients hold dangling pointers when the module unloads (remark: This is a similar approach to the one being used in ATL/COM - see: http://diranieh.com/ATLCOM/Architecture.htm)
std::atomic<int> moduleOverallObjectRefCounter;

#ifdef WIN32

#include <wtypes.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstance*/, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            assert(moduleOverallObjectRefCounter == 0); //"When invoking the initial hook into the loading of the module no objects of the module could have been instantiated!"
            break;
        case DLL_PROCESS_DETACH:
            assert(moduleOverallObjectRefCounter == 0); //"Detected unloading of module with outstanding external interface references that should have been released!!!"
            break;
    }
    return TRUE;
}
#else

void __attribute__((constructor)) SO_init()
{
  /* do some global initialization */
  //printf("LicenseLibrary: SO_init\n");
}

void __attribute__((destructor)) SO_uninit()
{
  /* do some global cleanup */
  //printf("LicenseLibrary: SO_uninit\n");
}

#endif

using namespace daq::modules::license_library;
OPENDAQ_MODULE_API daq::ErrCode createLicenseChecker(ILicenseChecker** licenseChecker)
{
    if (!licenseChecker)
        return OPENDAQ_ERR_INVALIDPARAMETER;

    const auto ptrRetVal = LicenseChecker::getInstance(&moduleOverallObjectRefCounter);
    ptrRetVal->addRef();

    *licenseChecker = ptrRetVal;
    return OPENDAQ_SUCCESS;
}


#ifdef OPENDAQ_TRACK_SHARED_LIB_OBJECT_COUNT
std::atomic<std::size_t> daq::daqSharedLibObjectCount(0);  // Required otherwise we get a linker error ?? LNK2001: unresolved symbol "struct std::atomic<unsigned __int64> daq::daqSharedLibObjectCount"
#endif
