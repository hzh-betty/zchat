#include <cassert>
#include <string>

#include "common/crypto.h"

int main() {
    // Argon2idHash
    const std::string hash = zchat::Argon2idHash("password123");
    assert(!hash.empty());
    assert(hash.rfind("$argon2id$", 0) == 0);

    // Argon2idVerify — correct password
    assert(zchat::Argon2idVerify(hash, "password123"));

    // Argon2idVerify — wrong password
    assert(!zchat::Argon2idVerify(hash, "wrong"));

    // Argon2idVerify — empty encoded string
    assert(!zchat::Argon2idVerify("", "password123"));

    // Each hash has a unique salt
    const std::string hash2 = zchat::Argon2idHash("password123");
    assert(hash != hash2);
    assert(zchat::Argon2idVerify(hash2, "password123"));

    // CsprngHex — length
    const std::string hex16 = zchat::CsprngHex(16);
    assert(hex16.size() == 32);
    const std::string hex0 = zchat::CsprngHex(0);
    assert(hex0.empty());

    // CsprngHex — only hex chars
    for (char c : hex16) {
        assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    // CsprngHex — uniqueness
    assert(zchat::CsprngHex(16) != zchat::CsprngHex(16));

    // CsprngUniform — within range
    for (int i = 0; i < 100; ++i) {
        const unsigned int val = zchat::CsprngUniform(100);
        assert(val < 100);
    }

    // CsprngUniform — edge case
    assert(zchat::CsprngUniform(0) == 0);
    assert(zchat::CsprngUniform(1) == 0);

    // ConstantTimeCompare — equal
    assert(zchat::ConstantTimeCompare("abcdef", "abcdef"));

    // ConstantTimeCompare — not equal
    assert(!zchat::ConstantTimeCompare("abcdef", "abcdeg"));

    // ConstantTimeCompare — different lengths
    assert(!zchat::ConstantTimeCompare("abc", "abcd"));

    // ConstantTimeCompare — both empty
    assert(zchat::ConstantTimeCompare("", ""));

    // Base64Encode (existing, ensure no regression)
    const std::string encoded = zchat::Base64Encode("hello");
    assert(!encoded.empty());

    return 0;
}
