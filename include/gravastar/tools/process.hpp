// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cerrno>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <vector>
extern char **environ;
namespace gravastar::tools {
inline int run(std::vector<std::string> args) {
  if (args.empty())
    throw std::invalid_argument("empty process argument vector");
  std::vector<char *> pointers;
  for (auto &a : args)
    pointers.push_back(a.data());
  pointers.push_back(nullptr);
  pid_t pid{};
  int code = posix_spawnp(&pid, pointers[0], nullptr, nullptr, pointers.data(),
                          environ);
  if (code)
    throw std::runtime_error("cannot launch " + args[0] + ": " +
                             std::to_string(code));
  int status{};
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR)
      throw std::runtime_error("waitpid failed");
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
} // namespace gravastar::tools
