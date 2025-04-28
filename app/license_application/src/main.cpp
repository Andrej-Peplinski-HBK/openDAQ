#include <iostream>

#include <boost/dll/shared_library.hpp>
#include <boost/filesystem.hpp>

#include <opendaq/opendaq.h>

using namespace daq;

void printHelp()
{
    std::cout << "Usage: " << std::endl;
    std::cout << "  -hash <hash>" << std::endl << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  -hash E01D211BD6CF2F2C4BA8DCDABBA929D0FD8D5457 " << std::endl << std::endl;
    std::cout << "Please note that: " << std::endl;
    std::cout << "  a.) On Windows you can determine the hash by taking the first hit of:" << std::endl;
    std::cout << "    >> signtool verify /pa /v ${license_library} | findstr \"SHA1\"" << std::endl;
    std::cout << "  b.) When running in the Visual Studio debugger you can specify the command line the project debug settings... "
              << std::endl;
}

int main(int argc, const char* argv[])
{
#pragma region Prime the test application by injecting the expected license hash into protected_analytics_module
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

    std::error_code libraryErrCode;
    boost::filesystem::path exePath = boost::filesystem::absolute(argv[0]).parent_path();
#if _DEBUG
    boost::filesystem::path dllPath = exePath / "ProtectedAnalyticsModule-64-3-debug.module.dll";
#else
    boost::filesystem::path dllPath = exePath / "ProtectedAnalyticsModule-64-3.module.dll";
#endif
    boost::dll::shared_library moduleLibrary(dllPath.c_str(), libraryErrCode);

    if (libraryErrCode)
    {
        std::cerr << "Module \"" << dllPath << "\" failed to load. Error: " << libraryErrCode.value() << std::endl;
        return 1;
    }

    {
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
    }
#pragma endregion

    // Create an instance pointer that does NOT load any modules (see: https://opendaq.github.io/opendaq/dev/knowledge_base/modules.html)
    const InstancePtr instance = Instance("");  //"[[none]]"

    // Verify that after start up our modules are loaded
    // !!! Please note that on a PC you might not want to load any module due to security constraints. It might be better to verify the
    // digital signature of known modules before hand...!!!

    ModulePtr protectedAnalyticsModulePtr;
    const auto modules = instance.getModuleManager().getModules();
    for (const ModulePtr& module : modules)
    {
        const auto moduleName = module.getModuleInfo().getName();
        if (moduleName == "ProtectedAnalyticsModule")
        {
            protectedAnalyticsModulePtr = module;
            break;
        }
    }

    if (!protectedAnalyticsModulePtr.assigned())
    {
        std::cerr << "The 'ProtectedAnalyticsModule' was not found!" << std::endl << "Press any key to continue...";
        std::cin.get();
        return 1;
    }

    return 0;
}
