#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static std::string trim(const std::string &s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

static std::vector<std::string> splitCommand(const std::string &input) {
  std::istringstream stream(input);
  std::vector<std::string> tokens;
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

static std::string findExecutableInPath(const std::string &command) {
  if (command.find('/') != std::string::npos) {
    if (fs::exists(command) && fs::is_regular_file(command) && access(command.c_str(), X_OK) == 0) {
      return command;
    }
    return "";
  }

  const char *pathEnv = std::getenv("PATH");
  if (pathEnv == nullptr) {
    return "";
  }

  std::string path = pathEnv;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find(':', start);
    std::string dir = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);

    if (!dir.empty()) {
      fs::path candidate = fs::path(dir) / command;
      std::error_code ec;
      if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
        if (access(candidate.c_str(), X_OK) == 0) {
          return candidate.string();
        }
      }
    }

    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }

  return "";
}

static void runExternalCommand(const std::vector<std::string> &tokens) {
  std::vector<char *> argv;
  argv.reserve(tokens.size() + 1);
  for (const auto &token : tokens) {
    argv.push_back(const_cast<char *>(token.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    execvp(argv[0], argv.data());
    _exit(127);
  }

  int status = 0;
  waitpid(pid, &status, 0);
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);
    input = trim(input);

    if (input.empty()) {
      continue;
    }

    auto tokens = splitCommand(input);
    if (tokens.empty()) {
      continue;
    }

    if (tokens[0] == "exit") {
      return 0;
    }

    if (tokens[0] == "echo") {
      std::string output;
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) {
          output += " ";
        }
        output += tokens[i];
      }
      std::cout << output << std::endl;
      continue;
    }

    if (tokens[0] == "type") {
      std::string command = tokens.size() > 1 ? tokens[1] : "";
      if (command == "type" || command == "echo" || command == "exit") {
        std::cout << command << " is a shell builtin" << std::endl;
      } else {
        std::string resolved = findExecutableInPath(command);
        if (!resolved.empty()) {
          std::cout << command << " is " << resolved << std::endl;
        } else {
          std::cout << command << ": not found" << std::endl;
        }
      }
      continue;
    }

    std::string resolved = findExecutableInPath(tokens[0]);
    if (!resolved.empty()) {
      runExternalCommand(tokens);
    } else {
      std::cout << tokens[0] << ": not found" << std::endl;
    }
  }
}
