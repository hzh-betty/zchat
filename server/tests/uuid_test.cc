#include <cassert>
#include <string>

#include "common/crypto.h"
#include "common/uuid.h"

int main() {
    // NewId — 16 bytes = 32 hex chars
    const std::string id1 = zchat::NewId();
    assert(id1.size() == 32);

    // NewId — only hex chars
    for (char c : id1) {
        assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    // NewId — uniqueness
    assert(zchat::NewId() != zchat::NewId());

    // NewRequestId — prefix "R" + 6 bytes = 12 hex chars = 13 total
    const std::string req1 = zchat::NewRequestId();
    assert(req1.size() == 13);
    assert(req1[0] == 'R');

    // NewRequestId — uniqueness
    assert(zchat::NewRequestId() != zchat::NewRequestId());

    // NewVerifyCode — 6 digit string
    const std::string code1 = zchat::NewVerifyCode();
    assert(code1.size() == 6);
    for (char c : code1) {
        assert(c >= '0' && c <= '9');
    }

    // NewVerifyCode — within [0, 999999]
    const int code_val = std::stoi(code1);
    assert(code_val >= 0 && code_val <= 999999);

    // UnixTimeSeconds — reasonable (after 2024-01-01)
    const std::int64_t ts = zchat::UnixTimeSeconds();
    assert(ts > 1704067200LL);

    return 0;
}
