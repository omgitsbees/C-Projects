#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <string>
#include <bitset>
#include <iomanip>
#include <filesystem>

// Node structure for Huffman tree
struct Node {
    char character;
    int frequency;
    Node* left;
    Node* right;
    
    Node(char ch, int freq) : character(ch), frequency(freq), left(nullptr), right(nullptr) {}
    Node(int freq) : character(0), frequency(freq), left(nullptr), right(nullptr) {}
    
    ~Node() {
        delete left;
        delete right;
    }
};

// Comparator for priority queue (min-heap based on frequency)
struct Compare {
    bool operator()(Node* a, Node* b) {
        if (a->frequency == b->frequency) {
            return a->character > b->character; // For consistent ordering
        }
        return a->frequency > b->frequency;
    }
};

class HuffmanCompressor {
private:
    Node* root;
    std::unordered_map<char, std::string> codes;
    std::unordered_map<std::string, char> reverseCodes;
    
    void generateCodes(Node* node, const std::string& code) {
        if (!node) return;
        
        if (!node->left && !node->right) {
            // Leaf node - store the code
            codes[node->character] = code.empty() ? "0" : code; // Handle single character case
            reverseCodes[code.empty() ? "0" : code] = node->character;
            return;
        }
        
        generateCodes(node->left, code + "0");
        generateCodes(node->right, code + "1");
    }
    
    void serializeTree(Node* node, std::string& serialized) {
        if (!node) {
            serialized += "0"; // Null marker
            return;
        }
        
        if (!node->left && !node->right) {
            serialized += "1"; // Leaf marker
            serialized += node->character;
        } else {
            serialized += "2"; // Internal node marker
            serializeTree(node->left, serialized);
            serializeTree(node->right, serialized);
        }
    }
    
    Node* deserializeTree(const std::string& serialized, size_t& index) {
        if (index >= serialized.length()) return nullptr;
        
        char marker = serialized[index++];
        if (marker == '0') {
            return nullptr;
        } else if (marker == '1') {
            // Leaf node
            if (index >= serialized.length()) return nullptr;
            char ch = serialized[index++];
            return new Node(ch, 0);
        } else if (marker == '2') {
            // Internal node
            Node* node = new Node(0);
            node->left = deserializeTree(serialized, index);
            node->right = deserializeTree(serialized, index);
            return node;
        }
        return nullptr;
    }
    
    std::string stringToBinary(const std::string& str) {
        std::string binary;
        for (char c : str) {
            binary += std::bitset<8>(c).to_string();
        }
        return binary;
    }
    
    std::string binaryToString(const std::string& binary) {
        std::string result;
        for (size_t i = 0; i < binary.length(); i += 8) {
            if (i + 8 <= binary.length()) {
                std::bitset<8> bits(binary.substr(i, 8));
                result += static_cast<char>(bits.to_ulong());
            }
        }
        return result;
    }

public:
    HuffmanCompressor() : root(nullptr) {}
    
    ~HuffmanCompressor() {
        delete root;
    }
    
    void buildHuffmanTree(const std::string& text) {
        // Count frequencies
        std::unordered_map<char, int> frequency;
        for (char c : text) {
            frequency[c]++;
        }
        
        // Special case: single character
        if (frequency.size() == 1) {
            char ch = frequency.begin()->first;
            root = new Node(ch, frequency[ch]);
            codes[ch] = "0";
            reverseCodes["0"] = ch;
            return;
        }
        
        // Build priority queue
        std::priority_queue<Node*, std::vector<Node*>, Compare> pq;
        for (auto& pair : frequency) {
            pq.push(new Node(pair.first, pair.second));
        }
        
        // Build Huffman tree
        while (pq.size() > 1) {
            Node* right = pq.top(); pq.pop();
            Node* left = pq.top(); pq.pop();
            
            Node* merged = new Node(left->frequency + right->frequency);
            merged->left = left;
            merged->right = right;
            pq.push(merged);
        }
        
        root = pq.top();
        
        // Generate codes
        codes.clear();
        reverseCodes.clear();
        generateCodes(root, "");
    }
    
