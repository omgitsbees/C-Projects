#include <iostream>
#include <random>
#include <string>
#include <cctype>

class RockPaperScissors {
private:
    int playerScore;
    int computerScore;
    int rounds;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;

public:
    RockPaperScissors() : playerScore(0), computerScore(0), rounds(0), 
                         rng(std::random_device{}()), dist(1, 3) {}

    void displayMenu() {
        std::cout << "\n=== ROCK PAPER SCISSORS ===" << std::endl;
        std::cout << "Choose your move:" << std::endl;
        std::cout << "1. Rock" << std::endl;
        std::cout << "2. Paper" << std::endl;
        std::cout << "3. Scissors" << std::endl;
        std::cout << "4. View Score" << std::endl;
        std::cout << "5. Quit Game" << std::endl;
        std::cout << "Enter your choice (1-5): ";
    }

    std::string getMoveName(int move) {
        switch(move) {
            case 1: return "Rock";
            case 2: return "Paper";
            case 3: return "Scissors";
            default: return "Invalid";
        }
    }

    int getPlayerMove() {
        int choice;
        while(true) {
            displayMenu();
            std::cin >> choice;
            
            if(std::cin.fail() || choice < 1 || choice > 5) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "Invalid input! Please enter a number between 1-5." << std::endl;
                continue;
            }
            
            return choice;
        }
    }

    int getComputerMove() {
        return dist(rng);
    }

    std::string determineWinner(int playerMove, int computerMove) {
        if(playerMove == computerMove) {
            return "tie";
        }
        
        // Check winning conditions for player
        if((playerMove == 1 && computerMove == 3) ||  // Rock beats Scissors
           (playerMove == 2 && computerMove == 1) ||  // Paper beats Rock
           (playerMove == 3 && computerMove == 2)) {  // Scissors beats Paper
            playerScore++;
            return "player";
        } else {
            computerScore++;
            return "computer";
        }
    }

    void displayRoundResult(int playerMove, int computerMove, std::string winner) {
        std::cout << "\n--- Round " << rounds << " Results ---" << std::endl;
        std::cout << "You played: " << getMoveName(playerMove) << std::endl;
        std::cout << "Computer played: " << getMoveName(computerMove) << std::endl;
        
        if(winner == "tie") {
            std::cout << "It's a tie!" << std::endl;
        } else if(winner == "player") {
            std::cout << "You win this round!" << std::endl;
        } else {
            std::cout << "Computer wins this round!" << std::endl;
        }
        
        std::cout << "Current Score - You: " << playerScore << " | Computer: " << computerScore << std::endl;
    }

    void displayScore() {
        std::cout << "\n=== CURRENT SCORE ===" << std::endl;
        std::cout << "Rounds played: " << rounds << std::endl;
        std::cout << "Your wins: " << playerScore << std::endl;
        std::cout << "Computer wins: " << computerScore << std::endl;
        std::cout << "Ties: " << (rounds - playerScore - computerScore) << std::endl;
        
        if(playerScore > computerScore) {
            std::cout << "You're currently winning!" << std::endl;
        } else if(computerScore > playerScore) {
            std::cout << "Computer is currently winning!" << std::endl;
        } else {
            std::cout << "It's currently tied!" << std::endl;
        }
    }

    void displayFinalScore() {
        std::cout << "\n=== FINAL GAME RESULTS ===" << std::endl;
        std::cout << "Total rounds played: " << rounds << std::endl;
        std::cout << "Your total wins: " << playerScore << std::endl;
        std::cout << "Computer total wins: " << computerScore << std::endl;
        std::cout << "Total ties: " << (rounds - playerScore - computerScore) << std::endl;
        
        if(playerScore > computerScore) {
            std::cout << "🎉 Congratulations! You won overall!" << std::endl;
        } else if(computerScore > playerScore) {
            std::cout << "Computer won overall. Better luck next time!" << std::endl;
        } else {
            std::cout << "The game ended in a tie! Great match!" << std::endl;
        }
    }

    bool playAgain() {
        char choice;
        std::cout << "\nPlay another round? (y/n): ";
        std::cin >> choice;
        return (std::tolower(choice) == 'y');
    }

    void playGame() {
        std::cout << "Welcome to Rock Paper Scissors!" << std::endl;
        std::cout << "Play as many rounds as you want, then quit to see final results." << std::endl;
        
        bool gameRunning = true;
        
        while(gameRunning) {
            int playerChoice = getPlayerMove();
            
            switch(playerChoice) {
                case 1:
                case 2:
                case 3: {
                    rounds++;
                    int computerMove = getComputerMove();
                    std::string winner = determineWinner(playerChoice, computerMove);
                    displayRoundResult(playerChoice, computerMove, winner);
                    break;
                }
                case 4:
                    displayScore();
                    break;
                case 5:
                    gameRunning = false;
                    break;
            }
        }
        
        if(rounds > 0) {
            displayFinalScore();
        }
        
        std::cout << "Thanks for playing!" << std::endl;
    }
};

int main() {
    RockPaperScissors game;
    game.playGame();
    return 0;
}