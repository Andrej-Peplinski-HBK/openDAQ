#pragma once

#include <protected_analytics_module/common.h>
#include <opendaq/module_impl.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE

class ProtectedAnalyticsModule final : public Module
{
public:
    explicit ProtectedAnalyticsModule(ContextPtr ctx);

    //DictPtr<IString, IFunctionBlockType> onGetAvailableFunctionBlockTypes() override;
    //FunctionBlockPtr onCreateFunctionBlock(const StringPtr& id, const ComponentPtr& parent, const StringPtr& localId, const PropertyObjectPtr& config) override;

    void setLicenseComponent(const void* licenseComponent);
private:
};

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
