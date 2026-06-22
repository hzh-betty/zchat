#include "common/crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <sodium.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace zchat {
namespace {

void EnsureSodiumInit() {
    static const int initialized = []() { return sodium_init(); }();
    (void)initialized;
}

} // namespace

std::string Base64Encode(const unsigned char *data, std::size_t length) {
    const std::size_t encoded_length = 4 * ((length + 2) / 3);
    std::string result(encoded_length, '\0');
    if (EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&result[0]), data,
                        static_cast<int>(length)) <= 0) {
        return std::string();
    }
    return result;
}

std::string Base64Encode(const std::string &input) {
    return Base64Encode(reinterpret_cast<const unsigned char *>(input.data()),
                        input.size());
}

std::string HmacSha1(const std::string &key, const std::string &message) {
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST,
                                                 const_cast<char *>("SHA1"), 0);
    params[1] = OSSL_PARAM_construct_end();

    auto *mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (mac == nullptr) {
        return std::string();
    }
    auto *ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (ctx == nullptr) {
        return std::string();
    }
    std::string digest;
    if (EVP_MAC_init(ctx, reinterpret_cast<const unsigned char *>(key.data()),
                     key.size(), params) != 1) {
        EVP_MAC_CTX_free(ctx);
        return std::string();
    }
    if (EVP_MAC_update(ctx,
                       reinterpret_cast<const unsigned char *>(message.data()),
                       message.size()) != 1) {
        EVP_MAC_CTX_free(ctx);
        return std::string();
    }
    std::size_t out_len = 0;
    if (EVP_MAC_final(ctx, nullptr, &out_len, 0) != 1) {
        EVP_MAC_CTX_free(ctx);
        return std::string();
    }
    digest.resize(out_len);
    if (EVP_MAC_final(ctx, reinterpret_cast<unsigned char *>(digest.data()),
                      &out_len, digest.size()) != 1) {
        EVP_MAC_CTX_free(ctx);
        return std::string();
    }
    EVP_MAC_CTX_free(ctx);
    digest.resize(out_len);
    return Base64Encode(reinterpret_cast<const unsigned char *>(digest.data()),
                        digest.size());
}

std::string UrlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase
                    << static_cast<int>(c);
        }
    }
    return escaped.str();
}

std::string Argon2idHash(const std::string &password) {
    EnsureSodiumInit();
    char encoded[crypto_pwhash_argon2id_STRBYTES];
    if (crypto_pwhash_argon2id_str(
            encoded, password.c_str(), password.size(),
            crypto_pwhash_argon2id_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_argon2id_MEMLIMIT_INTERACTIVE) != 0) {
        return std::string();
    }
    return std::string(encoded);
}

bool Argon2idVerify(const std::string &encoded, const std::string &password) {
    EnsureSodiumInit();
    return crypto_pwhash_argon2id_str_verify(encoded.c_str(), password.c_str(),
                                             password.size()) == 0;
}

std::string CsprngHex(std::size_t bytes) {
    EnsureSodiumInit();
    std::vector<unsigned char> buf(bytes);
    randombytes_buf(buf.data(), bytes);
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(bytes * 2, '\0');
    for (std::size_t i = 0; i < bytes; ++i) {
        result[i * 2] = kHex[buf[i] >> 4];
        result[i * 2 + 1] = kHex[buf[i] & 0x0f];
    }
    return result;
}

unsigned int CsprngUniform(unsigned int upper_exclusive) {
    EnsureSodiumInit();
    if (upper_exclusive == 0) {
        return 0;
    }
    return static_cast<unsigned int>(randombytes_uniform(upper_exclusive));
}

} // namespace zchat
