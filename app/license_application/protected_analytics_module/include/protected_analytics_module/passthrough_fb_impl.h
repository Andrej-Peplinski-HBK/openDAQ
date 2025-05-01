#pragma once

#include <protected_analytics_module/common.h>
#include <spdlog/spdlog.h>
#include <license_library/license_checker.h>
#include <opendaq/function_block_impl.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/data_packet_ptr.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE
namespace function_block
{

class PassthroughFbImpl final : public FunctionBlock
{
public:
    explicit PassthroughFbImpl(const ContextPtr& ctx,
                               const ComponentPtr& parent,
                               const StringPtr& localId,
                               daq::modules::license_library::ILicenseChecker* licenseComponent);
    ~PassthroughFbImpl() override;

    static constexpr const char* TypeID = "ProtectedAnalyticsModulePassthrough";
    static FunctionBlockTypePtr CreateType();

private:
    void createInputPorts();
    void createSignals();

    void onConnected(const InputPortPtr& port) override;
    void onDisconnected(const InputPortPtr& port) override;
    void onPacketReceived(const InputPortPtr& port) override;
    void processEventPacket(const EventPacketPtr& packet);
    void processDataPacket(DataPacketPtr&& packet, ListPtr<IPacket>& outQueue, ListPtr<IPacket>& outDomainQueue);

private:
    std::shared_ptr<spdlog::logger> _logger;
    daq::modules::license_library::ILicenseChecker* _licenseComponent;
    bool _isLicenseCheckedOut;
    InputPortPtr _inputPort;
    DataDescriptorPtr _inputDataDescriptor;
    DataDescriptorPtr _inputDomainDataDescriptor;
    SignalConfigPtr _outputSignal;
    SignalConfigPtr _outputDomainSignal;
};

}
END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
