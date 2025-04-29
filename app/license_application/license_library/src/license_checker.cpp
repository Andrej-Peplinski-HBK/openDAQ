#include <license_library/license_checker.h>
#include <fstream>
#include <mutex>
#include <regex>
#include <spdlog/sinks/stdout_color_sinks.h>

BEGIN_NAMESPACE_LICENSE_LIBRARY

std::mutex _mutex;
ILicenseChecker* LicenseChecker::getInstance(std::atomic<int>* ptrModuleOverallObjectRefCounter)
{
    if (_instance == nullptr)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        // Double-check after lock...
        if (_instance == nullptr)
            _instance = new LicenseChecker(ptrModuleOverallObjectRefCounter);
    }

    return _instance;
}
LicenseChecker::LicenseChecker(std::atomic<int>* ptrModuleOverallObjectRefCounter)
    : _ptrModuleOverallObjectRefCounter(ptrModuleOverallObjectRefCounter)
{
    assert(ptrModuleOverallObjectRefCounter != nullptr);

    _logger = spdlog::stdout_color_mt("LicenseChecker");
    // By default the logger is set to info level. Adjust using: _logger->set_level(spdlog::level::debug);

    ReloadLicenseFile();
}
LicenseChecker::~LicenseChecker()
{
    _ptrModuleOverallObjectRefCounter = nullptr;
}

int LicenseChecker::addRef()
{
    assert(_ptrModuleOverallObjectRefCounter != nullptr);
    _ptrModuleOverallObjectRefCounter->fetch_add(1, std::memory_order_relaxed);

    return ImplementationOf<ILicenseChecker>::addRef();
}
int LicenseChecker::releaseRef()
{
    assert(_ptrModuleOverallObjectRefCounter != nullptr);
    _ptrModuleOverallObjectRefCounter->fetch_sub(1, std::memory_order_acq_rel);

    return ImplementationOf<ILicenseChecker>::releaseRef();
}

ErrCode LicenseChecker::getNoOfFeatureTokens(const IString* feature, SizeT* overallCount, SizeT* remainingCount)
{
    if (overallCount == nullptr)
    {
        _logger->error("getNoOfFeatureTokens: overallCount is null!");
        return OPENDAQ_ERR_ARGUMENT_NULL;
    }

    if (remainingCount == nullptr)
    {
        _logger->error("getNoOfFeatureTokens: remainingCount is null!");
        return OPENDAQ_ERR_ARGUMENT_NULL;
    }

    *overallCount = 0;
    *remainingCount = 0;

    if (feature == nullptr)
    {
        _logger->error("getNoOfFeatureTokens: feature is null!");
        return OPENDAQ_ERR_ARGUMENT_NULL;
    }

    const std::string featureName = daq::StringPtr::Borrow(feature);
    const auto itOverall = _featureTokensOverall.find(featureName);
    if (itOverall != _featureTokensOverall.cend())
    {
        *overallCount = itOverall->second;

        const auto itCheckedOut = _featureTokensCheckedOut.find(featureName);
        if (itCheckedOut != _featureTokensCheckedOut.cend())
        {
            *remainingCount = (itOverall->second >= itCheckedOut->second ? itOverall->second - itCheckedOut->second : 0);
        }
        else
        {
            *remainingCount = itOverall->second;
        }
    }
    else
    {
        _logger->info("getNoOfFeatureTokens: feature not found in license file: {}", featureName);
    }

    return OPENDAQ_SUCCESS;
}

ErrCode LicenseChecker::checkOut(IString* feature, SizeT count)
{
    if (feature == nullptr)
    {
        _logger->error("checkOut: Arg 'feature' is null!");
        return OPENDAQ_ERR_ARGUMENT_NULL;
    }

    if (count == 0)
    {
        _logger->error("checkOut: Arg 'count' must be > 0!");
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }

    const std::string featureName = daq::StringPtr::Borrow(feature);
    const auto itOverall = _featureTokensOverall.find(featureName);
    if (itOverall != _featureTokensOverall.cend())
    {
        const auto& overallCount = itOverall->second;
        if (count > overallCount)
        {
            _logger->error("checkOut: feature '{}' has not enough tokens (requested: {}, max: {})", featureName, count, overallCount);
            return OPENDAQ_ERR_INVALID_ARGUMENT;
        }

        const auto itCheckedOut = _featureTokensCheckedOut.find(featureName);
        if (itCheckedOut != _featureTokensCheckedOut.cend())
        {
            const auto& checkedOutCount = itCheckedOut->second;
            if (count > overallCount - checkedOutCount)
            {
                _logger->error("checkOut: feature '{}' has not enough tokens (requested: {}, max: {})",
                               featureName,
                               count,
                               overallCount - checkedOutCount);
                return OPENDAQ_ERR_INVALID_ARGUMENT;
            }

            _featureTokensCheckedOut[featureName] = checkedOutCount + count;
        }
        else
        {
            // First time checkout...
            _featureTokensCheckedOut[featureName] = count;
        }

        return OPENDAQ_SUCCESS;
    }
    else
    {
        _logger->warn("checkOut: feature '{}' is not available.", featureName);
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }
}

