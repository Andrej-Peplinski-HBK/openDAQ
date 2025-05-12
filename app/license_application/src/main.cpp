#include <iostream>
#include <algorithm> // For std::find
#include <thread>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/filesystem.hpp>

#include <opendaq/opendaq.h>

using namespace daq;

const std::string Reset = "\033[0m";
const std::string Red = "\033[31m";
const std::string Green = "\033[32m";
const std::string Yellow = "\033[33m";

void printHelp()
{
    std::cout << "Usage: " << std::endl;
    std::cout << "  In order to run this demo application you will need to specify the hash of signing certificate used to sign the 'LicenseLibrary'." << std::endl;
    std::cout << "  On x64 Windows you will find the hash in the build output as the signing will be done automatically for you." << std::endl;
    std::cout << "  You can specify the hash either using an ENV variable:" << std::endl;
    std::cout << "    E.g.: set DEBUG_SET_LICENSE_MODULE_HASH=3012B9EE811245DB18F81074E7805A9165D00265" << std::endl << std::endl;
    std::cout << "  Or you can use the command line argument:" << std::endl << std::endl;
    std::cout << "    E.g.: license_application.exe -hash 3012B9EE811245DB18F81074E7805A9165D00265" << std::endl << std::endl;
    std::cout << "Please also note that: " << std::endl;
    std::cout << "  a.) You can optionally override to OpenDAQ module directory (CWD) with an ENV variable:" << std::endl;
    std::cout << "    >> set OPENDAQ_MODULES_PATH=%Your_Path%" << std::endl;
    std::cout << "  b.) On Windows you can determine the hash manually by taking the first hit of:" << std::endl;
    std::cout << "    >> signtool verify /pa /v ${license_library} | findstr \"SHA1\"" << std::endl;    
}

boost::filesystem::path getProtectedAnalyticsModulePath()
{
    boost::filesystem::path exePath = boost::filesystem::absolute(boost::dll::program_location().c_str()).parent_path();

    // Search for files matching the pattern "ProtectedAnalyticsModule*module.*"
    std::vector<boost::filesystem::path> matchingFiles;

    try
    {
        for (const auto& entry : boost::filesystem::directory_iterator(exePath))
        {
            if (boost::filesystem::is_regular_file(entry))
            {
                std::string filename = entry.path().filename().string();
                if (filename.find("ProtectedAnalyticsModule") != std::string::npos && filename.find("module") != std::string::npos)
                {
                    std::cout << Green << "Found matching module: " << filename << Reset << std::endl;
                    matchingFiles.push_back(entry.path());
                }
            }
        }
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        std::cerr << Red << "Error scanning directory: " << e.what() << Reset << std::endl;
    }

    // Use the first matching file if found, otherwise fall back to the default path
    if (matchingFiles.size() == 1)
    {
        return matchingFiles.front();
    }

    // Throw a filesystem_error with an appropriate error code
    if (matchingFiles.empty())
    {
        throw boost::filesystem::filesystem_error("ProtectedAnalyticsModule not found",
                                                  exePath,
                                                  boost::system::errc::make_error_code(boost::system::errc::no_such_file_or_directory));
    }

    throw boost::filesystem::filesystem_error("Unable to uniquely identify the ProtectedAnalyticsModule!",
                                              exePath,
                                              boost::system::errc::make_error_code(boost::system::errc::invalid_seek));
}

int main(int argc, const char* argv[])
{
    boost::dll::shared_library moduleProtectedAnalyticsModule;  //Define top-level object to ensure that the license library does not get unloaded after potentially having it loaded to set the license hash externally.

    // Check for the user-supplied license hash (1. Environment variable)
    const auto envLicenseHash = std::getenv("DEBUG_SET_LICENSE_MODULE_HASH");
    std::string hash = envLicenseHash ? envLicenseHash : "";
    if (hash.empty())
    {
        //2. Check for command line argument
        std::vector<uint8_t> expected_license_hashBuffer;
        for (auto i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "-hash")
            {
                if (i + 1 < argc)
                {
                    hash = argv[i + 1];
                    expected_license_hashBuffer = std::vector<uint8_t>(hash.size() / 2);
                    for (size_t j = 0; j < hash.size(); j += 2)
                    {
                        const auto hashValue = static_cast<uint8_t>(std::stoi(hash.substr(j, 2), nullptr, 16));
                        if (hashValue == 0)
                        {
                            std::cerr << Red << "Unable to handle hashes that contain 0s!" << std::endl;
                            return -1;
                        }

                        expected_license_hashBuffer[j / 2] = hashValue;
                    }
                }
                break;
            }
        }

        if (expected_license_hashBuffer.size() == 0)
        {
            printHelp();
            return 1;
        }

        const auto dllPath = getProtectedAnalyticsModulePath();

        std::error_code libraryErrCode;
        boost::dll::shared_library moduleLibrary(dllPath.c_str(), libraryErrCode);
        if (libraryErrCode)
        {
            std::cerr << Red << "Module \"" << dllPath << "\" failed to load. Error: " << libraryErrCode.value() << std::endl;
            return 1;
        }

        const auto fctName = "demoOnlySetLicenseHash";
        if (!moduleLibrary.has(fctName))
        {
            std::cerr << Red << "The function \"" << fctName << "\" was not found in the module!" << std::endl;
            return 1;
        }

        using DemoOnlySetLicenseHashFunc = ErrCode (*)(uint32_t, uint8_t*);
        DemoOnlySetLicenseHashFunc demoOnlySetLicenseHash = moduleLibrary.get<ErrCode(uint32_t, uint8_t*)>(fctName);

        const ErrCode errCode = demoOnlySetLicenseHash(expected_license_hashBuffer.size(), &expected_license_hashBuffer[0]);
        if (OPENDAQ_FAILED(errCode))
        {
            std::cerr << Red << "Failed to set the license hash in the module!" << std::endl;
            return 1;
        }

        moduleProtectedAnalyticsModule = std::move(moduleLibrary);  // Keep the module loaded until the end of the program
    }

    // For different options on how to load modules see: https://opendaq.github.io/opendaq/dev/knowledge_base/modules.html
    const InstancePtr instance = Instance("");  //"[[none]]"

    // Verify that after start up our modules are loaded
    // !!! Please note that on a PC you might not want to load any kind of module due to security constraints. It might be better to verify the
    // digital signature of known modules and only load those!!!
    const auto modules = instance.getModuleManager().getModules();
    auto itFound = std::find_if(modules.begin(),
                           modules.end(),
                           [](const ModulePtr& module) { return module.getModuleInfo().getName() == "ProtectedAnalyticsModule"; });

    if (itFound == modules.end())
    {
        std::cerr << Red << "The 'ProtectedAnalyticsModule' was not found!" << Reset << std::endl << "Press any key to continue...";
        std::cin.get();
        return 1;
    }

    ModulePtr protectedAnalyticsModulePtr = *itFound;

    const auto fbTypeID = "ProtectedAnalyticsModulePassthrough";  // daq::modules::protected_analytics_module::function_block::PassthroughFbImpl::TypeID;
    const auto fb = protectedAnalyticsModulePtr.createFunctionBlock(fbTypeID, nullptr, "id");

    SignalConfigPtr signalTime;
    SignalConfigPtr signalRamp;
