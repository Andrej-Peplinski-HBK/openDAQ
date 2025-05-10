#include <iostream>
#include <vector>
#include <algorithm> // For std::equal

#include <protected_analytics_module/license_module_verification.h>

#ifdef WIN32
    #include <windows.h>
    #include <Softpub.h>
    #include <wincrypt.h>
    #include <wintrust.h>
    #include <mscat.h>

// The implementation of this function has been inspired by https://github.com/dragokas/Verify-Signature-Cpp/blob/master/verify.cpp#L140.
daq::ErrCode CanTrustLicenseModule(const fs::path& path, const std::vector<uint8_t>& expected_license_hashBuffer, daq::IString** errMsg)
{
    if (expected_license_hashBuffer.empty())
    {
        std::cerr << "No license hash provided!" << std::endl;
        return OPENDAQ_ERR_INVALID_ARGUMENT;
    }

    HCATADMIN hCatAdmin = NULL;
    const GUID DriverGuid = DRIVER_ACTION_VERIFY;
    if (!CryptCATAdminAcquireContext(&hCatAdmin, &DriverGuid, 0))
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to initialize the Windows Crypto API! (Win32 error code: " << win32ErrCode << ")!" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;  // ¿Don't proceed when having problems with the Windows Crypto API?        
    }

    HANDLE hFile = CreateFileW(path.c_str(),
                               FILE_READ_ATTRIBUTES | FILE_READ_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to open the license module file! (Win32 error code: " << win32ErrCode << ")" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }
    
    WINTRUST_DATA wd = {};
    wd.cbStruct = sizeof(WINTRUST_DATA);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    wd.dwUnionChoice = WTD_CHOICE_FILE;

    WINTRUST_FILE_INFO wfi = {};
    wd.pFile = &wfi;

    wfi.cbStruct = sizeof(WINTRUST_FILE_INFO);
    wfi.pcwszFilePath = NULL;
    wfi.hFile = hFile;
    wfi.pgKnownSubject = NULL;

    GUID VerifyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const auto trustResultCode = WinVerifyTrust((HWND) INVALID_HANDLE_VALUE, &VerifyGuid, &wd);

    //Make sure to close the file handle as it's no longer needed!
    CloseHandle(hFile);
    hFile = NULL;

    if (trustResultCode != ERROR_SUCCESS
     && trustResultCode != CERT_E_UNTRUSTEDROOT     // Accept self-signed certificates for this demo...
     && trustResultCode != CERT_E_EXPIRED)          // Accept expired certificates for this demo...
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "¡¡¡ The user is most likely trying to a use a compromised license dll !!! (Return code: " << std::hex << win32ErrCode << std::dec << ")" << std::endl;

        return OPENDAQ_ERR_INVALID_OPERATION;
    }

    // Now verify the signature of the license module
    CRYPT_PROVIDER_DATA* pProvData = WTHelperProvDataFromStateData(wd.hWVTStateData);
    if (pProvData == NULL)
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to access the certificate state data! (Win32 error code: " << win32ErrCode << ")" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }

    const int idxSigner = 0;
    CRYPT_PROVIDER_SGNR* pCPSigner = WTHelperGetProvSignerFromChain(pProvData, idxSigner, FALSE, 0);
    if (pCPSigner == NULL)
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to access the certificate signer data! (Win32 error code: " << win32ErrCode << ")" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }

    PCCERT_CONTEXT pCertificate = CertDuplicateCertificateContext(pCPSigner->pasCertChain->pCert);
    if (pCertificate == NULL)
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to access the certificate context! (Win32 error code: " << win32ErrCode << ")" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }

    DWORD size = 0;
    CertGetCertificateContextProperty(pCertificate, CERT_HASH_PROP_ID, NULL, &size);
    if (size != expected_license_hashBuffer.size())
    {
        std::cerr << "Certificate hash validation failed (size of current != expected - "
            << size << "!=" << expected_license_hashBuffer.size() << ")!" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }

    std::vector<uint8_t> current_license_hashBuffer(size);
    if (!CertGetCertificateContextProperty(pCertificate, CERT_HASH_PROP_ID, &current_license_hashBuffer.front(), &size))
    {
        const auto win32ErrCode = GetLastError();
        std::cerr << "Failed to access the certificate hash property! (Win32 error code: " << win32ErrCode << ")" << std::endl;
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }

    std::string certificate_name;
    size = CertGetNameStringA(pCertificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, NULL);
    if (size)
    {
        std::vector<char> buff(size);
        CertGetNameStringA(pCertificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, &buff.front(), size);
        certificate_name = std::string(buff.cbegin(), buff.cend());
    }
    else
    {
        certificate_name = "Unknown";
    }

    // Get a printable version of the (SHA1) of the initial (signing) certificate returned by >> signtool verify /pa /v ${MODULE_PATH}
    auto getPrintableHash = [](const std::vector<uint8_t>& buff) -> std::string
    {
        const size_t MAX_HASH_STRING_SIZE = 100;
        char pszMemberTag[MAX_HASH_STRING_SIZE] = {0};

        const size_t printSize = std::min(MAX_HASH_STRING_SIZE / 2, buff.size());

        for (size_t i = 0; i < printSize; ++i)
            sprintf(&pszMemberTag[i * 2], "%02X", buff[i]);

        return std::string(pszMemberTag);
    };

    const auto current_license_hash = getPrintableHash(current_license_hashBuffer);

    if (std::equal(current_license_hashBuffer.cbegin(), current_license_hashBuffer.cend(), expected_license_hashBuffer.cbegin()))
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        {
            std::cout << "Successfully verified the certificate (\"" << certificate_name << ", hash: " << current_license_hash << ") of the license module:" << path << std::endl;
        }
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        return OPENDAQ_SUCCESS;
    }
    else
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        {
            const auto expected_license_hash = getPrintableHash(expected_license_hashBuffer);
            std::cerr << "Failed to verify the certificate (\"" << certificate_name << ", current hash: " << current_license_hash
                       << " vs. expected hash: " << expected_license_hash
                       << ") of the license module:" << path << std::endl;
        }
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        *errMsg = daq::String("No, no you cannot run with an invalid license file!!!").detach();
        return OPENDAQ_ERR_VALIDATE_FAILED;
    }
}

