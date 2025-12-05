#include <iostream>
#include <vector>
#include <conio.h>  // For _kbhit() and _getch() on Windows
#include <windows.h>  // For Windows console functions
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>

// For cross-platform compatibility, you might need to adjust these includes:
// Linux/Mac: Use termios.h, unistd.h, and implement kbhit() differently

enum Direction {
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4
};

struct Position {
    int x, y;
    
    Position() : x(0), y(0) {}
    Position(int x, int y) : x(x), y(y) {}
    
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
};

class Snake {
private:
    std::vector<Position> body;
    Direction direction;
    bool growing;
    
public:
    Snake(int startX, int startY) {
        body.push_back(Position(startX, startY));
        body.push_back(Position(startX, startY + 1));
        body.push_back(Position(startX, startY + 2));
        direction = UP;
        growing = false;
    }
    
    void setDirection(Direction newDir) {
        // Prevent snake from going back into itself
        if ((direction == UP && newDir == DOWN) ||
            (direction == DOWN && newDir == UP) ||
            (direction == LEFT && newDir == RIGHT) ||
            (direction == RIGHT && newDir == LEFT)) {
            return;
        }
        direction = newDir;
    }
    
    void move() {
        Position head = body[0];
        
        // Calculate new head position based on direction
        switch (direction) {
            case UP: head.y--; break;
            case DOWN: head.y++; break;
            case LEFT: head.x--; break;
            case RIGHT: head.x++; break;
        }
        
        // Add new head
        body.insert(body.begin(), head);
        
        // Remove tail unless growing
        if (!growing) {
            body.pop_back();
        } else {
            growing = false;
        }
    }
    
    void grow() {
        growing = true;
    }
    
    Position getHead() const {
        return body[0];
    }
    
    const std::vector<Position>& getBody() const {
        return body;
    }
    
    bool checkSelfCollision() const {
        const Position& head = body[0];
        for (size_t i = 1; i < body.size(); i++) {
            if (head == body[i]) {
                return true;
            }
        }
        return false;
    }
    
    int getLength() const {
        return body.size();
    }
};

class Game {
private:
    static const int BOARD_WIDTH = 50;
    static const int BOARD_HEIGHT = 20;
    
    Snake snake;
    Position food;
    int score;
    int level;
    int speed;  // Milliseconds between moves
    bool gameRunning;
    bool gameOver;
    int highScore;
    
    // Game board for display
    char board[BOARD_HEIGHT][BOARD_WIDTH];
    
public:
    Game() : snake(BOARD_WIDTH / 2, BOARD_HEIGHT / 2), score(0), level(1), 
             speed(200), gameRunning(true), gameOver(false), highScore(0) {
        srand(static_cast<unsigned>(time(nullptr)));
        loadHighScore();
        generateFood();
        initializeBoard();
    }
    