    std::string compress(const std::string& text) {
        if (text.empty()) return "";
        
        buildHuffmanTree(text);
        
        // Encode the text
        std::string encoded;
        for (char c : text) {
            encoded += codes[c];
        }
        
        return encoded;
    }
    
    std::string decompress(const std::string& encoded) {
        if (encoded.empty() || !root) return "";
        
        std::string decoded;
        Node* current = root;
        
        // Special case: single character tree
        if (!root->left && !root->right) {
            for (char bit : encoded) {
                decoded += root->character;
            }
            return decoded;
        }
        
        for (char bit : encoded) {
            if (bit == '0') {
                current = current->left;
            } else {
                current = current->right;
            }
            
            if (!current->left && !current->right) {
                decoded += current->character;
                current = root;
            }
        }
        
        return decoded;
    }
    
    bool compressFile(const std::string& inputFile, const std::string& outputFile) {
        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
            return false;
        }
        
        // Read file content
        std::string content((std::istreambuf_iterator<char>(inFile)),
                           std::istreambuf_iterator<char>());
        inFile.close();
        
        if (content.empty()) {
            std::cerr << "Error: Input file is empty" << std::endl;
            return false;
        }
        
        // Compress
        std::string compressed = compress(content);
        
        // Serialize tree
        std::string serializedTree;
        serializeTree(root, serializedTree);
        
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Cannot create output file " << outputFile << std::endl;
            return false;
        }
        
        // Write file format:
        // [tree_size][serialized_tree][original_text_size][compressed_data_bits][padding_bits]
        uint32_t treeSize = serializedTree.length();
        uint32_t originalSize = content.length();
        uint32_t compressedBits = compressed.length();
        
        outFile.write(reinterpret_cast<const char*>(&treeSize), sizeof(treeSize));
        outFile.write(serializedTree.c_str(), treeSize);
        outFile.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));
        outFile.write(reinterpret_cast<const char*>(&compressedBits), sizeof(compressedBits));
        
        // Convert binary string to bytes
        std::string paddedCompressed = compressed;
        int padding = 8 - (compressed.length() % 8);
        if (padding != 8) {
            paddedCompressed += std::string(padding, '0');
        }
        
        uint8_t paddingBits = (padding == 8) ? 0 : padding;
        outFile.write(reinterpret_cast<const char*>(&paddingBits), sizeof(paddingBits));
        
        // Write compressed data as bytes
        for (size_t i = 0; i < paddedCompressed.length(); i += 8) {
            std::bitset<8> byte(paddedCompressed.substr(i, 8));
            uint8_t byteValue = static_cast<uint8_t>(byte.to_ulong());
            outFile.write(reinterpret_cast<const char*>(&byteValue), sizeof(byteValue));
        }
        
        outFile.close();
        return true;
    }
    
    bool decompressFile(const std::string& inputFile, const std::string& outputFile) {
        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            std::cerr << "Error: Cannot open compressed file " << inputFile << std::endl;
            return false;
        }
        
        // Read tree size
        uint32_t treeSize;
        inFile.read(reinterpret_cast<char*>(&treeSize), sizeof(treeSize));
        
        // Read serialized tree
        std::string serializedTree(treeSize, '\0');
        inFile.read(&serializedTree[0], treeSize);
        
        // Deserialize tree
        size_t index = 0;
        delete root;
        root = deserializeTree(serializedTree, index);
        
        // Read original size and compressed bits count
        uint32_t originalSize, compressedBits;
        inFile.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
        inFile.read(reinterpret_cast<char*>(&compressedBits), sizeof(compressedBits));
        
        // Read padding
        uint8_t paddingBits;
        inFile.read(reinterpret_cast<char*>(&paddingBits), sizeof(paddingBits));
        
        // Read compressed data
        std::string compressedData;
        uint8_t byte;
        while (inFile.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
            compressedData += std::bitset<8>(byte).to_string();
        }
        inFile.close();
        
        // Remove padding
        if (paddingBits > 0) {
            compressedData = compressedData.substr(0, compressedData.length() - paddingBits);
        }
        
        // Ensure we have the right number of bits
        compressedData = compressedData.substr(0, compressedBits);
        
        // Generate codes for decompression
        codes.clear();
        reverseCodes.clear();
        generateCodes(root, "");
        
        // Decompress
        std::string decompressed = decompress(compressedData);
        
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Cannot create output file " << outputFile << std::endl;
            return false;
        }
        
        outFile.write(decompressed.c_str(), decompressed.length());
        outFile.close();
        
        return true;
    }
    
    void printCompressionStats(const std::string& originalFile, const std::string& compressedFile) {
        try {
            auto originalSize = std::filesystem::file_size(originalFile);
            auto compressedSize = std::filesystem::file_size(compressedFile);
            
            double compressionRatio = (double)compressedSize / originalSize;
            double spaceSaved = (1.0 - compressionRatio) * 100;
            
            std::cout << "\n=== Compression Statistics ===" << std::endl;
            std::cout << "Original file size:   " << originalSize << " bytes" << std::endl;
            std::cout << "Compressed file size: " << compressedSize << " bytes" << std::endl;
            std::cout << "Compression ratio:    " << std::fixed << std::setprecision(2) 
                      << compressionRatio << ":1" << std::endl;
            std::cout << "Space saved:          " << std::fixed << std::setprecision(1) 
                      << spaceSaved << "%" << std::endl;
            
            if (spaceSaved > 0) {
                std::cout << "Compression successful!" << std::endl;
            } else {
                std::cout << "Note: File expanded due to small size or low redundancy." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error calculating file sizes: " << e.what() << std::endl;
        }
    }
    
    void printHuffmanCodes() {
        std::cout << "\n=== Huffman Codes ===" << std::endl;
        for (const auto& pair : codes) {
            std::cout << "'" << (pair.first == '\n' ? "\\n" : 
                                pair.first == '\t' ? "\\t" : 
                                pair.first == ' ' ? "SPACE" : 
                                std::string(1, pair.first)) 
                      << "': " << pair.second << std::endl;
        }
    }
};

