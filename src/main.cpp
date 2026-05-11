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
  else if (input.size()>=4&&input.substr(0,4)=="echo") {
    std::cout<<input.substr(5,input.size()-5)<<std::endl;
  }
  //std::cout << input << ": command not found" << std::endl;}
}
  }
