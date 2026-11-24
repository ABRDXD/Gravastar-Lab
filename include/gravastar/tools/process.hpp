// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cerrno>
#include <fcntl.h>
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
// As run(), with standard output and error written to the given files
// (created or truncated). Used to keep encoder logs and to read probe output.
inline int run_capture(std::vector<std::string> args, const std::string &stdout_path,
                       const std::string &stderr_path) {
  if (args.empty())
    throw std::invalid_argument("empty process argument vector");
  std::vector<char *> pointers;
  for (auto &a : args)
    pointers.push_back(a.data());
  pointers.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions))
    throw std::runtime_error("cannot prepare process file actions");
  const int flags = O_WRONLY | O_CREAT | O_TRUNC;
  int code = posix_spawn_file_actions_addopen(&actions, 1, stdout_path.c_str(), flags, 0644);
  if (!code)
    code = posix_spawn_file_actions_addopen(&actions, 2, stderr_path.c_str(), flags, 0644);
  pid_t pid{};
  if (!code)
    code = posix_spawnp(&pid, pointers[0], &actions, nullptr, pointers.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (code)
    throw std::runtime_error("cannot launch " + args[0] + ": " + std::to_string(code));
  int status{};
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR)
      throw std::runtime_error("waitpid failed");
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
} // namespace gravastar::tools
