#include <iostream>
#include <random>
#include <limits>
#include <string>
 
class NumberGuessingGame {
private:
    int secretNumber;
    int attempts;
    int minRange;
    int maxRange;
    std::mt19937 rng;
    
public:
    NumberGuessingGame(int min = 1, int max = 100) 
        : minRange(min), maxRange(max), attempts(0), rng(std::random_device{}()) {
        generateNewNumber();
    }
    
    void generateNewNumber() {
        std::uniform_int_distribution<int> dist(minRange, maxRange);
        secretNumber = dist(rng);
        attempts = 0;
    }
    
    void displayWelcome() {
        std::cout << "\n=== NUMBER GUESSING GAME ===" << std::endl;
        std::cout << "I'm thinking of a number between " << minRange 
                  << " and " << maxRange << std::endl;
        std::cout << "Can you guess what it is?" << std::endl;
        std::cout << "Type 'quit' to exit the game." << std::endl;
        std::cout << "===========================\n" << std::endl;
    }
    
    int getValidInput() {
        std::string input;
        int guess;
        
        while (true) {
            std::cout << "Enter your guess: ";
            std::cin >> input;
            
            // Check if user wants to quit
            if (input == "quit" || input == "q" || input == "exit") {
                return -1; // Special value to indicate quit
            }
            
            // Try to convert to integer
            try {
                guess = std::stoi(input);
                
                // Check if guess is in valid range
                if (guess < minRange || guess > maxRange) {
                    std::cout << "Please enter a number between " << minRange 
                              << " and " << maxRange << "!" << std::endl;
                    continue;
                }
                
                return guess;
            }
            catch (const std::exception&) {
                std::cout << "Invalid input! Please enter a valid number or 'quit'." << std::endl;
                continue;
            }
        }
    }
    
    bool makeGuess(int guess) {
        attempts++;
        
        if (guess == secretNumber) {
            std::cout << "\n🎉 Congratulations! You guessed it!" << std::endl;
            std::cout << "The number was " << secretNumber << std::endl;
            std::cout << "It took you " << attempts << " attempt" 
                      << (attempts == 1 ? "" : "s") << "!" << std::endl;
            return true;
        }
        else if (guess < secretNumber) {
            std::cout << "Too low! Try a higher number." << std::endl;
        }
        else {
            std::cout << "Too high! Try a lower number." << std::endl;
        }
        
        // Give additional hints based on how close they are
        int difference = abs(guess - secretNumber);
        if (difference <= 5) {
            std::cout << "🔥 You're very close!" << std::endl;
        }
        else if (difference <= 10) {
            std::cout << "🔥 Getting warmer!" << std::endl;
        }
        
        return false;
    }
    
    void displayStats() {
        std::cout << "Attempts so far: " << attempts << std::endl;
    }
    
    bool playAgain() {
        std::string response;
        std::cout << "\nWould you like to play again? (y/n): ";
        std::cin >> response;
        
        return (response == "y" || response == "yes" || response == "Y" || response == "YES");
    }
    
    void setDifficulty() {
        std::cout << "\nChoose difficulty level:" << std::endl;
        std::cout << "1. Easy (1-50)" << std::endl;
        std::cout << "2. Medium (1-100)" << std::endl;
        std::cout << "3. Hard (1-200)" << std::endl;
        std::cout << "4. Expert (1-500)" << std::endl;
        
        int choice;
        std::cout << "Enter your choice (1-4): ";
        std::cin >> choice;
        
        switch (choice) {
            case 1:
                minRange = 1; maxRange = 50;
                break;
            case 2:
                minRange = 1; maxRange = 100;
                break;
            case 3:
                minRange = 1; maxRange = 200;
                break;
            case 4:
                minRange = 1; maxRange = 500;
                break;
            default:
                std::cout << "Invalid choice. Using Medium difficulty." << std::endl;
                minRange = 1; maxRange = 100;
        }
        
        generateNewNumber();
    }
};

int main() {
    NumberGuessingGame game;
    
    std::cout << "Welcome to the Number Guessing Game!" << std::endl;
    
    do {
        // Set difficulty for each new game
        game.setDifficulty();
        game.displayWelcome();
        
        bool gameWon = false;
        
        // Main game loop
        while (!gameWon) {
            int guess = game.getValidInput();
            
            // Check if player wants to quit
            if (guess == -1) {
                std::cout << "Thanks for playing! Goodbye!" << std::endl;
                return 0;
            }
            
            gameWon = game.makeGuess(guess);
            
            if (!gameWon) {
                game.displayStats();
                std::cout << std::endl;
            }
        }
        
    } while (game.playAgain());
    
    std::cout << "Thanks for playing! See you next time!" << std::endl;
    return 0;

}
