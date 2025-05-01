#include <iostream>
#include <algorithm> // For std::find

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/filesystem.hpp>

#include <opendaq/opendaq.h>

using namespace daq;

void printHelp()
{
    std::cout << "Usage: " << std::endl;
    std::cout << "  -hash <hash>" << std::endl << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  -hash E01D211BD6CF2F2C4BA8DCDABBA929D0FD8D5457" << std::endl << std::endl;
    std::cout << "Please note that: " << std::endl;
    std::cout << "  a.) On Windows you can determine the hash by taking the first hit of:" << std::endl;
    std::cout << "    >> signtool verify /pa /v ${license_library} | findstr \"SHA1\"" << std::endl;
    std::cout << "  b.) When running in the Visual Studio debugger you can specify the command line the project debug settings... "
              << std::endl;
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
                    std::cout << "Found matching module: " << filename << std::endl;
                    matchingFiles.push_back(entry.path());
                }
            }
        }
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
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

    // //Check if we are in a debug build (by looking for the debug folder in the path)
    // const auto itContainsDebug = std::find_if(exePath.begin(), exePath.end(), [](const auto& part) {
    //     return part.string().find("debug") != std::string::npos;
    // });

    // #if _WIN32
    // {
    //     return exePath / "ProtectedAnalyticsModule-64-3-debug.module.dll";
    // }
    // #elif defined(__linux__)
    // {
    //     //E.g.: "/home/apeplinski/dev/SourceCode/GitHub/openDAQ/build/x64/gcc/full/debug/bin/libProtectedAnalyticsModule-64-3-debug.module.so"
    //     boost::filesystem::path dllPath = exePath / "libProtectedAnalyticsModule-64-3-debug.module.so";
    //     return dllPath;
    // }
    // #else
    //     #pragma error "Unsupported platform!"
    // #endif
}
int main(int argc, const char* argv[])
{
#pragma region Prime the test application by injecting the expected license hash into protected_analytics_module
    boost::dll::shared_library moduleProtectedAnalyticsModule;
    {
        std::vector<uint8_t> expected_license_hashBuffer;
        for (auto i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "-hash")
            {
                if (i + 1 < argc)
                {
                    const std::string hash = argv[i + 1];
                    expected_license_hashBuffer = std::vector<uint8_t>(hash.size() / 2);
                    for (size_t j = 0; j < hash.size(); j += 2)
                    {
                        const auto hashValue = static_cast<uint8_t>(std::stoi(hash.substr(j, 2), nullptr, 16));
                        if (hashValue == 0)
                        {
                            std::cerr << "Unable to handle hashes that contain 0s!" << std::endl;
                            return 1;
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
            std::cerr << "Module \"" << dllPath << "\" failed to load. Error: " << libraryErrCode.value() << std::endl;
            return 1;
        }

        const auto fctName = "demoOnlySetLicenseHash";
        if (!moduleLibrary.has(fctName))
        {
            std::cerr << "The function \"" << fctName << "\" was not found in the module!" << std::endl;
            return 1;
        }

        using DemoOnlySetLicenseHashFunc = ErrCode (*)(uint32_t, uint8_t*);
        DemoOnlySetLicenseHashFunc demoOnlySetLicenseHash = moduleLibrary.get<ErrCode(uint32_t, uint8_t*)>(fctName);

        const ErrCode errCode = demoOnlySetLicenseHash(expected_license_hashBuffer.size(), &expected_license_hashBuffer[0]);
        if (OPENDAQ_FAILED(errCode))
        {
            std::cerr << "Failed to set the license hash in the module!" << std::endl;
            return 1;
        }

        moduleProtectedAnalyticsModule = std::move(moduleLibrary);  // Keep the module loaded until the end of the program
    }
#pragma endregion

    // Create an instance pointer that does NOT load any modules (see: https://opendaq.github.io/opendaq/dev/knowledge_base/modules.html)
    const InstancePtr instance = Instance("");  //"[[none]]"

    // Verify that after start up our modules are loaded
    // !!! Please note that on a PC you might not want to load any module due to security constraints. It might be better to verify the
    // digital signature of known modules before hand...!!!
    const auto modules = instance.getModuleManager().getModules();
    auto itFound = std::find_if(modules.begin(),
                           modules.end(),
                           [](const ModulePtr& module) { return module.getModuleInfo().getName() == "ProtectedAnalyticsModule"; });

    if (itFound == modules.end())
    {
        std::cerr << "The 'ProtectedAnalyticsModule' was not found!" << std::endl << "Press any key to continue...";
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
    auto voltageData = static_cast<float*>(packetRamp.getRawData());
    for (size_t i = 0; i < 100; i++)
        *voltageData++ = static_cast<float>(i);

    const auto reader = StreamReaderBuilder()
                            .setSkipEvents(True)
                            .setSignal(outputSignal)
                            .setValueReadType(SampleType::Float32)
                            .setDomainReadType(SampleType::Int64)
                            .setReadMode(ReadMode::Scaled)
                            .setReadTimeoutType(ReadTimeoutType::All)
                            .build();

    signalTime.sendPacket(packetTime);
    signalRamp.sendPacket(packetRamp);

    std::vector<float> data(noOfSamples);
    std::vector<int64_t> time(noOfSamples);

    SizeT noOfSamplesRead = noOfSamples;
    reader.readWithDomain(data.data(), time.data(), &noOfSamplesRead, std::numeric_limits<daq::SizeT>().max());
    //Check if we can read the data (otherwise, we might have had a license problem)...
    if (noOfSamplesRead == noOfSamples)
    {
        const auto cmp = std::memcmp(data.data(), packetRamp.getRawData(), noOfSamples * sizeof(float));
        std::cout << "Read " << noOfSamplesRead << " samples. Compare: " << cmp << std::endl;
    }
    else
    {
        const auto fbStatus = fb.getStatusContainer().getStatusMessage("ComponentStatus");
        std::cerr << "Failed to read the expected number of samples (status: " << fbStatus << ")" 
                  << std::endl;
        return -1;
    }

    return 0;
}
