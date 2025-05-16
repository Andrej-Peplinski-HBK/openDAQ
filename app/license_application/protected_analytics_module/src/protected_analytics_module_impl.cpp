#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>
#include <opendaq/exceptions.h>
#include <protected_analytics_module/passthrough_fb_impl.h>
#include <protected_analytics_module/protected_analytics_module_impl.h>
#include <protected_analytics_module/version.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE

ProtectedAnalyticsModule::ProtectedAnalyticsModule(ContextPtr ctx)
    : Module("ProtectedAnalyticsModule",
             daq::VersionInfo(
                 PROTECTEDANALYTICSMODULE_MAJOR_VERSION, PROTECTEDANALYTICSMODULE_MINOR_VERSION, PROTECTEDANALYTICSMODULE_PATCH_VERSION),
             std::move(ctx),
             "ProtectedAnalyticsModule")
    , _licenseComponent(nullptr)
{
}

void ProtectedAnalyticsModule::setLicenseComponent(daq::modules::license_library::ILicenseChecker* const licenseComponent)
{
    assert(licenseComponent != nullptr);

    _licenseComponent = licenseComponent;
}

DictPtr<IString, IFunctionBlockType> ProtectedAnalyticsModule::onGetAvailableFunctionBlockTypes()
{
    auto passThroughType = function_block::PassthroughFbImpl::CreateType();
    return Dict<IString, IFunctionBlockType>({{passThroughType.getId(), passThroughType}});
}

FunctionBlockPtr ProtectedAnalyticsModule::onCreateFunctionBlock(const StringPtr& id,
                                                                 const ComponentPtr& parent,
                                                                 const StringPtr& localId,
                                                                 const PropertyObjectPtr& config)
{
    if (_licenseComponent == nullptr)
    {
        loggerComponent->logMessage(SourceLocation{__FILE__, __LINE__, OPENDAQ_CURRENT_FUNCTION},
                                    "License component has not been set! Cannot create function block.",
                                    LogLevel::Critical);
        DAQ_THROW_EXCEPTION(NotAssignedException, "License component has not been set!");
    }

    if (id == function_block::PassthroughFbImpl::TypeID)
        return createWithImplementation<IFunctionBlock, function_block::PassthroughFbImpl>(context, parent, localId, _licenseComponent);

    loggerComponent->logMessage(SourceLocation{__FILE__, __LINE__, OPENDAQ_CURRENT_FUNCTION},
                                      fmt::format("Function block \"{}\" has not been implemented in this module!", id).c_str(),
                                      LogLevel::Warn);
    DAQ_THROW_EXCEPTION(InvalidParameterException, "Function block of type \"{}\" has not been implemented in this module!", id);
}

END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
