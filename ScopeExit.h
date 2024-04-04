#pragma once

#include <utility>

#define CPPRUN_CAT_IMPL(a, b) a##b
#define CPPRUN_CAT(a, b) CPPRUN_CAT_IMPL(a, b)

namespace cpprun {
template <class Fn> struct ScopeExit {
private:
  Fn m_fn;

public:
  ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;
  ScopeExit(ScopeExit &&) noexcept = delete;
  ScopeExit &operator=(ScopeExit &&) noexcept = delete;
  explicit ScopeExit(Fn fn) noexcept : m_fn{std::move(fn)} {}
  ~ScopeExit() { m_fn(); }
};

namespace detail {
enum class ScopeExitHelper {};
template <class Fn> auto operator+(ScopeExitHelper, Fn &&fn) noexcept {
  return ScopeExit{std::forward<Fn>(fn)};
}
} // namespace detail


#define SCOPE_EXIT const auto CPPRUN_CAT(scope_exit_, __LINE__) = cpprun::detail::ScopeExitHelper{} + [&]
} // namespace cpprun