#else
    //Must not issue warning because it causes build problems on
    // * Ubuntu 22.04 - clang 14 configuration  (see: https://github.com/Andrej-Peplinski-HBK/openDAQ/actions/runs/14894021156/job/41832684391)
    // * manylinux - gcc configuration          (see: https://github.com/Andrej-Peplinski-HBK/openDAQ/actions/runs/14894021156/job/41832684425)
    // * macos-13 - clang configuration         (see: https://github.com/Andrej-Peplinski-HBK/openDAQ/actions/runs/14894021156/job/41832684386)
    //#warning "A valid implementation of 'CanTrustLicenseModule' is not available for this platform."

    daq::ErrCode CanTrustLicenseModule(const fs::path& path, const std::vector<uint8_t>& expected_license_hashBuffer, daq::IString** errMsg)
    {
        std::cerr << "The verification of the license module has not been implemented!!!" << std::endl;
        
        //Use cmake commands to sign the file
        // openssl genpkey -algorithm RSA -out private_key.pem        
        // openssl rsa -pubout -in private_key.pem -out public_key.pem
        // openssl dgst -sha256 -sign private_key.pem -out tmplibLicenseLibrary.signature libLicenseLibrary-64-3-signed.so
        // objcopy --add-section .signature=tmplibLicenseLibrary.signature --set-section-flags .signature=noload,readonly libLicenseLibrary-64-3-signed.so libLicenseLibrary-64-3-signed.so

        //Manually verify the signature on the commandline
        // objcopy --dump-section .signature=extracted_sig.bin libLicenseLibrary-64-3-signed.so
        // openssl dgst -sha256 -verify public_key.pem -signature extracted_sig.bin libLicenseLibrary-64-3-debug.so //Here we must use the library without the extra section!!!

        // Load the public key
        //     FILE* pub_key_file = fopen(public_key_path, "r");
        //     RSA* rsa_pub_key = PEM_read_RSA_PUBKEY(pub_key_file, NULL, NULL, NULL);
        //     fclose(pub_key_file);

        return  OPENDAQ_SUCCESS;    //... just to get the compilation going ...
    }
#endif  // WIN32


