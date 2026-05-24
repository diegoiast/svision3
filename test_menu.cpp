#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

std::string get_printable_shortcut(std::string_view s) {
    std::string result;
    // Replace "Std" with Ctrl or Cmd depending on platform
#ifdef __APPLE__
    std::string accel = "Cmd+";
#else
    std::string accel = "Ctrl+";
#endif
    
    std::string input(s);
    size_t pos = input.find("Std+");
    if (pos != std::string::npos) {
        input.replace(pos, 4, accel);
    }
    return input;
}

int main() {
    std::cout << get_printable_shortcut("Std+Z") << std::endl;
    return 0;
}
