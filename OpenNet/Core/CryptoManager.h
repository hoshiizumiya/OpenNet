#pragma once

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Storage.Streams.h>
#include <vector>
#include <string>
#include <memory>

namespace OpenNet::Core
{
    // 加密算法类型 / Encryption Algorithm Type
    enum class EncryptionAlgorithm
    {
        AES_128,                       // AES 128位 / AES 128-bit
        AES_256,                       // AES 256位 / AES 256-bit
        ChaCha20                       // ChaCha20
    };

    // 哈希算法类型 / Hash Algorithm Type
    enum class HashAlgorithm
    {
        MD5,                           // MD5 (不推荐用于安全 / Not recommended for security)
        SHA1,                          // SHA-1 (不推荐用于安全 / Not recommended for security)
        SHA256,                        // SHA-256
        SHA512                         // SHA-512
    };

    // 密钥交换方法 / Key Exchange Method
    enum class KeyExchangeMethod
    {
        DiffieHellman,                 // Diffie-Hellman
        ECDH,                          // 椭圆曲线Diffie-Hellman / Elliptic Curve Diffie-Hellman
        RSA                            // RSA
    };

    // 加密密钥信息 / Encryption Key Information
    struct EncryptionKey
    {
        std::vector<uint8_t> key;      // 密钥数据 / Key Data
        std::vector<uint8_t> iv;       // 初始化向量 / Initialization Vector
        EncryptionAlgorithm algorithm; // 加密算法 / Encryption Algorithm
        std::chrono::system_clock::time_point created; // 创建时间 / Created Time
        std::chrono::system_clock::time_point expires;  // 过期时间 / Expiry Time

        EncryptionKey()
            : algorithm(EncryptionAlgorithm::AES_256)
            , created(std::chrono::system_clock::now())
            , expires(std::chrono::system_clock::now() + std::chrono::hours(24))
        {
        }
    };

    // 数字签名信息 / Digital Signature Information
    struct DigitalSignature
    {
        std::vector<uint8_t> signature; // 签名数据 / Signature Data
        std::vector<uint8_t> publicKey; // 公钥 / Public Key
        HashAlgorithm hashAlgorithm;    // 哈希算法 / Hash Algorithm
        std::chrono::system_clock::time_point timestamp; // 时间戳 / Timestamp

        DigitalSignature()
            : hashAlgorithm(HashAlgorithm::SHA256)
            , timestamp(std::chrono::system_clock::now())
        {
        }
    };

    // 加密管理器类 / Crypto Manager Class
    class CryptoManager
    {
    public:
        CryptoManager();
        ~CryptoManager();

        // 初始化 / Initialize
        winrt::Windows::Foundation::IAsyncOperation<bool> InitializeAsync();

        // 对称加密 / Symmetric Encryption
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> EncryptAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            const EncryptionKey& key);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> DecryptAsync(
            winrt::Windows::Storage::Streams::IBuffer const& encryptedData,
            const EncryptionKey& key);