ErrCode LicenseChecker::checkIn(IString* feature, SizeT count)
{
    if (feature == nullptr)
    {
        _logger->error("checkIn: Arg 'feature' is null!");
        return OPENDAQ_ERR_ARGUMENT_NULL;
    }

    if (count == 0)
    {
        _logger->error("checkIn: Arg 'count' must be > 0!");
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }

    const std::string featureName = daq::StringPtr::Borrow(feature);
    const auto itCheckedOut = _featureTokensCheckedOut.find(featureName);
    if (itCheckedOut != _featureTokensCheckedOut.cend())
    {
        const auto& checkedOutCount = itCheckedOut->second;
        if (count > checkedOutCount)
        {
            _logger->error("checkIn: Cannot check in more {} tokens of feature '{}' than what has been checked out ({})!",
                           count,
                           featureName,
                           checkedOutCount);
            return OPENDAQ_ERR_INVALID_ARGUMENT;
        }

        if (count == checkedOutCount)
            _featureTokensCheckedOut.erase(itCheckedOut);
        else
            _featureTokensCheckedOut[featureName] = checkedOutCount - count;

        return OPENDAQ_SUCCESS;
    }
    else
    {
        _logger->error("checkIn: Feature '{}' not been checked out yet!",featureName);
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }
}

const std::string getLicenseFilePath()
{
    // Check for environment variable override
    const char* envPath = std::getenv("OPENDAQ_LICENSE_PATH");

    // Use environment variable if set, otherwise use compile-time default
    if (envPath != nullptr && std::strlen(envPath) > 0)
        return envPath;

#ifdef WIN32
    return "C:/temp/license.lic";
#else
    return "~/license.lic";
#endif

    // Alternatively, consider using relative path to executable...
    // boost::filesystem::path execPath = boost::dll::program_location().parent_path();
    // return execPath.string() + "/license.lic";
}

void LicenseChecker::ReloadLicenseFile()
{
    const auto licFilePath = getLicenseFilePath();

    std::ifstream file(licFilePath);
    if (!file.is_open())
    {
        if (!file)
        {
            _logger->error("License file does not exist: {}", licFilePath);

            _featureTokensOverall.clear();
        }
        else
        {
            _logger->error("Failed to open license file: {}", licFilePath);
        }
        return;
    }

    _featureTokensOverall.clear();

    const std::regex lineRegex(R"(^\s*(\w+)\s*[:-]?\s*(\d+)\s*$)");
    std::string line;

    // Read each line
    while (std::getline(file, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, lineRegex))
        {
            // Extract feature name and count
            const auto featureName = match[1].str();
            const auto count = static_cast<SizeT>(std::stoull(match[2].str()));

            // Populate _featureTokensOverall
            _featureTokensOverall[featureName] = count;
        }
        else
        {
            // Log or handle invalid lines
            if (_logger)
            {
                _logger->warn("Invalid line in license file: {}", line);
            }
        }
    }

    if (_featureTokensCheckedOut.size() > 0)
    {
        for (const auto& [feature, overallCount] : _featureTokensOverall)
        {
            auto it = _featureTokensCheckedOut.find(feature);
            if (it != _featureTokensCheckedOut.cend())
            {
                // Check if the feature is checked out
                const auto& checkedOutCount = it->second;
                if (checkedOutCount > overallCount)
                {
                    _logger->warn("Feature {} checked out count ({}) exceeds overall count ({})!", feature, checkedOutCount, overallCount);
                }
            }
        }
    }
}

END_NAMESPACE_LICENSE_LIBRARY
