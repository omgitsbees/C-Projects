#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>

class WordFrequencyCounter {
private:
    std::map<std::string, int> wordCount;
    
    // Function to convert string to lowercase and remove punctuation
    std::string cleanWord(const std::string& word) {
        std::string cleaned;
        for (char c : word) {
            if (std::isalnum(c)) {
                cleaned += std::tolower(c);
            }
        }
        return cleaned;
    }
    
public:
    // Read file and count word frequencies
    bool processFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
            return false;
        }
        
        std::string line, word;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            while (iss >> word) {
                std::string cleanedWord = cleanWord(word);
                if (!cleanedWord.empty()) {
                    wordCount[cleanedWord]++;
                }
            }
        }
        
        file.close();
        return true;
    }
    
    // Display results sorted by frequency (descending)
    void displayByFrequency() const {
        if (wordCount.empty()) {
            std::cout << "No words found in the file." << std::endl;
            return;
        }
        
        // Convert map to vector of pairs for sorting
        std::vector<std::pair<std::string, int>> wordPairs(wordCount.begin(), wordCount.end());
        
        // Sort by frequency (descending), then alphabetically for ties
        std::sort(wordPairs.begin(), wordPairs.end(), 
                  [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                      if (a.second != b.second) {
                          return a.second > b.second; // Higher frequency first
                      }
                      return a.first < b.first; // Alphabetical for ties
                  });
        
        std::cout << "\n=== Word Frequency Results (Sorted by Frequency) ===" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        for (const auto& pair : wordPairs) {
            std::cout << std::left << std::setw(25) << pair.first 
                      << " : " << pair.second << std::endl;
        }
    }
    
    // Display results sorted alphabetically
    void displayAlphabetically() const {
        if (wordCount.empty()) {
            std::cout << "No words found in the file." << std::endl;
            return;
        }
        
        std::cout << "\n=== Word Frequency Results (Alphabetical Order) ===" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        for (const auto& pair : wordCount) {
            std::cout << std::left << std::setw(25) << pair.first 
                      << " : " << pair.second << std::endl;
        }
    }
    
    // Get statistics
    void displayStatistics() const {
        if (wordCount.empty()) {
            std::cout << "No statistics available." << std::endl;
            return;
        }
        
        int totalWords = 0;
        int maxFreq = 0;
        std::string mostCommonWord;
        
        for (const auto& pair : wordCount) {
            totalWords += pair.second;
            if (pair.second > maxFreq) {
                maxFreq = pair.second;
                mostCommonWord = pair.first;
            }
        }
        
        std::cout << "\n=== Statistics ===" << std::endl;
        std::cout << "Unique words: " << wordCount.size() << std::endl;
        std::cout << "Total words: " << totalWords << std::endl;
        std::cout << "Most common word: \"" << mostCommonWord << "\" (" << maxFreq << " times)" << std::endl;
    }
    
    // Search for a specific word
    void searchWord(const std::string& word) const {
        std::string cleanedWord = cleanWord(word);
        auto it = wordCount.find(cleanedWord);
        
        if (it != wordCount.end()) {
            std::cout << "Word \"" << cleanedWord << "\" appears " << it->second << " times." << std::endl;
        } else {
            std::cout << "Word \"" << cleanedWord << "\" not found in the file." << std::endl;
        }
    }
};

// Function to create a sample text file for testing
void createSampleFile(const std::string& filename) {
    std::ofstream file(filename);
    file << "The quick brown fox jumps over the lazy dog.\n";
    file << "The dog was really lazy, and the fox was very quick.\n";
    file << "Quick brown animals are fascinating to watch.\n";
    file << "The lazy dog slept while the quick fox ran around.\n";
    file << "Brown, quick, lazy - these are simple adjectives.\n";
    file.close();
    std::cout << "Sample file '" << filename << "' created successfully!" << std::endl;
}

int main() {
    WordFrequencyCounter counter;
    std::string filename;
    
    std::cout << "=== Word Frequency Counter ===" << std::endl;
    
    // Option to create a sample file for testing
    std::cout << "\nWould you like to create a sample text file for testing? (y/n): ";
    char choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear the newline from buffer
    
    if (choice == 'y' || choice == 'Y') {
        createSampleFile("sample.txt");
        filename = "sample.txt";
    } else {
        std::cout << "Enter the filename to analyze: ";
        std::getline(std::cin, filename);
    }
    
    // Process the file
    if (!counter.processFile(filename)) {
        return 1;
    }
    
    // Interactive menu
    while (true) {
        std::cout << "\n=== Options ===" << std::endl;
        std::cout << "1. Display words sorted by frequency" << std::endl;
        std::cout << "2. Display words alphabetically" << std::endl;
        std::cout << "3. Show statistics" << std::endl;
        std::cout << "4. Search for a specific word" << std::endl;
        std::cout << "5. Process a different file" << std::endl;
        std::cout << "6. Exit" << std::endl;
        std::cout << "Choose an option (1-6): ";
        
        int option;
        std::cin >> option;
        std::cin.ignore(); // Clear the newline
        
        switch (option) {
            case 1:
                counter.displayByFrequency();
                break;
            case 2:
                counter.displayAlphabetically();
                break;
            case 3:
                counter.displayStatistics();
                break;
            case 4: {
                std::cout << "Enter word to search for: ";
                std::string searchWord;
                std::getline(std::cin, searchWord);
                counter.searchWord(searchWord);
                break;
            }
            case 5: {
                std::cout << "Enter new filename: ";
                std::getline(std::cin, filename);
                if (counter.processFile(filename)) {
                    std::cout << "New file processed successfully!" << std::endl;
                }
                break;
            }
            case 6:
                std::cout << "Thank you for using Word Frequency Counter!" << std::endl;
                return 0;
            default:
                std::cout << "Invalid option. Please try again." << std::endl;
        }
    }
    
    return 0;
}