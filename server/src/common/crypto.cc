#include "common/crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace zchat {

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

} // namespace zchat