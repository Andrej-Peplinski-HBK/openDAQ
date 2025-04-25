# Welcome to the License demo application

## Introduction
This application show-cases how to prevent that a custom `protected_analytics_module` can be used in a demo environment, where a user might have tempered with a fictious license component. The application consists of three modules:

* `demo application` (console executable)
* `protected_analytics_module` (dll/so)
* `license_library` (dll/so)

where `protected_analytics_module` contains the functionality that we want to protect. This is achieved by using `license_library` that verifies that the correct number of licenses are available. To prevent malicious users from "patching" the `license_library` to circumvent the license protection; we will digitally sign the `license_library`. Thus, any attempt to modify `license_library` will break the digital signature - which will be discovered by the `protected_analytics_module` module, which consequently refused to be loaded into the [openDAQ](../../README.md) SDK.

## Preliminary steps
1. Create your own, self-signed certificate (in absence of a proper certificate)
    
    > **Note**: The self-signed certificate created here is for testing purposes only and should not be used in production environments. For production, obtain a certificate from a trusted Certificate Authority (CA).

    Powershell:
    ```powershell
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=HBK Test Certificate Authority (CA)" -CertStoreLocation cert:\currentuser\my -KeyAlgorithm rsa -Provider "Microsoft Enhanced Cryptographic Provider v1.0"

2. Obtain PFX certificate file
    
    Powershell:
    ```powershell
    Export-PfxCertificate -Cert $cert -FilePath "Selfsigned-CA.pfx" -Password (ConvertTo-SecureString -String "Passw0rd123" -Force -AsPlainText)
    ```

3. Sign a copy of `license_library`
    
    VS command prompt (requires [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)):
    ```cmd
    signtool.exe sign /v /n "Test Certificate Authority (CA)" /f "Selfsigned-CA.pfx" /fd SHA256 /t http://timestamp.digicert.com license_library-signed.dll
    ```


4. (Optionally) Create an tempered version of `license_library`

    One way of creating a tempered `license_library` is to use your favourite hex editor - z.B.
    ```cmd
    winget install hxd
    "C:\Program Files\HxD\HxD.exe" license_library-tempered.dll
    ```
    to flip a bit in the binary. This will invalidate the digitial signature of file will be invalidated. See 👇...

5. (Optionally) Verify signature

    VS command prompt (using [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)):
    ```cmd
    signtool.exe verify /pa /v license_library-tempered.dll
    ```

    Command promt (alternatively using [Sigcheck](https://learn.microsoft.com/en-us/sysinternals/downloads/sigcheck)):
    ```cmd
    sigcheck license_library-tempered.dll
    ```

## Workflow
After compilation, you will be able to run the application from the output directory.

It is recommended to have three versions of `license_library`:

* An <b>unsigned</b> version (created through the build)
* A <b>signed</b> version ([see](#preliminary-steps) 👆)
* A <b>tempered</b> version ([see](#preliminary-steps) 👆)

Depending on the test scenario copy the corresponding version to the output directory before starting the application. The application will then make sure that only in the case of a correctly signed `license_library` the `protected_analytics_module` will be loaded into the [openDAQ](../../README.md).
