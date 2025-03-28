#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <protected_analytics_module/version.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE

ProtectedAnalyticsModule::ProtectedAnalyticsModule(ContextPtr ctx)
    : Module("ProtectedAnalyticsModule",
            daq::VersionInfo(PROTECTEDANALYTICSMODULE_MAJOR_VERSION, PROTECTEDANALYTICSMODULE_MINOR_VERSION, PROTECTEDANALYTICSMODULE_PATCH_VERSION),
            std::move(ctx),
            "ProtectedAnalyticsModule")
{
}

void ProtectedAnalyticsModule::setLicenseComponent(const void* licenseComponent)
{
    // Method implementation
}

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
