#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cctype>
  
// Token types for lexical analysis
enum class TokenType {
    NUMBER,
    IDENTIFIER,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    ASSIGN,
    SEMICOLON,
    LPAREN,
    RPAREN,
    IF,
    ELSE,
    WHILE,
    PRINT,
    EQUALS,
    LESS,
    GREATER,
    END_OF_FILE
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
    
    Token(TokenType t, std::string v = "") : type(t), value(v) {}
};

// Lexer class for tokenizing input
class Lexer {
private:
    std::string input;
    size_t position;
    std::map<std::string, TokenType> keywords;

public:
    Lexer(const std::string& source) : input(source), position(0) {
        keywords["if"] = TokenType::IF;
        keywords["else"] = TokenType::ELSE;
        keywords["while"] = TokenType::WHILE;
        keywords["print"] = TokenType::PRINT;
    }

    Token getNextToken() {
        while (position < input.length() && std::isspace(input[position])) {
            position++;
        }

        if (position >= input.length()) {
            return Token(TokenType::END_OF_FILE);
        }

        char current = input[position];

        // Handle numbers
        if (std::isdigit(current)) {
            std::string number;
            while (position < input.length() && std::isdigit(input[position])) {
                number += input[position++];
            }
            return Token(TokenType::NUMBER, number);
        }

        // Handle identifiers and keywords
        if (std::isalpha(current)) {
            std::string identifier;
            while (position < input.length() && 
                   (std::isalnum(input[position]) || input[position] == '_')) {
                identifier += input[position++];
            }
            
            auto it = keywords.find(identifier);
            if (it != keywords.end()) {
                return Token(it->second);
            }
            return Token(TokenType::IDENTIFIER, identifier);
        }

        // Handle operators and other symbols
        position++;
        switch (current) {
            case '+': return Token(TokenType::PLUS);
            case '-': return Token(TokenType::MINUS);
            case '*': return Token(TokenType::MULTIPLY);
            case '/': return Token(TokenType::DIVIDE);
            case '=': 
                if (position < input.length() && input[position] == '=') {
                    position++;
                    return Token(TokenType::EQUALS);
                }
                return Token(TokenType::ASSIGN);
            case ';': return Token(TokenType::SEMICOLON);
            case '(': return Token(TokenType::LPAREN);
            case ')': return Token(TokenType::RPAREN);
            case '<': return Token(TokenType::LESS);
            case '>': return Token(TokenType::GREATER);
            default:
                throw std::runtime_error("Invalid character: " + std::string(1, current));
        }
    }
};

// Main interpreter class
class Interpreter {
private:
    std::map<std::string, double> variables;

public:
    double evaluateExpression(const std::string& expr) {
        Lexer lexer(expr);
        return parseExpression(lexer);
    }

private:
    double parseExpression(Lexer& lexer) {
        Token token = lexer.getNextToken();
        
        if (token.type == TokenType::NUMBER) {
            return std::stod(token.value);
        }
        else if (token.type == TokenType::IDENTIFIER) {
            auto it = variables.find(token.value);
            if (it == variables.end()) {
                throw std::runtime_error("Undefined variable: " + token.value);
            }
            return it->second;
        }
        
        throw std::runtime_error("Invalid expression");
    }
};

int main() {
    std::string input = R"(
        x = 10;
        y = 20;
        if (x < y) {
            print x;
        }
        while (x < 15) {
            x = x + 1;
            print x;
        }
    )";

    try {
        Interpreter interpreter;
        std::cout << interpreter.evaluateExpression("5 + 3 * 2") << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;

}
