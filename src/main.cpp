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
  else if (input.size()>=4&&input.substr(0,4)=="type"){ 
    std::string m=input.substr(5,input.size()-5);
    if(m=="type"||m=="echo"||m=="exit"){
      std::cout<<m<<" is a shell builtin"<<std::endl;
    }
    else{
      std:cout << input << ": command not found" << std::endl;}
    }
  }
  else
  std:cout << input << ": command not found" << std::endl;}

  }
