#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <protected_analytics_module/version.h>
#include <spdlog/sinks/stdout_color_sinks.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE


ProtectedAnalyticsModule::ProtectedAnalyticsModule(ContextPtr ctx)
    : Module(
          "ProtectedAnalyticsModule",
            daq::VersionInfo(PROTECTEDANALYTICSMODULE_MAJOR_VERSION, PROTECTEDANALYTICSMODULE_MINOR_VERSION, PROTECTEDANALYTICSMODULE_PATCH_VERSION),
            std::move(ctx),
            "ProtectedAnalyticsModule"),
    _licenseComponent(nullptr)
{
    _logger = spdlog::stdout_color_mt("ProtectedAnalyticsModule");
}

void ProtectedAnalyticsModule::setLicenseComponent(daq::modules::license_library::ILicenseChecker* const licenseComponent)
{
    assert(licenseComponent != nullptr);

    _licenseComponent = licenseComponent;
}

//auto feature = daq::String("fft");
//daq::SizeT overallCountInitial = 0;
//daq::SizeT remainingCountInitial = 0;
//
//if (OPENDAQ_SUCCEEDED(licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCountInitial, &remainingCountInitial)) &&
//    remainingCountInitial > 0)
//{
//    licenseCheckerPtr->checkOut(feature, remainingCountInitial);
//
//    daq::SizeT overallCount2 = 0;
//    daq::SizeT remainingCount2 = 0;
//    licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCount2, &remainingCount2);
//
//    assert(overallCountInitial == overallCount2);
//    assert(remainingCount2 == 0);
//
//    licenseCheckerPtr->checkIn(feature, remainingCountInitial);
//
//    daq::SizeT overallCount3 = 0;
//    daq::SizeT remainingCount3 = 0;
//    licenseCheckerPtr->getNoOfFeatureTokens(feature, &overallCount3, &remainingCount3);
//
//    assert(overallCountInitial == overallCount3);
//    assert(overallCountInitial == remainingCount3);
//}

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
