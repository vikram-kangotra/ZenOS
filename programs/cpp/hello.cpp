void strcpy(char* dst, const char* src) {
    while (*src != '\0') {
        *dst = *src;
        src++;
    }
}

// Function with a buffer overflow vulnerability
void vulnerable_function(const char* input) {
    char buffer[8];  // Small buffer
    strcpy(buffer, input);  // No bounds checking - vulnerable!
}

// Function to demonstrate stack smashing
void stack_smash_demo() {
    // Normal usage
    vulnerable_function("short");
    
    // Stack smashing attempt
    const char* long_string = "This is a very long string that will overflow the buffer and smash the stack!";
    vulnerable_function(long_string);
}

// Main function
int main() {
    stack_smash_demo();
    return 0;
} 