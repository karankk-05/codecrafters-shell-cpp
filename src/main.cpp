#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string findExecutableInPath(const std::string &command) {
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

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
      continue;
    }

    if (input == "exit") {
      return 0;
    }

    if (input.rfind("echo", 0) == 0 && (input.size() == 4 || input[4] == ' ')) {
      std::cout << input.substr(5) << std::endl;
      continue;
    }

    if (input.rfind("type", 0) == 0 && (input.size() == 4 || input[4] == ' ')) {
      std::string command = input.substr(5);
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

    std::string command = input;
    std::string resolved = findExecutableInPath(command);
    if (!resolved.empty()) {
      std::cout << command << " is " << resolved << std::endl;
    } else {
      std::cout << command << ": not found" << std::endl;
    }
  }
}