    void initializeBoard() {
        // Clear board
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < BOARD_WIDTH; j++) {
                board[i][j] = ' ';
            }
        }
        
        // Draw borders
        for (int i = 0; i < BOARD_WIDTH; i++) {
            board[0][i] = '#';
            board[BOARD_HEIGHT - 1][i] = '#';
        }
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            board[i][0] = '#';
            board[i][BOARD_WIDTH - 1] = '#';
        }
    }
    
    void generateFood() {
        do {
            food.x = 1 + rand() % (BOARD_WIDTH - 2);
            food.y = 1 + rand() % (BOARD_HEIGHT - 2);
        } while (isFoodOnSnake());
    }
    
    bool isFoodOnSnake() const {
        const std::vector<Position>& snakeBody = snake.getBody();
        for (const Position& segment : snakeBody) {
            if (food == segment) {
                return true;
            }
        }
        return false;
    }
    
    void updateBoard() {
        initializeBoard();
        
        // Place snake on board
        const std::vector<Position>& snakeBody = snake.getBody();
        for (size_t i = 0; i < snakeBody.size(); i++) {
            const Position& segment = snakeBody[i];
            if (i == 0) {
                board[segment.y][segment.x] = 'O';  // Head
            } else {
                board[segment.y][segment.x] = 'o';  // Body
            }
        }
        
        // Place food
        board[food.y][food.x] = '*';
    }
    
    void displayBoard() const {
        // Clear screen (Windows specific)
        system("cls");
        
        // Display game info
        std::cout << "SNAKE GAME - Score: " << score 
                  << " | Level: " << level 
                  << " | Length: " << snake.getLength()
                  << " | High Score: " << highScore << std::endl;
        std::cout << "Use WASD or Arrow Keys to move. ESC to quit." << std::endl;
        std::cout << std::endl;
        
        // Display board
        for (int i = 0; i < BOARD_HEIGHT; i++) {
            for (int j = 0; j < BOARD_WIDTH; j++) {
                std::cout << board[i][j];
            }
            std::cout << std::endl;
        }
        
        // Display level progression info
        int nextLevelScore = level * 100;
        std::cout << std::endl;
        std::cout << "Next Level at: " << nextLevelScore << " points" << std::endl;
        std::cout << "Current Speed: Level " << level << std::endl;
        
        if (gameOver) {
            std::cout << std::endl;
            std::cout << "GAME OVER!" << std::endl;
            std::cout << "Final Score: " << score << std::endl;
            if (score > highScore) {
                std::cout << "NEW HIGH SCORE!" << std::endl;
            }
            std::cout << "Press R to restart or ESC to quit." << std::endl;
        }
    }
    
    void handleInput() {
        if (_kbhit()) {
            char key = _getch();
            
            // Convert to uppercase for consistency
            key = toupper(key);
            
            switch (key) {
                case 'W':
                case 72: // Up arrow key code
                    snake.setDirection(UP);
                    break;
                case 'S':
                case 80: // Down arrow key code
                    snake.setDirection(DOWN);
                    break;
                case 'A':
                case 75: // Left arrow key code
                    snake.setDirection(LEFT);
                    break;
                case 'D':
                case 77: // Right arrow key code
                    snake.setDirection(RIGHT);
                    break;
                case 27: // ESC key
                    gameRunning = false;
                    break;
                case 'R':
                    if (gameOver) {
                        restartGame();
                    }
                    break;
                case 'P': // Pause functionality
                    pause();
                    break;
            }
        }
    }
    
    void pause() {
        std::cout << "\nGAME PAUSED - Press any key to continue..." << std::endl;
        _getch();
    }
    
    bool checkCollisions() {
        Position head = snake.getHead();
        
        // Check wall collision
        if (head.x <= 0 || head.x >= BOARD_WIDTH - 1 || 
            head.y <= 0 || head.y >= BOARD_HEIGHT - 1) {
            return true;
        }
        
        // Check self collision
        return snake.checkSelfCollision();
    }
    
    void checkFoodCollision() {
        if (snake.getHead() == food) {
            snake.grow();
            score += 10 * level;  // More points for higher levels
            
            // Check for level up
            if (score >= level * 100) {
                levelUp();
            }
            
            generateFood();
        }
    }
    
    void levelUp() {
        level++;
        speed = std::max(50, speed - 20);  // Increase speed, minimum 50ms
        
        // Display level up message
        std::cout << "\nLEVEL UP! Now at Level " << level << std::endl;
        std::cout << "Speed increased! Press any key to continue..." << std::endl;
        _getch();
    }
    
    void gameOverSequence() {
        gameOver = true;
        
        // Update high score
        if (score > highScore) {
            highScore = score;
            saveHighScore();
        }
        
        displayBoard();
        
        // Wait for restart or quit
        while (gameOver && gameRunning) {
            handleInput();
            Sleep(50);
        }
    }
    
    void restartGame() {
        // Reset game state
        snake = Snake(BOARD_WIDTH / 2, BOARD_HEIGHT / 2);
        score = 0;
        level = 1;
        speed = 200;
        gameOver = false;
        generateFood();
        initializeBoard();
    }
    
    void saveHighScore() const {
        std::ofstream file("snake_highscore.txt");
        if (file.is_open()) {
            file << highScore;
            file.close();
        }
    }
    
    void loadHighScore() {
        std::ifstream file("snake_highscore.txt");
        if (file.is_open()) {
            file >> highScore;
            file.close();
        }
    }
    
    void showStartScreen() {
        system("cls");
        std::cout << "========================================" << std::endl;
        std::cout << "           CONSOLE SNAKE GAME          " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "HOW TO PLAY:" << std::endl;
        std::cout << "- Use WASD or Arrow Keys to move" << std::endl;
        std::cout << "- Eat food (*) to grow and gain points" << std::endl;
        std::cout << "- Avoid hitting walls and yourself" << std::endl;
        std::cout << "- Every 100 points = level up + speed increase" << std::endl;
        std::cout << "- Press P to pause, ESC to quit" << std::endl;
        std::cout << std::endl;
        std::cout << "SCORING:" << std::endl;
        std::cout << "- Food: 10 x Level points" << std::endl;
        std::cout << "- Level 1: 10 points per food" << std::endl;
        std::cout << "- Level 2: 20 points per food" << std::endl;
        std::cout << "- And so on..." << std::endl;
        std::cout << std::endl;
        std::cout << "Current High Score: " << highScore << std::endl;
        std::cout << std::endl;
        std::cout << "Press any key to start the game!" << std::endl;
        
        _getch();
    }
    
    void run() {
        showStartScreen();
        
        while (gameRunning) {
            if (!gameOver) {
                handleInput();
                snake.move();
                
                if (checkCollisions()) {
                    gameOverSequence();
                    continue;
                }
                
                checkFoodCollision();
                updateBoard();
                displayBoard();
                
                Sleep(speed);  // Control game speed
            } else {
                handleInput();
                Sleep(50);
            }
        }
        
        // Final goodbye message
        system("cls");
        std::cout << "Thanks for playing Snake!" << std::endl;
        std::cout << "Final Statistics:" << std::endl;
        std::cout << "- Final Score: " << score << std::endl;
        std::cout << "- Level Reached: " << level << std::endl;
        std::cout << "- Snake Length: " << snake.getLength() << std::endl;
        std::cout << "- High Score: " << highScore << std::endl;
        std::cout << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        _getch();
    }
};

