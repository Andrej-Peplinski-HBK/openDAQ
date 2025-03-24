#include <protected_analytics_module/module_dll.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>

using namespace daq::modules::protected_analytics_module;

OPENDAQ_MODULE_API daq::ErrCode checkDependencies(daq::IString** errMsg)
{
    // OPENDAQ_ERR_RESOLVEFAILED
    return OPENDAQ_SUCCESS;
}

OPENDAQ_MODULE_API daq::ErrCode createModule(daq::IModule** module, daq::IContext* context)
{
    return daq::createObject<daq::IModule, ProtectedAnalyticsModule>(module, context);
}
OPENDAQ_MODULE_API daq::ErrCode createProtectedAnalyticsModule(daq::IModule** module, daq::IContext* context)
{
    return createModule(module, context);
}

#ifdef OPENDAQ_TRACK_SHARED_LIB_OBJECT_COUNT
    std::atomic<std::size_t> daq::daqSharedLibObjectCount(0);   //Required otherwise we get a linker error 👉 LNK2001: unresolved external symbol "struct std::atomic<unsigned __int64> daq::daqSharedLibObjectCount"
#endif

//  OPENDAQ_MODULE_API daq::ErrCode daqGetObjectCount(daq::SizeT* count)
// {
//      *count = daq::daqSharedLibObjectCount;
//      return OPENDAQ_SUCCESS;
//  }
