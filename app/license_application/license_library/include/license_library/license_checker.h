#pragma once

#include <license_library/common.h>
#include <opendaq/component.h>
#include <spdlog/spdlog.h>
#include <map>

BEGIN_NAMESPACE_LICENSE_LIBRARY

/*!
 * @brief A demo-only definition of a license checker interface that can be used
 * to check which licenses are available and how many tokens are available for each license.
 * And, furthermore, allows to check out and check in tokens for a specific license.
 */
DECLARE_OPENDAQ_INTERFACE(ILicenseChecker, IBaseObject)
{
    /// @brief Returns the overall number of tokens available amd the remaining count for a specific license feature.
    /// @param feature The feature name to check.
    /// @param overallCount The number of tokens available for the feature.
    /// @param remainingCount The number of tokens remaining for the feature (i.e. overallCount - checkedOutCount).
    /// @return ErrCode Returns an error code indicating the success or failure of the operation.
    virtual ErrCode INTERFACE_FUNC getNoOfFeatureTokens(const IString* feature, SizeT* overallCount, SizeT* remainingCount) = 0;

    /// @brief Tries to check out a number of tokens for a specific license feature, so that the feature can be used.
    /// @param feature The feature name to check out.
    /// @param count The number of tokens to check out.
    /// @return ErrCode Returns an error code indicating the success or failure of the operation.
    virtual ErrCode INTERFACE_FUNC checkOut(IString * feature, SizeT count) = 0;


    /**
     * @brief Checks in a specified feature with a given count.
     * 
     * This function is used to release or return a previously checked-out feature
     * back to the license system. It ensures that the feature is no longer in use
     * and updates the license count accordingly.
     * 
     * @param feature The feature name to check in.
     * @param count The number of licenses to check in (so that they become available again).
     * @return ErrCode Returns an error code indicating the success or failure of the operation.
     */
    virtual ErrCode INTERFACE_FUNC checkIn(IString * feature, SizeT count) = 0;
};
/*!@}*/


/**
 * @class LicenseChecker
 * @brief Implementation of the ILicenseChecker interface for managing license features and tokens.
 * 
 * This class provides functionality to manage license features, including checking out and checking in
 * feature tokens, as well as tracking the overall and remaining counts of tokens. It also includes 
 * reference counting methods to assist in troubleshooting module unload problems.
 * 
 * @remark: This class is a singleton and should be accessed through the getInstance() method.
 * In a real-world application, you will most likely want to make sure that only a single instance
 * may exist on the how system. And, furthermore, want to implement a proper license checking mechanism.
 */
class LicenseChecker : public ImplementationOf<ILicenseChecker>
{
public:    
    /**
     * @brief Retrieves the singleton instance of the LicenseChecker.
     * 
     * @param ptrModuleOverallObjectRefCounter Pointer to the atomic integer tracking the overall object reference count.
     * @return ILicenseChecker* Pointer to the singleton instance of LicenseChecker.
     */
    static ILicenseChecker* getInstance(std::atomic<int>* ptrModuleOverallObjectRefCounter);

    ErrCode INTERFACE_FUNC getNoOfFeatureTokens(const IString* feature, SizeT* overallCount, SizeT* remainingCount) override;
    ErrCode INTERFACE_FUNC checkOut(IString* feature, SizeT count) override;
    ErrCode INTERFACE_FUNC checkIn(IString* feature, SizeT count) override;

public:
    
    /// @brief Overloaded Ref-Counting method to keep track of external references in order to be able to troubleshoot module unload problems...
    /// @return The reference count after the increment.
    int INTERFACE_FUNC addRef() override;
    /// @brief Overloaded Ref-Counting method to keep track of external references in order to be able to troubleshoot module unload problems...
    /// @return The reference count after the decrement.
    int INTERFACE_FUNC releaseRef() override;

private:
    explicit LicenseChecker(std::atomic<int>* ptrModuleOverallObjectRefCounter);

    void ReloadLicenseFile(bool useLock);

private:
    static inline ILicenseChecker* _instance = nullptr;

    std::atomic<int>* _ptrModuleOverallObjectRefCounter;
    std::shared_ptr<spdlog::logger> _logger;
    std::map<std::string, SizeT> _featureTokensOverall;
    std::map<std::string, SizeT> _featureTokensCheckedOut;
};
/*!@}*/


END_NAMESPACE_LICENSE_LIBRARY