void printUsage() {
    std::cout << "Usage:" << std::endl;
    std::cout << "  Compress:   ./huffman -c input.txt compressed.huf" << std::endl;
    std::cout << "  Decompress: ./huffman -d compressed.huf output.txt" << std::endl;
    std::cout << "  Help:       ./huffman -h" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
            printUsage();
            return 0;
        }
        std::cerr << "Error: Invalid number of arguments." << std::endl;
        printUsage();
        return 1;
    }
    
    std::string mode = argv[1];
    std::string inputFile = argv[2];
    std::string outputFile = argv[3];
    
    HuffmanCompressor compressor;
    
    if (mode == "-c" || mode == "--compress") {
        std::cout << "Compressing " << inputFile << " to " << outputFile << "..." << std::endl;
        
        if (compressor.compressFile(inputFile, outputFile)) {
            std::cout << "Compression completed successfully!" << std::endl;
            compressor.printCompressionStats(inputFile, outputFile);
            compressor.printHuffmanCodes();
        } else {
            std::cerr << "Compression failed!" << std::endl;
            return 1;
        }
        
    } else if (mode == "-d" || mode == "--decompress") {
        std::cout << "Decompressing " << inputFile << " to " << outputFile << "..." << std::endl;
        
        if (compressor.decompressFile(inputFile, outputFile)) {
            std::cout << "Decompression completed successfully!" << std::endl;
        } else {
            std::cerr << "Decompression failed!" << std::endl;
            return 1;
        }
        
    } else {
        std::cerr << "Error: Invalid mode. Use -c for compression or -d for decompression." << std::endl;
        printUsage();
        return 1;
    }
    
    return 0;
}

// Example usage and test function (uncomment to test)
/*
void testCompression() {
    HuffmanCompressor compressor;
    
    // Create a test file
    std::ofstream testFile("test.txt");
    testFile << "This is a sample text file for testing Huffman compression. "
             << "It contains repeated characters and words to demonstrate compression efficiency. "
             << "The algorithm should work well with this text!";
    testFile.close();
    
    // Test compression
    if (compressor.compressFile("test.txt", "test.huf")) {
        std::cout << "Test compression successful!" << std::endl;
        compressor.printCompressionStats("test.txt", "test.huf");
        
        // Test decompression
        if (compressor.decompressFile("test.huf", "test_decompressed.txt")) {
            std::cout << "Test decompression successful!" << std::endl;
        }
    }
}
*/