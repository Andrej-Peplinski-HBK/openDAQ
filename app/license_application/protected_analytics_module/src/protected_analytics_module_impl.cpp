#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE

ProtectedAnalyticsModule::ProtectedAnalyticsModule(ContextPtr ctx)
    : Module("ProtectedAnalyticsModule",
            daq::VersionInfo(3u, 11u, 0u),//daq::VersionInfo(REF_FB_MODULE_MAJOR_VERSION, REF_FB_MODULE_MINOR_VERSION, REF_FB_MODULE_PATCH_VERSION),
            std::move(ctx),
            "ProtectedAnalyticsModule")
{
}

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
