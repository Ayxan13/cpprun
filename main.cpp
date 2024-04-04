#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "ScopeExit.h"

using std::string_literals::operator""s;
using std::string_view_literals::operator""sv;

namespace {
int sendFile(const int src, const int dst) noexcept {
  struct ::stat fileStat = {};
  if (::fstat(src, &fileStat) == -1) {
    std::perror("stat");
    return -1;
  }

  ::ssize_t sentSoFar = 0;
  for (;;) {
    const auto sent =
        ::sendfile(dst, src, nullptr, fileStat.st_size - sentSoFar);
    if (sent < 0) {
      std::perror("sendfile");
      return -1;
    }
    sentSoFar += sent;
    if (sentSoFar == fileStat.st_size) {
      return 0;
    }
  }
}

const char *getCppCompiler() noexcept {
  const char *compiler = getenv("CXX");
  if (!compiler) {
    compiler = "/usr/bin/c++";
  }
  return compiler;
}

int run(std::string &executable, char **argv, char **envp) noexcept {
  ::pid_t child;
  if (::posix_spawn(&child, executable.c_str(), nullptr, nullptr, argv, envp) !=
      0) {
    std::perror("posix_spawn");
    return -1;
  }

  int status;
  if (::waitpid(child, &status, 0) == -1) {
    std::perror("waitpid");
    return 1;
  }

  const auto childExited = WIFEXITED(status);
  if (childExited) {
    if (WEXITSTATUS(status) != 0) {
      std::fprintf(stderr, "%s exited with status %d\n", executable.c_str(),
                   WEXITSTATUS(status));
      return -1;
    }
  } else if (WIFSIGNALED(status)) {
    std::fprintf(stderr, "%s killed by signal %d\n", executable.c_str(),
                 WTERMSIG(status));
    return -1;
  }
  return 0;
}

int run(std::string executable, std::vector<std::string> arguments,
        char **envp) noexcept {
  std::vector<char *> argv;
  argv.reserve(1 + arguments.size() + 1);
  argv.push_back(executable.data());
  for (auto &s : arguments) {
    argv.push_back(s.data());
  }
  argv.push_back(nullptr);

  return run(executable, argv.data(), envp);
}
} // namespace

int main(int argc, char **argv, char **envp) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "Usage: %s [path to C++ source file] [runtime arguments to "
                 "the C++ program]\n",
                 argv[0]);
    return EXIT_FAILURE;
  }

  const auto srcPath = argv[1];
  const auto src = ::open(srcPath, O_RDONLY);
  if (src == -1) {
    std::fprintf(stderr, "File with name %s not found\n", srcPath);
    return EXIT_FAILURE;
  }

  SCOPE_EXIT { ::close(src); };

  char dstPath[] = "/tmp/cpprun_tmpSrc_XXXXXX.cpp";
  int dst = ::mkstemps(dstPath, ".cpp"sv.size());
  if (dst == -1) {
    std::perror("mkstemps");
    return EXIT_FAILURE;
  }

  SCOPE_EXIT {
    if (dst != -1) ::close(dst);
    ::unlink(dstPath);
  };

  if (::write(dst, "//", 2) == -1) { // This ignores the first line in dst
    std::perror("write");
    return EXIT_FAILURE;
  }

  if (sendFile(src, dst) != 0) {
    return EXIT_FAILURE;
  }

  ::close(dst);
  dst = -1;

  auto dstCompiled = dstPath + ".out"s;

  if (run(getCppCompiler(), {"-O3"s, dstPath, "-o", dstCompiled}, envp) != 0) {
    return EXIT_FAILURE;
  }

  SCOPE_EXIT { ::unlink(dstCompiled.c_str()); };

  if (run(dstCompiled, argv + 1, envp) != 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
