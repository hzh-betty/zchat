#include "common/crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
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
    EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&result[0]), data,
                    static_cast<int>(length));
    return result;
}

std::string Base64Encode(const std::string &input) {
    return Base64Encode(reinterpret_cast<const unsigned char *>(input.data()),
                        input.size());
}

std::string HmacSha1(const std::string &key, const std::string &message) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_length = 0;
    HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char *>(message.data()),
         message.size(), digest, &digest_length);
    return Base64Encode(digest, digest_length);
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
