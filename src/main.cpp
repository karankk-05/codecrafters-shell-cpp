#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(1){
  std::cout << "$ ";
  std::string input;
  std::getline(std::cin, input);
if(input=="exit") return 0;
  std::cout << input << ": command not found" << std::endl;}
  }