#pragma region Create signals
    {
        const auto ctx = instance.getContext();
        signalTime = Signal(ctx, nullptr, "time");
        signalRamp = Signal(ctx, nullptr, "ramp");

        signalRamp.setDomainSignal(signalTime);

        const auto timeDescriptor = DataDescriptorBuilder()
                                        .setSampleType(SampleType::Int64)
                                        .setTickResolution(Ratio(1, 1000))
                                        .setOrigin("1970-01-01T00:00:00")
                                        .setRule(LinearDataRule(1, 0))
                                        .setUnit(Unit("s", -1, "second", "time"))
                                        .build();
        signalTime.setDescriptor(timeDescriptor);

        const auto voltageDescriptor = DataDescriptorBuilder().setSampleType(SampleType::Float32).build();
        signalRamp.setDescriptor(voltageDescriptor);
    }
#pragma endregion

    const auto lstInputPorts = fb.getInputPorts();
    assert(lstInputPorts.getCount() == 1);

    lstInputPorts[0].connect(signalRamp);
    const auto outputSignal = fb.getSignals()[0];

    const uint64_t noOfSamples = 100;
    const auto packetTime = DataPacket(signalTime.getDescriptor(), noOfSamples, 0);

    const auto packetRamp = DataPacketWithDomain(packetTime, signalRamp.getDescriptor(), noOfSamples);
    auto pRampDataRaw = static_cast<float*>(packetRamp.getRawData());
    for (size_t i = 0; i < noOfSamples; i++)
        *pRampDataRaw++ = static_cast<float>(i);

    const auto reader = StreamReaderBuilder() //BlockReaderBuilder().setBlockSize(noOfSamples)
                            .setSignal(outputSignal)
                            .setValueReadType(SampleType::Float32)
                            .setDomainReadType(SampleType::Int64)
                            .setReadMode(ReadMode::Scaled)
                            .setReadTimeoutType(ReadTimeoutType::All)
                            .setSkipEvents(true)
                            .build();

    signalTime.sendPacket(packetTime);
    signalRamp.sendPacket(packetRamp);

    // Wait for the reader to process the packets
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<float> data(noOfSamples);
    std::vector<int64_t> time(noOfSamples);

    int retVal = -1;
    SizeT noOfSamplesRead = noOfSamples;
    const auto readerStatus = reader.readWithDomain(data.data(), time.data(), &noOfSamplesRead);
    //Check if we can read the data (otherwise, we might have had a license problem)...
    if (noOfSamplesRead == noOfSamples)
    {
        const auto cmp = std::memcmp(data.data(), packetRamp.getRawData(), noOfSamples * sizeof(float));
        if (cmp == 0)
        {
            retVal = 0;
            std::cout << Green << "Read all " << noOfSamplesRead << " samples and verified their correctness..."
                      << Reset << std::endl;
        }
        else
        {
            std::cerr << Red << "Read all " << noOfSamplesRead << " samples but detected unexpected values (compare: " << cmp << ")!"
                      << Reset << std::endl;
        }
    }
    else
    {
        std::string readerStatusStr;
        switch (readerStatus.getReadStatus())
        {
            case ReadStatus::Ok:
                readerStatusStr = "Ok";
                break;
            case ReadStatus::Fail:
                readerStatusStr = "Fail";
                break;
            case ReadStatus::Event:
                readerStatusStr = "Event";
                break;
            default:
                readerStatusStr = "Unknown";
                break;
        }
        
        auto fbStatus = fb.getStatusContainer().getStatusMessage("ComponentStatus");
        if (fbStatus.getLength() == 0)
            fbStatus = "<OK>";

        std::cerr << Red << "Failed to read the expected number of samples (reader status: "
            << readerStatusStr << ", status of pass-through function block: "
            << fbStatus << ")"
            << Reset << std::endl;
        return -1;
    }

    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();

    return 0;
}
