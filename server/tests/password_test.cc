#include <cassert>
#include <string>

#include "common/crypto.h"

int main() {
    // Argon2id 哈希格式验证
    const std::string password = "TestPass123";
    const std::string hash = zchat::Argon2idHash(password);
    assert(!hash.empty());
    assert(hash.rfind("$argon2id$", 0) == 0);

    // 正确密码验证
    assert(zchat::Argon2idVerify(hash, password));

    // 错误密码验证
    assert(!zchat::Argon2idVerify(hash, "WrongPass456"));

    // 每次 hash 使用不同 salt
    const std::string hash2 = zchat::Argon2idHash(password);
    assert(hash != hash2);
    assert(zchat::Argon2idVerify(hash2, password));

    // 模拟 legacy 明文迁移流程
    const std::string legacy_plaintext = "OldPassword789";
    // legacy 阶段：常数时间比较验证明文
    assert(zchat::ConstantTimeCompare(legacy_plaintext, "OldPassword789"));
    assert(!zchat::ConstantTimeCompare(legacy_plaintext, "WrongPassword"));
    // 迁移后：用 Argon2id 重新哈希
    const std::string migrated_hash = zchat::Argon2idHash(legacy_plaintext);
    assert(migrated_hash.rfind("$argon2id$", 0) == 0);
    assert(zchat::Argon2idVerify(migrated_hash, legacy_plaintext));
    assert(!zchat::Argon2idVerify(migrated_hash, "WrongPassword"));

    // 空密码处理
    const std::string empty_hash = zchat::Argon2idHash("");
    assert(!empty_hash.empty());
    assert(zchat::Argon2idVerify(empty_hash, ""));

    // 长密码处理
    const std::string long_password(64, 'x');
    const std::string long_hash = zchat::Argon2idHash(long_password);
    assert(zchat::Argon2idVerify(long_hash, long_password));

    return 0;
}
