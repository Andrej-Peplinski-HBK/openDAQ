#include <protected_analytics_module/common.h>
#include <protected_analytics_module/passthrough_fb_impl.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <opendaq/packet_factory.h>
#include <opendaq/event_packet_ids.h>
#include <opendaq/event_packet_params.h>
#include <opendaq/reusable_data_packet_ptr.h>

BEGIN_NAMESPACE_PROTECTED_ANALYTICS_MODULE
namespace function_block {

daq::StringPtr strRequiredLicense("passthrough");

PassthroughFbImpl::PassthroughFbImpl(const ContextPtr& ctx,
                                     const ComponentPtr& parent,
                                     const StringPtr& localId,
                                     daq::modules::license_library::ILicenseChecker* licenseComponent)
    : FunctionBlock(CreateType(), ctx, parent, localId)
    , _licenseComponent(licenseComponent)
    , _isLicenseCheckedOut(false)
{
    assert(_licenseComponent != nullptr);

    _logger = spdlog::stdout_color_mt("PassthroughFbImpl");

    initComponentStatus();    
    createInputPorts();
    createSignals();
}
PassthroughFbImpl::~PassthroughFbImpl()
{
    if (_isLicenseCheckedOut)
    {
        assert(_licenseComponent != nullptr);

        if (OPENDAQ_FAILED(_licenseComponent->checkIn(strRequiredLicense, 1)))
            _logger->error("The previously checked out license could not be checked in again!");
    }
}

FunctionBlockTypePtr PassthroughFbImpl::CreateType()
{
    return FunctionBlockType(TypeID, "Passthrough", "Passes the input signal data through if the 'passthrough' license is available.");
}
void PassthroughFbImpl::createInputPorts()
{
    _inputPort = createAndAddInputPort("Input", PacketReadyNotification::SchedulerQueueWasEmpty);
}
void PassthroughFbImpl::createSignals()
{
    _outputSignal = createAndAddSignal("output");
    _outputDomainSignal = createAndAddSignal("output_domain", nullptr, false);

    _outputSignal.setDomainSignal(_outputDomainSignal);
}

void PassthroughFbImpl::onConnected(const InputPortPtr& port)
{
    if (!_isLicenseCheckedOut)
    {
        if (OPENDAQ_SUCCEEDED(_licenseComponent->checkOut(strRequiredLicense, 1)))
        {
            _isLicenseCheckedOut = true;
            setComponentStatus(ComponentStatus::Ok);
            _logger->info("onConnected: Successfully checked out '{}' license.", strRequiredLicense);
        }
        else
        {
            _logger->warn("onConnected: Failed to check out required '{}' license!", strRequiredLicense);
            setComponentStatusWithMessage(ComponentStatus::Warning, "Required license is missing!");
        }
    }
}
void PassthroughFbImpl::onDisconnected(const InputPortPtr& port)
{
    if (_isLicenseCheckedOut)
    {
        if (OPENDAQ_SUCCEEDED(_licenseComponent->checkIn(strRequiredLicense, 1)))
        {
            _isLicenseCheckedOut = false;
            _logger->info("onDisconnected: Successfully check in '{}' license.", strRequiredLicense);
        }
        else
        {
            _logger->error("onDisconnected: Failed to check in '{}' license.", strRequiredLicense);
        }
    }
}

void PassthroughFbImpl::onPacketReceived(const InputPortPtr& port)
{
    const auto connection = port.getConnection();
    if (!connection.assigned())
        return;

    auto outQueue = List<IPacket>();
    auto outDomainQueue = List<IPacket>();

    auto lock = this->getAcquisitionLock();

    auto packet = connection.dequeue();
    while (packet.assigned())
    {
        switch (packet.getType())
        {
            case PacketType::Event:
                processEventPacket(packet);
                break;
            case PacketType::Data:
                processDataPacket(std::move(packet), outQueue, outDomainQueue);
                break;
        }

        packet = connection.dequeue();
    }

    if (outQueue.getCount() > 0)
    {
        _outputSignal.sendPackets(std::move(outQueue));
        _outputDomainSignal.sendPackets(std::move(outDomainQueue));  // You can safely ignore the incorrect C26110 warning - Caller failing to hold lock 'lock' before calling function 'func'. Please also note that a "#pragma warning ignore" does not help to remove the squigglies in Visual Studio.
    }
}
void PassthroughFbImpl::processEventPacket(const EventPacketPtr& packet)
{
    if (packet.getEventId() == event_packet_id::DATA_DESCRIPTOR_CHANGED)
    {
        if (!_isLicenseCheckedOut)
        {
            _logger->warn("Required license could not be checked out!");            
            _outputSignal.setDescriptor(nullptr);
            return;
        }

        const DataDescriptorPtr inputDataDescriptor = packet.getParameters().get(event_packet_param::DATA_DESCRIPTOR);
        const DataDescriptorPtr inputDomainDataDescriptor = packet.getParameters().get(event_packet_param::DOMAIN_DATA_DESCRIPTOR);

        const auto nullDataDescriptor = NullDataDescriptor();

        if (!inputDataDescriptor.assigned() || inputDataDescriptor == nullDataDescriptor)
        {
            _logger->error("processEventPacket: Input data descriptor is null!");
            setComponentStatusWithMessage(ComponentStatus::Error, "Failed to set descriptor for output signal: Input data descriptor is null!");
            _outputSignal.setDescriptor(nullptr);
            return;
        }

        if (!inputDomainDataDescriptor.assigned() || inputDomainDataDescriptor == nullDataDescriptor)
        {
            _logger->error("processEventPacket: Input domain data descriptor is null!");
            setComponentStatusWithMessage(ComponentStatus::Error, "Failed to set descriptor for output signal: Input domain data descriptor is null!");
            _outputSignal.setDescriptor(nullptr);
            return;
        }

        const auto inputSampleType = inputDataDescriptor.getSampleType();
        const auto inputRange = inputDataDescriptor.getValueRange();
        const auto inputName = inputDataDescriptor.getName();
        const auto inputUnit = inputDataDescriptor.getUnit();

        auto outputDataDescriptorBuilder = DataDescriptorBuilder()
            .setSampleType(inputSampleType)
            .setValueRange(inputRange)
            .setName(inputName.toStdString() + "/passthrough")
            .setUnit(inputUnit);

        const auto outputDataDescriptor = outputDataDescriptorBuilder.build();

        _outputSignal.setDescriptor(outputDataDescriptor);
        _outputDomainSignal.setDescriptor(inputDomainDataDescriptor);

        setComponentStatus(ComponentStatus::Ok);
    }
}
void PassthroughFbImpl::processDataPacket(DataPacketPtr&& packet, ListPtr<IPacket>& outQueue, ListPtr<IPacket>& outDomainQueue)
{
    if (!_isLicenseCheckedOut)
        return;

    DataPacketPtr outputPacket;
    DataPacketPtr outputDomainPacket = packet.getDomainPacket();

    const auto outputDataDescriptor = _outputSignal.getDescriptor();
    const auto reusablePacket = packet.asPtrOrNull<IReusableDataPacket>(true);
    if (reusablePacket.assigned()
     && packet.getRefCount() == 1
     && reusablePacket.reuse(outputDataDescriptor, std::numeric_limits<SizeT>::max(), nullptr, nullptr, false))
    {
        outputPacket = std::move(packet);
    }
    else
    {
        const auto sampleCount = packet.getSampleCount();

        outputPacket = DataPacketWithDomain(outputDomainPacket, outputDataDescriptor, sampleCount);

        auto ptrDestData = outputPacket.getData();
        const auto ptrSourceData = packet.getData();
        const auto byteSize = packet.getDataSize();

        std::memcpy(ptrDestData, ptrSourceData, byteSize);
    }

    outQueue.pushBack(std::move(outputPacket));
    outDomainQueue.pushBack(std::move(outputDomainPacket));
}

}
END_NAMESPACE_PROTECTED_ANALYTICS_MODULE
