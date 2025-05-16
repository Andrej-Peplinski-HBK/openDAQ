#pragma once

#include <protected_analytics_module/common.h>
#include <license_library/license_checker.h>
#include <opendaq/function_block_impl.h>
#include <opendaq/function_block_ptr.h>
#include <opendaq/event_packet_ptr.h>
#include <opendaq/data_packet_ptr.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE
namespace function_block
{

/**
 * @class PassthroughFbImpl
 * @brief A final implementation of the FunctionBlock class that acts as a passthrough module
 *        with integrated license checking functionality.
 *
 * This class is responsible for managing input ports, signals, and processing packets
 * while ensuring that the required license is checked out. It is part of the 
 * Protected Analytics Module and provides functionality to handle event and data packets.
 *
 * @note This class cannot be inherited from as it is marked `final`.
 *
 * @details
 * - The class provides static methods for type creation and identification.
 * - It overrides specific methods from the base FunctionBlock class to handle
 *   connection, disconnection, and packet reception events.
 * - It integrates with the license library to ensure proper license checking.
 */
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
    daq::modules::license_library::ILicenseChecker* _licenseComponent;
    bool _isLicenseCheckedOut;
    InputPortPtr _inputPort;
    SignalConfigPtr _outputSignal;
    SignalConfigPtr _outputDomainSignal;
};
/*!@}*/

}
END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
