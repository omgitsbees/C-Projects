#include <iostream>
#include <iomanip>
#include <limits>
#include <string>

class TemperatureConverter {
private:
    // Conversion functions
    static double celsiusToFahrenheit(double celsius) {
        return (celsius * 9.0 / 5.0) + 32.0;
    }
    
    static double celsiusToKelvin(double celsius) {
        return celsius + 273.15;
    }
    
    static double fahrenheitToCelsius(double fahrenheit) {
        return (fahrenheit - 32.0) * 5.0 / 9.0;
    }
    
    static double fahrenheitToKelvin(double fahrenheit) {
        return celsiusToKelvin(fahrenheitToCelsius(fahrenheit));
    }
    
    static double kelvinToCelsius(double kelvin) {
        return kelvin - 273.15;
    }
    
    static double kelvinToFahrenheit(double kelvin) {
        return celsiusToFahrenheit(kelvinToCelsius(kelvin));
    }
    
    // Input validation
    static bool isValidTemperature(double temp, char unit) {
        switch(unit) {
            case 'C': case 'c':
                return temp >= -273.15; // Absolute zero in Celsius
            case 'F': case 'f':
                return temp >= -459.67; // Absolute zero in Fahrenheit
            case 'K': case 'k':
                return temp >= 0.0;     // Absolute zero in Kelvin
            default:
                return false;
        }
    }
    
    static void clearInputBuffer() {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

public:
    static void displayMenu() {
        std::cout << "\n╔══════════════════════════════════════╗\n";
        std::cout << "║        Temperature Converter         ║\n";
        std::cout << "╠══════════════════════════════════════╣\n";
        std::cout << "║ 1. Celsius to Fahrenheit             ║\n";
        std::cout << "║ 2. Celsius to Kelvin                 ║\n";
        std::cout << "║ 3. Fahrenheit to Celsius             ║\n";
        std::cout << "║ 4. Fahrenheit to Kelvin              ║\n";
        std::cout << "║ 5. Kelvin to Celsius                 ║\n";
        std::cout << "║ 6. Kelvin to Fahrenheit              ║\n";
        std::cout << "║ 7. Convert All (from one unit)       ║\n";
        std::cout << "║ 8. Exit                              ║\n";
        std::cout << "╚══════════════════════════════════════╝\n";
        std::cout << "Enter your choice (1-8): ";
    }
    
    static double getTemperatureInput(const std::string& unit) {
        double temp;
        while (true) {
            std::cout << "Enter temperature in " << unit << ": ";
            if (std::cin >> temp) {
                return temp;
            } else {
                std::cout << "Invalid input! Please enter a numeric value.\n";
                clearInputBuffer();
            }
        }
    }
    
    static void performConversion(int choice) {
        double temp, result;
        
        switch (choice) {
            case 1: {
                temp = getTemperatureInput("Celsius");
                if (!isValidTemperature(temp, 'C')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-273.15°C)\n";
                    return;
                }
                result = celsiusToFahrenheit(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << "°C = " << result << "°F\n";
                break;
            }
            case 2: {
                temp = getTemperatureInput("Celsius");
                if (!isValidTemperature(temp, 'C')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-273.15°C)\n";
                    return;
                }
                result = celsiusToKelvin(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << "°C = " << result << " K\n";
                break;
            }
            case 3: {
                temp = getTemperatureInput("Fahrenheit");
                if (!isValidTemperature(temp, 'F')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-459.67°F)\n";
                    return;
                }
                result = fahrenheitToCelsius(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << "°F = " << result << "°C\n";
                break;
            }
            case 4: {
                temp = getTemperatureInput("Fahrenheit");
                if (!isValidTemperature(temp, 'F')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-459.67°F)\n";
                    return;
                }
                result = fahrenheitToKelvin(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << "°F = " << result << " K\n";
                break;
            }
            case 5: {
                temp = getTemperatureInput("Kelvin");
                if (!isValidTemperature(temp, 'K')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (0 K)\n";
                    return;
                }
                result = kelvinToCelsius(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << " K = " << result << "°C\n";
                break;
            }
            case 6: {
                temp = getTemperatureInput("Kelvin");
                if (!isValidTemperature(temp, 'K')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (0 K)\n";
                    return;
                }
                result = kelvinToFahrenheit(temp);
                std::cout << std::fixed << std::setprecision(2);
                std::cout << temp << " K = " << result << "°F\n";
                break;
            }
            case 7: {
                convertAll();
                break;
            }
            default:
                std::cout << "Invalid choice! Please try again.\n";
        }
    }
    
    static void convertAll() {
        char unit;
        double temp;
        
        std::cout << "Enter the unit you want to convert FROM (C/F/K): ";
        std::cin >> unit;
        
        switch (unit) {
            case 'C': case 'c': {
                temp = getTemperatureInput("Celsius");
                if (!isValidTemperature(temp, 'C')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-273.15°C)\n";
                    return;
                }
                std::cout << std::fixed << std::setprecision(2);
                std::cout << "\n--- Conversion Results ---\n";
                std::cout << "Original: " << temp << "°C\n";
                std::cout << "To Fahrenheit: " << celsiusToFahrenheit(temp) << "°F\n";
                std::cout << "To Kelvin: " << celsiusToKelvin(temp) << " K\n";
                break;
            }
            case 'F': case 'f': {
                temp = getTemperatureInput("Fahrenheit");
                if (!isValidTemperature(temp, 'F')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (-459.67°F)\n";
                    return;
                }
                std::cout << std::fixed << std::setprecision(2);
                std::cout << "\n--- Conversion Results ---\n";
                std::cout << "Original: " << temp << "°F\n";
                std::cout << "To Celsius: " << fahrenheitToCelsius(temp) << "°C\n";
                std::cout << "To Kelvin: " << fahrenheitToKelvin(temp) << " K\n";
                break;
            }
            case 'K': case 'k': {
                temp = getTemperatureInput("Kelvin");
                if (!isValidTemperature(temp, 'K')) {
                    std::cout << "Invalid temperature! Cannot be below absolute zero (0 K)\n";
                    return;
                }
                std::cout << std::fixed << std::setprecision(2);
                std::cout << "\n--- Conversion Results ---\n";
                std::cout << "Original: " << temp << " K\n";
                std::cout << "To Celsius: " << kelvinToCelsius(temp) << "°C\n";
                std::cout << "To Fahrenheit: " << kelvinToFahrenheit(temp) << "°F\n";
                break;
            }
            default:
                std::cout << "Invalid unit! Please enter C, F, or K.\n";
        }
    }
    
    static int getMenuChoice() {
        int choice;
        while (true) {
            if (std::cin >> choice && choice >= 1 && choice <= 8) {
                return choice;
            } else {
                std::cout << "Invalid input! Please enter a number between 1 and 8: ";
                clearInputBuffer();
            }
        }
    }
};

int main() {
    std::cout << "Welcome to the Temperature Converter!\n";
    
    int choice;
    do {
        TemperatureConverter::displayMenu();
        choice = TemperatureConverter::getMenuChoice();
        
        if (choice != 8) {
            TemperatureConverter::performConversion(choice);
            
            // Ask if user wants to continue
            char continueChoice;
            std::cout << "\nPress Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();
        }
        
    } while (choice != 8);
    
    std::cout << "\nThank you for using the Temperature Converter! Goodbye!\n";
    
    return 0;
}
