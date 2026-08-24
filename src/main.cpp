#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <readline/readline.h>
#include <readline/history.h>
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
  std::vector<std::string> tokens;
  std::string current;
  size_t i = 0;
  while (i < input.size()) {
    char c = input[i];
    if (c == '\'') {
      // Single-quoted string: everything is literal
      i++;
      while (i < input.size() && input[i] != '\'') {
        current += input[i];
        i++;
      }
      if (i < input.size()) i++;
    } else if (c == '"') {
      // Double-quoted string: backslash escapes only " \ $ ` and newline
      i++;
      while (i < input.size() && input[i] != '"') {
        if (input[i] == '\\' && i + 1 < input.size()) {
          char next = input[i + 1];
          if (next == '"' || next == '\\' || next == '$' || next == '`' || next == '\n') {
            current += next;
            i += 2;
            continue;
          }
        }
        current += input[i];
        i++;
      }
      if (i < input.size()) i++;
    } else if (c == ' ' || c == '\t') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      i++;
    } else if (c == '\\') {
      // Backslash outside quotes: next character is literal
      i++;
      if (i < input.size()) {
        current += input[i];
        i++;
      }
    } else {
      current += c;
      i++;
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
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

struct Redirect {
  int fd = -1;          // 1 = stdout, 2 = stderr
  std::string file;
  bool append = false;
};

static void parseRedirects(std::vector<std::string> &tokens, std::vector<Redirect> &redirects) {
  std::vector<std::string> cleaned;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i] == ">" || tokens[i] == "1>") {
      if (i + 1 < tokens.size()) {
        redirects.push_back({1, tokens[i + 1], false});
        i++;
      }
    } else if (tokens[i] == "2>") {
      if (i + 1 < tokens.size()) {
        redirects.push_back({2, tokens[i + 1], false});
        i++;
      }
    } else if (tokens[i] == ">>" || tokens[i] == "1>>") {
      if (i + 1 < tokens.size()) {
        redirects.push_back({1, tokens[i + 1], true});
        i++;
      }
    } else if (tokens[i] == "2>>") {
      if (i + 1 < tokens.size()) {
        redirects.push_back({2, tokens[i + 1], true});
        i++;
      }
    } else {
      cleaned.push_back(tokens[i]);
    }
  }
  tokens = cleaned;
}

static int openRedirect(const Redirect &r) {
  int flags = O_WRONLY | O_CREAT;
  flags |= r.append ? O_APPEND : O_TRUNC;
  return open(r.file.c_str(), flags, 0644);
}

static void runExternalCommand(const std::vector<std::string> &tokens, const std::vector<Redirect> &redirects) {
  std::vector<char *> argv;
  argv.reserve(tokens.size() + 1);
  for (const auto &token : tokens) {
    argv.push_back(const_cast<char *>(token.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid == 0) {
    for (const auto &r : redirects) {
      int fd = openRedirect(r);
      if (fd >= 0) {
        dup2(fd, r.fd);
        close(fd);
      }
    }
    execvp(argv[0], argv.data());
    _exit(127);
  }

  int status = 0;
  waitpid(pid, &status, 0);
}

char *command_generator(const char *text, int state) {
  static size_t list_index;
  static std::vector<std::string> matches;
  
  if (!state) {
    list_index = 0;
    matches.clear();
    std::string prefix(text);
    
    // Builtins
    std::vector<std::string> all_builtins = {"echo", "exit", "pwd", "cd", "type", "complete"};
    for (const auto &b : all_builtins) {
      if (b.compare(0, prefix.size(), prefix) == 0) {
        matches.push_back(b);
      }
    }
    
    // Executables in PATH
    const char *pathEnv = std::getenv("PATH");
    if (pathEnv != nullptr) {
      std::string path = pathEnv;
      size_t start = 0;
      while (start <= path.size()) {
        size_t end = path.find(':', start);
        std::string dir = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        if (!dir.empty()) {
          std::error_code ec;
          if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
            for (const auto &entry : fs::directory_iterator(dir, ec)) {
              std::string filename = entry.path().filename().string();
              if (filename.compare(0, prefix.size(), prefix) == 0) {
                if (fs::is_regular_file(entry.path(), ec)) {
                  if (access(entry.path().c_str(), X_OK) == 0) {
                    matches.push_back(filename);
                  }
                }
              }
            }
          }
        }
        if (end == std::string::npos) break;
        start = end + 1;
      }
    }
    
    // Sort and remove duplicates
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
  }
  
  if (list_index < matches.size()) {
    return strdup(matches[list_index++].c_str());
  }
  return nullptr;
}

char **shell_completion(const char *text, int start, int end) {
  // Only autocomplete command names at the beginning of the line
  if (start == 0) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
  }
  return nullptr;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  rl_attempted_completion_function = shell_completion;

  while (true) {
    char *line = readline("$ ");
    if (line == nullptr) {
      break;
    }
    std::string input(line);
    free(line);
    
    input = trim(input);

    if (input.empty()) {
      continue;
    }

    auto tokens = splitCommand(input);
    if (tokens.empty()) {
      continue;
    }

    // Parse redirects from tokens
    std::vector<Redirect> redirects;
    parseRedirects(tokens, redirects);

    // Setup redirects for builtins
    int saved_stdout = -1, saved_stderr = -1;
    for (const auto &r : redirects) {
      int fd = openRedirect(r);
      if (fd >= 0) {
        if (r.fd == 1) {
          saved_stdout = dup(1);
          dup2(fd, 1);
        } else if (r.fd == 2) {
          saved_stderr = dup(2);
          dup2(fd, 2);
        }
        close(fd);
      }
    }

    auto restoreRedirects = [&]() {
      if (saved_stdout >= 0) {
        dup2(saved_stdout, 1);
        close(saved_stdout);
      }
      if (saved_stderr >= 0) {
        dup2(saved_stderr, 2);
        close(saved_stderr);
      }
    };

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
      restoreRedirects();
      continue;
    }

    if (tokens[0] == "type") {
      std::string command = tokens.size() > 1 ? tokens[1] : "";
      if (command == "type" || command == "echo" || command == "exit" || command == "pwd" || command == "cd" || command == "complete") {
        std::cout << command << " is a shell builtin" << std::endl;
      } else {
        std::string resolved = findExecutableInPath(command);
        if (!resolved.empty()) {
          std::cout << command << " is " << resolved << std::endl;
        } else {
          std::cout << command << ": not found" << std::endl;
        }
      }
      restoreRedirects();
      continue;
    }

    if (tokens[0] == "pwd") {
      std::cout << fs::current_path().string() << std::endl;
      restoreRedirects();
      continue;
    }

    if (tokens[0] == "cd") {
      if (tokens.size() > 1) {
        std::string target = tokens[1];
        if (target == "~") {
          const char *home = std::getenv("HOME");
          if (home) {
            target = home;
          }
        }
        std::error_code ec;
        fs::current_path(target, ec);
        if (ec) {
          std::cout << "cd: " << tokens[1] << ": No such file or directory" << std::endl;
        }
      }
      restoreRedirects();
      continue;
    }

    if (tokens[0] == "complete") {
      // Placeholder for programmable completions: do nothing for now
      restoreRedirects();
      continue;
    }

    // Restore redirects before running external command (it handles its own)
    restoreRedirects();

    std::string resolved = findExecutableInPath(tokens[0]);
    if (!resolved.empty()) {
      runExternalCommand(tokens, redirects);
    } else {
      std::cout << tokens[0] << ": not found" << std::endl;
    }
  }
}
