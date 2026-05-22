#ifndef ZCHAT_SERVER_SRC_COMMON_NONCOPYABLE_H_
#define ZCHAT_SERVER_SRC_COMMON_NONCOPYABLE_H_

namespace zchat {

class NonCopyable {
  protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
    NonCopyable(NonCopyable &&) = delete;
    NonCopyable &operator=(NonCopyable &&) = delete;
};

} // namespace zchat

#endif // ZCHAT_SERVER_SRC_COMMON_NONCOPYABLE_H_
