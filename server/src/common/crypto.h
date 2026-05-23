#ifndef ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_
#define ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_

#include <string>
#include <vector>

namespace zchat {

std::string Base64Encode(const std::string &input);
std::string Base64Encode(const unsigned char *data, std::size_t length);
std::string HmacSha1(const std::string &key, const std::string &message);
std::string UrlEncode(const std::string &value);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_