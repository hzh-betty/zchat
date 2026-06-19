#ifndef ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_
#define ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace zchat {

std::string Base64Encode(const std::string &input);
std::string Base64Encode(const unsigned char *data, std::size_t length);
std::string HmacSha1(const std::string &key, const std::string &message);
std::string UrlEncode(const std::string &value);

std::string Argon2idHash(const std::string &password);

bool Argon2idVerify(const std::string &encoded, const std::string &password);

std::string CsprngHex(std::size_t bytes);

unsigned int CsprngUniform(unsigned int upper_exclusive);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_
