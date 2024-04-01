#include <string>
#include <variant>

namespace lwip {

enum class ErrorCode {
  OK,
  AGAIN,
  UNKNOWN,
};

struct Error {
  ErrorCode code;
  std::string message;

  inline operator bool() const { return code == ErrorCode::OK; }
};

template <typename T>
using Maybe = std::variant<Error, T>;

}  // namespace lwip