// Bonus: High Score Manager class
class HighScoreManager {
private:
    struct ScoreEntry {
        std::string name;
        int score;
        int level;
        std::string date;
        
        ScoreEntry(const std::string& n, int s, int l, const std::string& d) 
            : name(n), score(s), level(l), date(d) {}
    };
    
    std::vector<ScoreEntry> scores;
    static const int MAX_SCORES = 10;
    
public:
    void addScore(const std::string& name, int score, int level) {
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string date(dt);
        date.pop_back(); // Remove newline
        
        scores.emplace_back(name, score, level, date);
        
        // Sort by score (descending)
        std::sort(scores.begin(), scores.end(), 
                  [](const ScoreEntry& a, const ScoreEntry& b) {
                      return a.score > b.score;
                  });
        
        // Keep only top scores
        if (scores.size() > MAX_SCORES) {
            scores.resize(MAX_SCORES);
        }
        
        saveScores();
    }
    
    void displayHighScores() const {
        system("cls");
        std::cout << "========================================" << std::endl;
        std::cout << "           HIGH SCORES                  " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        if (scores.empty()) {
            std::cout << "No high scores yet!" << std::endl;
        } else {
            std::cout << "Rank | Name         | Score | Level | Date" << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            
            for (size_t i = 0; i < scores.size(); i++) {
                const ScoreEntry& entry = scores[i];
                std::cout << std::setw(4) << (i + 1) << " | "
                          << std::setw(12) << entry.name << " | "
                          << std::setw(5) << entry.score << " | "
                          << std::setw(5) << entry.level << " | "
                          << entry.date << std::endl;
            }
        }
        
        std::cout << std::endl;
        std::cout << "Press any key to continue..." << std::endl;
        _getch();
    }
    
    void saveScores() const {
        std::ofstream file("snake_highscores.txt");
        if (file.is_open()) {
            for (const ScoreEntry& entry : scores) {
                file << entry.name << "," << entry.score << "," 
                     << entry.level << "," << entry.date << std::endl;
            }
            file.close();
        }
    }
    
