#include <license_library/license_checker.h>
#include <mutex>

BEGIN_NAMESPACE_LICENSE_LIBRARY

std::mutex _mutex;
ILicenseChecker* LicenseChecker::getInstance(std::atomic<int>* ptrModuleOverallObjectRefCounter)
{
    if (_instance == nullptr)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        //Double-check after lock...
        if (_instance == nullptr)
            _instance = new LicenseChecker(ptrModuleOverallObjectRefCounter);
    }

    return _instance;
}
LicenseChecker::LicenseChecker(std::atomic<int>* ptrModuleOverallObjectRefCounter)
    : _ptrModuleOverallObjectRefCounter(ptrModuleOverallObjectRefCounter)
{
    assert(ptrModuleOverallObjectRefCounter != nullptr);
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

ErrCode LicenseChecker::getNoOfFeatureTokens(const IString* feature, SizeT* count)
{
    *count = 0;
    return OPENDAQ_ERR_NOTIMPLEMENTED;
}

ErrCode LicenseChecker::checkOut(IString* feature, SizeT count)
{
    return OPENDAQ_ERR_NOTIMPLEMENTED;
}

ErrCode LicenseChecker::checkIn(IString* feature, SizeT count)
{
    return OPENDAQ_ERR_NOTIMPLEMENTED;
}

// OPENDAQ_DEFINE_CLASS_FACTORY(LIBRARY_FACTORY, LicenseChecker)
// extern "C" daq::ErrCode PUBLIC_EXPORT createLicenseChecker()
//{
//     return daq::createObject<IUnit, UnitImpl>(objTmp, id, symbol, name, quantity);
// }

END_NAMESPACE_LICENSE_LIBRARY
