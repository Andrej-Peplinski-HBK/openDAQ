#pragma once

#include <license_library/license_checker.h>
#include <protected_analytics_module/common.h>
#include <opendaq/module_impl.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE

/*!
 * @brief Entry class to the Protected Analytics Module, which gives access to the
 *        one and only @ref PassthroughFbImpl function block type.
 */
class ProtectedAnalyticsModule final : public Module
{
public:
    explicit ProtectedAnalyticsModule(ContextPtr ctx);

    DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    FunctionBlockPtr onCreateFunctionBlock(const StringPtr& id, const ComponentPtr& parent, const StringPtr& localId, const PropertyObjectPtr& config) override;

    void setLicenseComponent(daq::modules::license_library::ILicenseChecker* const licenseComponent);

private:
    daq::modules::license_library::ILicenseChecker* _licenseComponent;
};

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
