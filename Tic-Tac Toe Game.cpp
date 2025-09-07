#include <iostream>
#include <vector>
#include <limits>

class TicTacToe {
private:
    std::vector<std::vector<char>> board;
    char currentPlayer;
    int gameCount;
    int playerXWins;
    int playerOWins;
    int draws;

public:
    TicTacToe() : board(3, std::vector<char>(3, ' ')), currentPlayer('X'), 
                  gameCount(0), playerXWins(0), playerOWins(0), draws(0) {}

    void displayBoard() {
        std::cout << "\n   1   2   3\n";
        for (int i = 0; i < 3; i++) {
            std::cout << i + 1 << "  " << board[i][0] << " | " << board[i][1] 
                      << " | " << board[i][2] << "\n";
            if (i < 2) {
                std::cout << "  -----------\n";
            }
        }
        std::cout << "\n";
    }

    void displayInstructions() {
        std::cout << "=================================\n";
        std::cout << "    Welcome to Tic-Tac-Toe!     \n";
        std::cout << "=================================\n";
        std::cout << "Enter your move as row and column (1-3).\n";
        std::cout << "Example: '2 1' places your mark at row 2, column 1.\n";
        std::cout << "Player X goes first!\n\n";
    }

    bool isValidMove(int row, int col) {
        return (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ');
    }

    bool makeMove(int row, int col) {
        if (!isValidMove(row, col)) {
            return false;
        }
        board[row][col] = currentPlayer;
        return true;
    }

    bool checkWin() {
        // Check rows
        for (int i = 0; i < 3; i++) {
            if (board[i][0] != ' ' && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
                return true;
            }
        }

        // Check columns
        for (int j = 0; j < 3; j++) {
            if (board[0][j] != ' ' && board[0][j] == board[1][j] && board[1][j] == board[2][j]) {
                return true;
            }
        }

        // Check diagonals
        if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
            return true;
        }
        if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
            return true;
        }

        return false;
    }

    bool isBoardFull() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (board[i][j] == ' ') {
                    return false;
                }
            }
        }
        return true;
    }

    void switchPlayer() {
        currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
    }

    void resetBoard() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                board[i][j] = ' ';
            }
        }
        currentPlayer = 'X';
        gameCount++;
    }

    void displayStats() {
        std::cout << "\n=== GAME STATISTICS ===\n";
        std::cout << "Games played: " << gameCount << "\n";
        std::cout << "Player X wins: " << playerXWins << "\n";
        std::cout << "Player O wins: " << playerOWins << "\n";
        std::cout << "Draws: " << draws << "\n";
        std::cout << "======================\n\n";
    }

    int getInput(const std::string& prompt) {
        int value;
        while (true) {
            std::cout << prompt;
            if (std::cin >> value) {
                return value;
            } else {
                std::cout << "Invalid input! Please enter a number.\n";
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }
    }

    bool askPlayAgain() {
        char choice;
        std::cout << "Play another game? (y/n): ";
        std::cin >> choice;
        return (choice == 'y' || choice == 'Y');
    }

    void playGame() {
        displayInstructions();

        do {
            resetBoard();
            bool gameWon = false;
            bool gameDraw = false;

            while (!gameWon && !gameDraw) {
                displayBoard();
                std::cout << "Player " << currentPlayer << "'s turn\n";

                int row = getInput("Enter row (1-3): ");
                int col = getInput("Enter column (1-3): ");

                // Convert to 0-based indexing
                row--;
                col--;

                if (makeMove(row, col)) {
                    if (checkWin()) {
                        displayBoard();
                        std::cout << "🎉 Player " << currentPlayer << " wins! 🎉\n";
                        if (currentPlayer == 'X') {
                            playerXWins++;
                        } else {
                            playerOWins++;
                        }
                        gameWon = true;
                    } else if (isBoardFull()) {
                        displayBoard();
                        std::cout << "It's a draw! 🤝\n";
                        draws++;
                        gameDraw = true;
                    } else {
                        switchPlayer();
                    }
                } else {
                    std::cout << "Invalid move! That position is already taken or out of bounds.\n";
                    std::cout << "Please try again.\n\n";
                }
            }

            displayStats();

        } while (askPlayAgain());

        std::cout << "\nThanks for playing Tic-Tac-Toe! Goodbye! 👋\n";
    }
};

int main() {
    TicTacToe game;
    game.playGame();
    return 0;
}