#include <iostream>
#include <limits>
#include <string>
#include <cctype>
 
class Calculator {
private:
    double num1, num2;
    char operation;
    
    // Function to clear input buffer
    void clearInputBuffer() {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    
    // Function to validate and get a number from user
    bool getNumber(double& number, const std::string& prompt) {
        std::cout << prompt;
        
        if (!(std::cin >> number)) {
            std::cout << "Error: Please enter a valid number.\n";
            clearInputBuffer();
            return false;
        }
        return true;
    }
    
    // Function to validate and get operation from user
    bool getOperation(char& op) {
        std::cout << "Enter operation (+, -, *, /): ";
        std::cin >> op;
        
        // Convert to lowercase for case-insensitive comparison
        op = std::tolower(op);
        
        if (op != '+' && op != '-' && op != '*' && op != '/') {
            std::cout << "Error: Invalid operation. Please use +, -, *, or /\n";
            return false;
        }
        return true;
    }
    
    // Function to perform the calculation
    bool calculate(double& result) {
        switch (operation) {
            case '+':
                result = num1 + num2;
                return true;
            case '-':
                result = num1 - num2;
                return true;
            case '*':
                result = num1 * num2;
                return true;
            case '/':
                if (num2 == 0) {
                    std::cout << "Error: Division by zero is not allowed.\n";
                    return false;
                }
                result = num1 / num2;
                return true;
            default:
                std::cout << "Error: Unknown operation.\n";
                return false;
        }
    }
    
public:
    // Main calculator function
    void run() {
        std::cout << "=== Basic Calculator ===" << std::endl;
        std::cout << "Supports: +, -, *, /" << std::endl;
        std::cout << "Type 'quit' or 'q' to exit\n" << std::endl;
        
        while (true) {
            std::string input;
            std::cout << "Enter 'calc' to start calculation or 'quit' to exit: ";
            std::cin >> input;
            
            // Convert input to lowercase
            for (char& c : input) {
                c = std::tolower(c);
            }
            
            if (input == "quit" || input == "q") {
                std::cout << "Thanks for using the calculator!" << std::endl;
                break;
            }
            
            if (input == "calc" || input == "calculate") {
                performCalculation();
            } else {
                std::cout << "Invalid command. Please enter 'calc' or 'quit'.\n" << std::endl;
            }
        }
    }
    
    // Function to handle a single calculation
    void performCalculation() {
        std::cout << "\n--- New Calculation ---" << std::endl;
        
        // Get first number with validation
        while (!getNumber(num1, "Enter first number: ")) {
            // Keep asking until valid input
        }
        
        // Get operation with validation
        while (!getOperation(operation)) {
            // Keep asking until valid input
        }
        
        // Get second number with validation
        while (!getNumber(num2, "Enter second number: ")) {
            // Keep asking until valid input
        }
        
        // Perform calculation
        double result;
        if (calculate(result)) {
            std::cout << "\nResult: " << num1 << " " << operation << " " << num2 << " = " << result << std::endl;
        }
        
        std::cout << std::endl;
    }
};

// Function to display menu (alternative simple interface)
void simpleCalculator() {
    double num1, num2, result;
    char operation;
    
    std::cout << "=== Simple Calculator Mode ===" << std::endl;
    std::cout << "Enter first number: ";
    std::cin >> num1;
    
    std::cout << "Enter operation (+, -, *, /): ";
    std::cin >> operation;
    
    std::cout << "Enter second number: ";
    std::cin >> num2;
    
    switch (operation) {
        case '+':
            result = num1 + num2;
            std::cout << "Result: " << num1 << " + " << num2 << " = " << result << std::endl;
            break;
        case '-':
            result = num1 - num2;
            std::cout << "Result: " << num1 << " - " << num2 << " = " << result << std::endl;
            break;
        case '*':
            result = num1 * num2;
            std::cout << "Result: " << num1 << " * " << num2 << " = " << result << std::endl;
            break;
        case '/':
            if (num2 != 0) {
                result = num1 / num2;
                std::cout << "Result: " << num1 << " / " << num2 << " = " << result << std::endl;
            } else {
                std::cout << "Error: Division by zero!" << std::endl;
            }
            break;
        default:
            std::cout << "Error: Invalid operation!" << std::endl;
    }
}

int main() {
    int choice;
    
    std::cout << "Choose calculator mode:" << std::endl;
    std::cout << "1. Advanced (with full validation and menu)" << std::endl;
    std::cout << "2. Simple (basic calculation)" << std::endl;
    std::cout << "Enter choice (1 or 2): ";
    
    std::cin >> choice;
    
    if (choice == 1) {
        Calculator calc;
        calc.run();
    } else if (choice == 2) {
        simpleCalculator();
    } else {
        std::cout << "Invalid choice. Starting advanced mode..." << std::endl;
        Calculator calc;
        calc.run();
    }
    
    return 0;

}
