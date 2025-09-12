#include <iostream>
#include <string>
#include <random>
#include <algorithm>
#include <vector>
#include <cctype>

class PasswordGenerator {
private:
    std::mt19937 rng;
    
    // Character sets
    const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string numbers = "0123456789";
    const std::string symbols = "!@#$%^&*()_+-=[]{}|;:,.<>?";
    
    struct PasswordOptions {
        int length = 8;
        bool useLowercase = true;
        bool useUppercase = true;
        bool useNumbers = true;
        bool useSymbols = false;
    };

public:
    PasswordGenerator() : rng(std::random_device{}()) {}

    void displayMenu() {
        std::cout << "\n=== PASSWORD GENERATOR ===" << std::endl;
        std::cout << "1. Generate Password with Current Settings" << std::endl;
        std::cout << "2. Customize Password Settings" << std::endl;
        std::cout << "3. View Current Settings" << std::endl;
        std::cout << "4. Generate Multiple Passwords" << std::endl;
        std::cout << "5. Password Strength Test" << std::endl;
        std::cout << "6. Exit" << std::endl;
        std::cout << "Choose an option (1-6): ";
    }

    int getValidInput(int min, int max, const std::string& prompt) {
        int input;
        while(true) {
            std::cout << prompt;
            std::cin >> input;
            
            if(std::cin.fail() || input < min || input > max) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "Invalid input! Please enter a number between " 
                          << min << " and " << max << "." << std::endl;
                continue;
            }
            
            return input;
        }
    }

    bool getYesNo(const std::string& prompt) {
        char input;
        while(true) {
            std::cout << prompt << " (y/n): ";
            std::cin >> input;
            input = std::tolower(input);
            
            if(input == 'y' || input == 'n') {
                return input == 'y';
            }
            
            std::cout << "Please enter 'y' for yes or 'n' for no." << std::endl;
        }
    }

    PasswordOptions customizeSettings() {
        PasswordOptions options;
        
        std::cout << "\n=== CUSTOMIZE PASSWORD SETTINGS ===" << std::endl;
        
        options.length = getValidInput(1, 128, "Password length (1-128): ");
        
        options.useLowercase = getYesNo("Include lowercase letters (a-z)?");
        options.useUppercase = getYesNo("Include uppercase letters (A-Z)?");
        options.useNumbers = getYesNo("Include numbers (0-9)?");
        options.useSymbols = getYesNo("Include symbols (!@#$%^&*)?");
        
        // Ensure at least one character type is selected
        if(!options.useLowercase && !options.useUppercase && 
           !options.useNumbers && !options.useSymbols) {
            std::cout << "Warning: No character types selected! Enabling lowercase letters." << std::endl;
            options.useLowercase = true;
        }
        
        return options;
    }

    void displaySettings(const PasswordOptions& options) {
        std::cout << "\n=== CURRENT SETTINGS ===" << std::endl;
        std::cout << "Password Length: " << options.length << std::endl;
        std::cout << "Lowercase Letters: " << (options.useLowercase ? "Yes" : "No") << std::endl;
        std::cout << "Uppercase Letters: " << (options.useUppercase ? "Yes" : "No") << std::endl;
        std::cout << "Numbers: " << (options.useNumbers ? "Yes" : "No") << std::endl;
        std::cout << "Symbols: " << (options.useSymbols ? "Yes" : "No") << std::endl;
    }

    std::string generatePassword(const PasswordOptions& options) {
        // Build character pool based on options
        std::string charPool = "";
        
        if(options.useLowercase) charPool += lowercase;
        if(options.useUppercase) charPool += uppercase;
        if(options.useNumbers) charPool += numbers;
        if(options.useSymbols) charPool += symbols;
        
        if(charPool.empty()) {
            return "Error: No character types selected!";
        }
        
        // Generate password
        std::string password = "";
        std::uniform_int_distribution<size_t> dist(0, charPool.length() - 1);
        
        // Ensure at least one character from each selected type
        std::vector<char> requiredChars;
        if(options.useLowercase) {
            std::uniform_int_distribution<size_t> lowDist(0, lowercase.length() - 1);
            requiredChars.push_back(lowercase[lowDist(rng)]);
        }
        if(options.useUppercase) {
            std::uniform_int_distribution<size_t> upDist(0, uppercase.length() - 1);
            requiredChars.push_back(uppercase[upDist(rng)]);
        }
        if(options.useNumbers) {
            std::uniform_int_distribution<size_t> numDist(0, numbers.length() - 1);
            requiredChars.push_back(numbers[numDist(rng)]);
        }
        if(options.useSymbols) {
            std::uniform_int_distribution<size_t> symDist(0, symbols.length() - 1);
            requiredChars.push_back(symbols[symDist(rng)]);
        }
        
        // Add required characters
        for(char c : requiredChars) {
            password += c;
        }
        
        // Fill remaining length with random characters
        for(int i = requiredChars.size(); i < options.length; i++) {
            password += charPool[dist(rng)];
        }
        
        // Shuffle the password to randomize position of required characters
        std::shuffle(password.begin(), password.end(), rng);
        
        return password;
    }

    void generateMultiplePasswords(const PasswordOptions& options) {
        int count = getValidInput(1, 50, "How many passwords to generate (1-50): ");
        
        std::cout << "\n=== GENERATED PASSWORDS ===" << std::endl;
        for(int i = 1; i <= count; i++) {
            std::cout << i << ". " << generatePassword(options) << std::endl;
        }
    }

    int calculatePasswordStrength(const std::string& password) {
        int score = 0;
        bool hasLower = false, hasUpper = false, hasNumber = false, hasSymbol = false;
        
        // Length scoring
        if(password.length() >= 8) score += 25;
        else if(password.length() >= 6) score += 15;
        else score += 5;
        
        if(password.length() >= 12) score += 10;
        if(password.length() >= 16) score += 15;
        
        // Character variety scoring
        for(char c : password) {
            if(std::islower(c) && !hasLower) { hasLower = true; score += 10; }
            else if(std::isupper(c) && !hasUpper) { hasUpper = true; score += 10; }
            else if(std::isdigit(c) && !hasNumber) { hasNumber = true; score += 10; }
            else if(!std::isalnum(c) && !hasSymbol) { hasSymbol = true; score += 20; }
        }
        
        return std::min(score, 100);
    }

    std::string getStrengthLabel(int score) {
        if(score >= 80) return "Very Strong";
        else if(score >= 60) return "Strong";
        else if(score >= 40) return "Moderate";
        else if(score >= 20) return "Weak";
        else return "Very Weak";
    }

    void testPasswordStrength() {
        std::cout << "\nEnter a password to test its strength: ";
        std::string password;
        std::cin.ignore();
        std::getline(std::cin, password);
        
        int strength = calculatePasswordStrength(password);
        std::string label = getStrengthLabel(strength);
        
        std::cout << "\n=== PASSWORD STRENGTH ANALYSIS ===" << std::endl;
        std::cout << "Password: " << std::string(password.length(), '*') << std::endl;
        std::cout << "Length: " << password.length() << " characters" << std::endl;
        std::cout << "Strength Score: " << strength << "/100" << std::endl;
        std::cout << "Strength Level: " << label << std::endl;
        
        // Recommendations
        std::cout << "\nRecommendations:" << std::endl;
        if(password.length() < 12) {
            std::cout << "- Consider using at least 12 characters" << std::endl;
        }
        
        bool hasLower = false, hasUpper = false, hasNumber = false, hasSymbol = false;
        for(char c : password) {
            if(std::islower(c)) hasLower = true;
            else if(std::isupper(c)) hasUpper = true;
            else if(std::isdigit(c)) hasNumber = true;
            else if(!std::isalnum(c)) hasSymbol = true;
        }
        
        if(!hasLower) std::cout << "- Add lowercase letters" << std::endl;
        if(!hasUpper) std::cout << "- Add uppercase letters" << std::endl;
        if(!hasNumber) std::cout << "- Add numbers" << std::endl;
        if(!hasSymbol) std::cout << "- Add symbols for maximum security" << std::endl;
    }

    void run() {
        PasswordOptions currentOptions;
        
        std::cout << "=== WELCOME TO PASSWORD GENERATOR ===" << std::endl;
        std::cout << "Generate secure passwords with customizable options!" << std::endl;
        
        bool running = true;
        while(running) {
            displayMenu();
            int choice = getValidInput(1, 6, "");
            
            switch(choice) {
                case 1: {
                    std::string password = generatePassword(currentOptions);
                    std::cout << "\nGenerated Password: " << password << std::endl;
                    int strength = calculatePasswordStrength(password);
                    std::cout << "Strength: " << getStrengthLabel(strength) 
                              << " (" << strength << "/100)" << std::endl;
                    break;
                }
                case 2:
                    currentOptions = customizeSettings();
                    std::cout << "Settings updated successfully!" << std::endl;
                    break;
                case 3:
                    displaySettings(currentOptions);
                    break;
                case 4:
                    generateMultiplePasswords(currentOptions);
                    break;
                case 5:
                    testPasswordStrength();
                    break;
                case 6:
                    running = false;
                    std::cout << "Thanks for using Password Generator!" << std::endl;
                    break;
            }
        }
    }
};

int main() {
    PasswordGenerator generator;
    generator.run();
    return 0;
}