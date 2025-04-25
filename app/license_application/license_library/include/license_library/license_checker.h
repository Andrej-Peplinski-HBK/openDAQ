#pragma once

#include <license_library/common.h>
#include <opendaq/component.h>

BEGIN_NAMESPACE_LICENSE_LIBRARY

DECLARE_OPENDAQ_INTERFACE(ILicenseChecker, IBaseObject)
{
    virtual ErrCode INTERFACE_FUNC getNoOfFeatureTokens(const IString* feature, SizeT* count) = 0;
    virtual ErrCode INTERFACE_FUNC checkOut(IString * feature, SizeT count) = 0;
    virtual ErrCode INTERFACE_FUNC checkIn(IString * feature, SizeT count) = 0;
    // ToDo: Extend interface to allow to show all available features, their total and consumed tokens and their expiry date...
};

class LicenseChecker : public ImplementationOf<ILicenseChecker>
{
public:
    
    // Static method to get the singleton, !!!not-reference!!! added instance
    static ILicenseChecker* getInstance(std::atomic<int>* ptrModuleOverallObjectRefCounter);   
    ~LicenseChecker() override;

    ErrCode INTERFACE_FUNC getNoOfFeatureTokens(const IString* feature, SizeT* count) override;
    ErrCode INTERFACE_FUNC checkOut(IString* feature, SizeT count) override;
    ErrCode INTERFACE_FUNC checkIn(IString* feature, SizeT count) override;

public:
    int INTERFACE_FUNC addRef() override;
    int INTERFACE_FUNC releaseRef() override;

private:
    explicit LicenseChecker(std::atomic<int>* ptrModuleOverallObjectRefCounter);
    LicenseChecker(const LicenseChecker&) = delete;
    LicenseChecker& operator=(const LicenseChecker&) = delete;

private:
    static inline ILicenseChecker* _instance = nullptr;

    std::atomic<int>* _ptrModuleOverallObjectRefCounter;
};

//Not needed for now: OPENDAQ_DECLARE_CLASS_FACTORY(LIBRARY_FACTORY, LicenseChecker)

END_NAMESPACE_LICENSE_LIBRARY