        // 非对称加密 / Asymmetric Encryption
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> EncryptWithPublicKeyAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            const std::vector<uint8_t>& publicKey);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> DecryptWithPrivateKeyAsync(
            winrt::Windows::Storage::Streams::IBuffer const& encryptedData,
            const std::vector<uint8_t>& privateKey);

        // 密钥管理 / Key Management
        winrt::Windows::Foundation::IAsyncOperation<EncryptionKey> GenerateSymmetricKeyAsync(
            EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES_256);

        winrt::Windows::Foundation::IAsyncOperation<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> 
            GenerateAsymmetricKeyPairAsync();

        winrt::Windows::Foundation::IAsyncOperation<EncryptionKey> DeriveKeyFromPasswordAsync(
            const std::wstring& password,
            const std::vector<uint8_t>& salt,
            uint32_t iterations = 10000);

        // 密钥交换 / Key Exchange
        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> GenerateKeyExchangeDataAsync(
            KeyExchangeMethod method = KeyExchangeMethod::ECDH);

        winrt::Windows::Foundation::IAsyncOperation<EncryptionKey> CompleteKeyExchangeAsync(
            const std::vector<uint8_t>& remoteKeyData,
            const std::vector<uint8_t>& localPrivateKey,
            KeyExchangeMethod method = KeyExchangeMethod::ECDH);

        // 哈希计算 / Hash Calculation
        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> CalculateHashAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            HashAlgorithm algorithm = HashAlgorithm::SHA256);

        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> CalculateFileHashAsync(
            winrt::Windows::Storage::StorageFile const& file,
            HashAlgorithm algorithm = HashAlgorithm::SHA256);

        // 数字签名 / Digital Signature
        winrt::Windows::Foundation::IAsyncOperation<DigitalSignature> SignDataAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            const std::vector<uint8_t>& privateKey);

        winrt::Windows::Foundation::IAsyncOperation<bool> VerifySignatureAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            const DigitalSignature& signature);

        // 随机数生成 / Random Number Generation
        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> GenerateRandomBytesAsync(uint32_t length);
        winrt::Windows::Foundation::IAsyncOperation<std::wstring> GenerateRandomStringAsync(
            uint32_t length,
            bool includeSymbols = false);

        // 安全擦除 / Secure Wipe
        void SecureWipeMemory(void* ptr, size_t size);
        void SecureWipeVector(std::vector<uint8_t>& data);

        // 密钥存储 / Key Storage
        winrt::Windows::Foundation::IAsyncAction StoreKeyAsync(
            const std::wstring& keyId,
            const EncryptionKey& key);

        winrt::Windows::Foundation::IAsyncOperation<EncryptionKey> LoadKeyAsync(
            const std::wstring& keyId);

        winrt::Windows::Foundation::IAsyncAction DeleteKeyAsync(
            const std::wstring& keyId);

        // 证书管理 / Certificate Management
        struct Certificate
        {
            std::wstring subjectName;       // 主题名称 / Subject Name
            std::wstring issuerName;        // 颁发者名称 / Issuer Name
            std::vector<uint8_t> publicKey; // 公钥 / Public Key
            std::vector<uint8_t> certificateData; // 证书数据 / Certificate Data
            std::chrono::system_clock::time_point validFrom;   // 有效期开始 / Valid From
            std::chrono::system_clock::time_point validTo;     // 有效期结束 / Valid To
            bool isValid;                   // 是否有效 / Is Valid
        };

        winrt::Windows::Foundation::IAsyncOperation<Certificate> GenerateSelfSignedCertificateAsync(
            const std::wstring& subjectName);

        winrt::Windows::Foundation::IAsyncOperation<bool> ValidateCertificateAsync(
            const Certificate& certificate);

        // 安全配置 / Security Configuration
        void SetEncryptionAlgorithm(EncryptionAlgorithm algorithm);
        void SetHashAlgorithm(HashAlgorithm algorithm);
        void SetKeyExchangeMethod(KeyExchangeMethod method);
        void EnableSecureMemory(bool enable);

        // 安全统计 / Security Statistics
        struct SecurityStatistics
        {
            uint64_t encryptionsPerformed;
            uint64_t decryptionsPerformed;
            uint64_t keysGenerated;
            uint64_t signaturesCreated;
            uint64_t signaturesVerified;
            uint32_t activeKeys;
        };

        SecurityStatistics GetSecurityStatistics() const;

    private:
        // 内部加密实现 / Internal Encryption Implementation
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> EncryptAESAsync(
            winrt::Windows::Storage::Streams::IBuffer const& data,
            const std::vector<uint8_t>& key,
            const std::vector<uint8_t>& iv);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> DecryptAESAsync(
            winrt::Windows::Storage::Streams::IBuffer const& encryptedData,
            const std::vector<uint8_t>& key,
            const std::vector<uint8_t>& iv);

        // 密钥派生 / Key Derivation
        winrt::Windows::Foundation::IAsyncOperation<std::vector<uint8_t>> DeriveKeyPBKDF2Async(
            const std::wstring& password,
            const std::vector<uint8_t>& salt,
            uint32_t iterations,
            uint32_t keyLength);

        // 实用方法 / Utility Methods
        std::vector<uint8_t> StringToBytes(const std::wstring& str);
        std::wstring BytesToString(const std::vector<uint8_t>& bytes);
        std::wstring BytesToHex(const std::vector<uint8_t>& bytes);
        std::vector<uint8_t> HexToBytes(const std::wstring& hex);

        // 验证方法 / Validation Methods
        bool ValidateKeySize(EncryptionAlgorithm algorithm, uint32_t keySize);
        bool ValidateIVSize(EncryptionAlgorithm algorithm, uint32_t ivSize);

    private:
        // 配置 / Configuration
        EncryptionAlgorithm m_defaultEncryptionAlgorithm;
        HashAlgorithm m_defaultHashAlgorithm;
        KeyExchangeMethod m_defaultKeyExchangeMethod;
        bool m_secureMemoryEnabled;

        // 密钥存储 / Key Storage
        std::unordered_map<std::wstring, EncryptionKey> m_keyStore;
        mutable std::mutex m_keyStoreMutex;

        // 统计信息 / Statistics
        SecurityStatistics m_statistics;
        mutable std::mutex m_statisticsMutex;

        // WinRT加密提供者 / WinRT Crypto Providers
        winrt::Windows::Security::Cryptography::Core::SymmetricKeyAlgorithmProvider m_aesProvider;
        winrt::Windows::Security::Cryptography::Core::AsymmetricKeyAlgorithmProvider m_rsaProvider;
        winrt::Windows::Security::Cryptography::Core::HashAlgorithmProvider m_hashProvider;
        winrt::Windows::Security::Cryptography::Core::KeyDerivationAlgorithmProvider m_kdfProvider;

        // 常量 / Constants
        static constexpr uint32_t AES_128_KEY_SIZE = 16;   // 128 bits = 16 bytes
        static constexpr uint32_t AES_256_KEY_SIZE = 32;   // 256 bits = 32 bytes
        static constexpr uint32_t AES_BLOCK_SIZE = 16;     // AES block size = 16 bytes
        static constexpr uint32_t DEFAULT_SALT_SIZE = 32;  // 256 bits = 32 bytes
        static constexpr uint32_t DEFAULT_IV_SIZE = 16;    // 128 bits = 16 bytes
        static constexpr uint32_t RSA_KEY_SIZE = 2048;     // RSA key size in bits
    };
}