#include <audio_application/utils.h>
#include <iostream>
#include <thread>
#include <opendaq/opendaq.h>

int main(int /*argc*/, const char* /*argv*/[])
{
    const auto instance = daq::Instance();
    const auto availableDevices = instance.getAvailableDevices();
    const auto availableAudioDevices = filterDevicesInfos(availableDevices, "miniaudio");
    if (availableAudioDevices.getCount() == 0)
    {
        std::cout << "No audio devices detected";
        return 1;
    }
    printDevices(std::cout, availableAudioDevices);
    std::cout << "Selected device: ";
    size_t deviceIndex;
    std::cin >> deviceIndex;

    const auto device = instance.addDevice(availableAudioDevices[deviceIndex].getConnectionString());
    //device.setPropertyValue("SampleRate", 44100);     <-- Andrej Peplinski: Commented out as it will cause an AccessException because the parameter cannot be written to...
    const auto rendererFb = instance.addFunctionBlock("ref_fb_module_renderer");
    const auto statisticsFb = instance.addFunctionBlock("ref_fb_module_statistics");
    statisticsFb.setPropertyValue("BlockSize", 1000);
    
    const auto deviceChannel = device.getChannels()[0];
    const auto deviceSignal = deviceChannel.getSignals()[0];

    const auto wavWriterFb = instance.addFunctionBlock("audio_device_module_wav_writer");
    wavWriterFb.getInputPorts()[0].connect(deviceSignal);
    wavWriterFb.setPropertyValue("FileName", "OpenDAQ-audio-app.wav");
    wavWriterFb.setPropertyValue("Storing", true);  // Start the recording of the wav file (can only be invoked after the 'connect' and 'setPropertyValue("FileName", ...)' call)

    statisticsFb.getInputPorts()[0].connect(deviceSignal);
    const auto rmsSignal = statisticsFb.getSignals()[1];

    rendererFb.getInputPorts()[0].connect(deviceSignal);
    rendererFb.getInputPorts()[1].connect(rmsSignal);

    const auto rmsReader = PacketReader(rmsSignal);

    hideCursor(std::cout);    
    for (size_t i = 0; i < 200; ++i)
    {
        printLastValueBar(std::cout, rmsReader);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