    void loadScores() {
        scores.clear();
        std::ifstream file("snake_highscores.txt");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                // Parse CSV format: name,score,level,date
                size_t pos1 = line.find(',');
                size_t pos2 = line.find(',', pos1 + 1);
                size_t pos3 = line.find(',', pos2 + 1);
                
                if (pos1 != std::string::npos && pos2 != std::string::npos && 
                    pos3 != std::string::npos) {
                    std::string name = line.substr(0, pos1);
                    int score = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                    int level = std::stoi(line.substr(pos2 + 1, pos3 - pos2 - 1));
                    std::string date = line.substr(pos3 + 1);
                    
                    scores.emplace_back(name, score, level, date);
                }
            }
            file.close();
        }
    }
};

// Enhanced main function with menu system
int main() {
    // Set console title (Windows specific)
    SetConsoleTitle(L"Snake Game");
    
    // Hide cursor for better visual experience
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(consoleHandle, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(consoleHandle, &cursorInfo);
    
    HighScoreManager scoreManager;
    scoreManager.loadScores();
    
    while (true) {
        system("cls");
        std::cout << "========================================" << std::endl;
        std::cout << "           SNAKE GAME MENU              " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "1. Play Game" << std::endl;
        std::cout << "2. View High Scores" << std::endl;
        std::cout << "3. Instructions" << std::endl;
        std::cout << "4. Exit" << std::endl;
        std::cout << std::endl;
        std::cout << "Enter your choice (1-4): ";
        
        char choice = _getch();
        
        switch (choice) {
            case '1': {
                Game game;
                game.run();
                
                // After game ends, optionally add to high scores
                system("cls");
                std::cout << "Enter your name for high scores (or press ESC to skip): ";
                std::string name;
                std::getline(std::cin, name);
                
                if (!name.empty() && name[0] != 27) { // Not ESC
                    // Note: You'd need to pass the final score and level from Game class
                    // This would require modifying the Game class to return these values
                }
                break;
            }
            case '2':
                scoreManager.displayHighScores();
                break;
            case '3': {
                system("cls");
                std::cout << "========================================" << std::endl;
                std::cout << "           INSTRUCTIONS                 " << std::endl;
                std::cout << "========================================" << std::endl;
                std::cout << std::endl;
                std::cout << "OBJECTIVE:" << std::endl;
                std::cout << "Control the snake to eat food and grow as long as possible!" << std::endl;
                std::cout << std::endl;
                std::cout << "CONTROLS:" << std::endl;
                std::cout << "W / Up Arrow    - Move Up" << std::endl;
                std::cout << "S / Down Arrow  - Move Down" << std::endl;
                std::cout << "A / Left Arrow  - Move Left" << std::endl;
                std::cout << "D / Right Arrow - Move Right" << std::endl;
                std::cout << "P               - Pause Game" << std::endl;
                std::cout << "ESC             - Quit Game" << std::endl;
                std::cout << "R               - Restart (when game over)" << std::endl;
                std::cout << std::endl;
                std::cout << "GAMEPLAY:" << std::endl;
                std::cout << "- Eat food (*) to grow and gain points" << std::endl;
                std::cout << "- Avoid hitting walls (#) and yourself" << std::endl;
                std::cout << "- Every 100 points increases your level" << std::endl;
                std::cout << "- Higher levels = faster speed + more points per food" << std::endl;
                std::cout << "- Snake head is 'O', body segments are 'o'" << std::endl;
                std::cout << std::endl;
                std::cout << "SCORING:" << std::endl;
                std::cout << "Points per food = 10 × Current Level" << std::endl;
                std::cout << std::endl;
                std::cout << "Press any key to return to menu..." << std::endl;
                _getch();
                break;
            }
            case '4':
                // Restore cursor before exit
                cursorInfo.bVisible = true;
                SetConsoleCursorInfo(consoleHandle, &cursorInfo);
                
                std::cout << "\nThanks for playing Snake! Goodbye!" << std::endl;
                return 0;
            default:
                std::cout << "\nInvalid choice! Press any key to continue..." << std::endl;
                _getch();
        }
    }
    
    return 0;
}
