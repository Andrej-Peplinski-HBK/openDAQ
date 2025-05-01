#pragma once

#include <license_library/license_checker.h>
#include <protected_analytics_module/common.h>
#include <opendaq/module_impl.h>
#include <spdlog/spdlog.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE


class ProtectedAnalyticsModule final : public Module
{
public:
    explicit ProtectedAnalyticsModule(ContextPtr ctx);

    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onCreateFunctionBlock(const StringPtr& id, const ComponentPtr& parent, const StringPtr& localId, const PropertyObjectPtr& config) override;

    void setLicenseComponent(daq::modules::license_library::ILicenseChecker* const licenseComponent);

private:
    std::shared_ptr<spdlog::logger> _logger;
    daq::modules::license_library::ILicenseChecker* _licenseComponent;
};

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
