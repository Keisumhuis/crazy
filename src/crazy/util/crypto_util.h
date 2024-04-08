/**
 * @file crypto_util.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_CRYPTO_UTIL_H____
#define ____CRAZY_CRYPTO_UTIL_H____

#include <stdint.h>
#include <stdio.h>

#include <iostream>
#include <memory>
#include <string>

#include <openssl/ssl.h>
#include <openssl/evp.h>

namespace crazy {

    class CryptoUtil {
    public:
        static int32_t AES256Ecb(const void* key
                                ,const void* in
                                ,int32_t in_len
                                ,void* out
                                ,bool encode);
        static int32_t AES128Ecb(const void* key
                                ,const void* in
                                ,int32_t in_len
                                ,void* out
                                ,bool encode);
        static int32_t AES256Cbc(const void* key, const void* iv
                                ,const void* in, int32_t in_len
                                ,void* out, bool encode);
        static int32_t AES128Cbc(const void* key, const void* iv
                                ,const void* in, int32_t in_len
                                ,void* out, bool encode);
        static int32_t Crypto(const EVP_CIPHER* cipher, bool enc
                              ,const void* key, const void* iv
                              ,const void* in, int32_t in_len
                              ,void* out, int32_t* out_len);
    };


    class RSACipher {
    public:
        typedef std::shared_ptr<RSACipher> ptr;
        static int32_t GenerateKey(const std::string& pubkey_file
                                   ,const std::string& prikey_file
                                   ,uint32_t length = 1024);
        static RSACipher::ptr Create(const std::string& pubkey_file
                                    ,const std::string& prikey_file);
        RSACipher();
        ~RSACipher();
        int32_t privateEncrypt(const void* from, int flen,
                               void* to, int padding = RSA_NO_PADDING);
        int32_t publicEncrypt(const void* from, int flen,
                               void* to, int padding = RSA_NO_PADDING);
        int32_t privateDecrypt(const void* from, int flen,
                               void* to, int padding = RSA_NO_PADDING);
        int32_t publicDecrypt(const void* from, int flen,
                               void* to, int padding = RSA_NO_PADDING);
        int32_t privateEncrypt(const void* from, int flen,
                               std::string& to, int padding = RSA_NO_PADDING);
        int32_t publicEncrypt(const void* from, int flen,
                               std::string& to, int padding = RSA_NO_PADDING);
        int32_t privateDecrypt(const void* from, int flen,
                               std::string& to, int padding = RSA_NO_PADDING);
        int32_t publicDecrypt(const void* from, int flen,
                               std::string& to, int padding = RSA_NO_PADDING);
        const std::string& getPubkeyStr() const { return m_pubkeyStr;}
        const std::string& getPrikeyStr() const { return m_prikeyStr;}
        int32_t getPubRSASize();
        int32_t getPriRSASize();
    private:
        RSA* m_pubkey;
        RSA* m_prikey;
        std::string m_pubkeyStr;
        std::string m_prikeyStr;
    };

}

#endif // ! ____CRAZY_CRYPTO_UTIL_H____