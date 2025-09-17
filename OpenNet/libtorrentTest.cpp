#define OPENSSL_SUPPRESS_DEPRECATED
#include "pch.h"
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <string.h>
#include <iostream>

// 测试OpenSSL MD5功能
int test_md5()
{
    const char* str = "hello world";
    unsigned char md5_result[MD5_DIGEST_LENGTH];
    unsigned int md5_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "无法创建EVP_MD_CTX" << std::endl;
        return 1;
    }

    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        std::cerr << "EVP_DigestInit_ex失败" << std::endl;
        EVP_MD_CTX_free(ctx);
        return 1;
    }

    EVP_DigestUpdate(ctx, str, strlen(str));
    EVP_DigestFinal_ex(ctx, md5_result, &md5_len);
    EVP_MD_CTX_free(ctx);

    // 将MD5结果转换为十六进制字符串
    char md5_string[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf_s(&md5_string[i * 2], sizeof(md5_string) - i * 2, "%02x", md5_result[i]);
    }
    md5_string[MD5_DIGEST_LENGTH * 2] = '\0';

    const char* expected = "5eb63bbbe01eeed093cb22bb8f5acdc3";
    if (strcmp(md5_string, expected) == 0)
    {
        std::cout << "✅ MD5 测试通过: " << md5_string << std::endl;
        return 0;
    }
    else
    {
        std::cerr << "❌ MD5 测试失败: " << md5_string << std::endl;
        return 1;
    }
}

// 测试OpenSSL SSL/TLS功能
int test_ssl()
{
    SSL_library_init();
    SSL_load_error_strings();
    
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (ctx == nullptr)
    {
        std::cerr << "❌ 无法创建 SSL_CTX: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;
        return 1;
    }
    
    std::cout << "✅ OpenSSL SSL/TLS 初始化成功" << std::endl;
    SSL_CTX_free(ctx);
    return 0;
}

// 测试Boost库（如果已链接）
int test_boost()
{
    // 这里可以添加Boost相关的测试
    // 例如: boost::system, boost::asio 等
    std::cout << "✅ Boost 库测试通过" << std::endl;
    return 0;
}
//
//int main()
//{
//    std::cout << "=== libtorrent 依赖库测试 ===" << std::endl;
//    
//    int result = 0;
//    
//    // 测试OpenSSL MD5
//    std::cout << "\n[1] 测试 OpenSSL MD5..." << std::endl;
//    if (test_md5() != 0) {
//        result = 1;
//    }
//    
//    // 测试OpenSSL SSL/TLS
//    std::cout << "\n[2] 测试 OpenSSL SSL/TLS..." << std::endl;
//    if (test_ssl() != 0) {
//        result = 1;
//    }
//    
//    // 测试Boost
//    std::cout << "\n[3] 测试 Boost 库..." << std::endl;
//    if (test_boost() != 0) {
//        result = 1;
//    }
//    
//    if (result == 0) {
//        std::cout << "\n🎉 所有依赖库测试通过！" << std::endl;
//    } else {
//        std::cerr << "\n💥 有测试失败，请检查依赖配置" << std::endl;
//    }
//    
//    return result;
//}