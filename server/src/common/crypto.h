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

// Argon2id 密码哈希，返回标准编码串（含 salt），可直接存库。
std::string Argon2idHash(const std::string &password);

// 验证 Argon2id 编码串与明文密码是否匹配。
bool Argon2idVerify(const std::string &encoded, const std::string &password);

// 使用 CSPRNG 生成指定字节数的随机十六进制字符串。
std::string CsprngHex(std::size_t bytes);

// 使用 CSPRNG 生成 [0, upper_exclusive) 范围内的随机数。
unsigned int CsprngUniform(unsigned int upper_exclusive);

// 常数时间内存比较，长度不等时直接返回 false。
bool ConstantTimeCompare(std::string_view a, std::string_view b);

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_CRYPTO_H_
