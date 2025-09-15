#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

// Forward declarations
class Book;
class Member;
class Transaction;

// Utility function to get current date as string
std::string getCurrentDate() {
    time_t now = time(0);
    char* dt = ctime(&now);
    std::string date(dt);
    date.pop_back(); // Remove newline
    return date;
}

// Calculate days between two date strings (simplified)
int daysBetween(const std::string& date1, const std::string& date2) {
    // Simplified calculation - in real implementation, use proper date parsing
    return 5; // Mock return for demo
}

// Book class
class Book {
private:
    int bookId;
    std::string title;
    std::string author;
    std::string isbn;
    bool isAvailable;
    std::string genre;

public:
    Book() : bookId(0), isAvailable(true) {}
    
    Book(int id, const std::string& t, const std::string& a, 
         const std::string& i, const std::string& g) 
        : bookId(id), title(t), author(a), isbn(i), genre(g), isAvailable(true) {}

    // Getters
    int getId() const { return bookId; }
    std::string getTitle() const { return title; }
    std::string getAuthor() const { return author; }
    std::string getIsbn() const { return isbn; }
    std::string getGenre() const { return genre; }
    bool getAvailability() const { return isAvailable; }

    // Setters
    void setAvailability(bool available) { isAvailable = available; }
    void setTitle(const std::string& t) { title = t; }
    void setAuthor(const std::string& a) { author = a; }
    void setGenre(const std::string& g) { genre = g; }

    // Display book information
    void displayInfo() const {
        std::cout << "ID: " << bookId << " | Title: " << title 
                  << " | Author: " << author << " | ISBN: " << isbn
                  << " | Genre: " << genre 
                  << " | Status: " << (isAvailable ? "Available" : "Checked Out") << std::endl;
    }

    // Serialize to string for file storage
    std::string serialize() const {
        return std::to_string(bookId) + "," + title + "," + author + "," + 
               isbn + "," + genre + "," + (isAvailable ? "1" : "0");
    }

    // Deserialize from string
    static Book deserialize(const std::string& data) {
        std::stringstream ss(data);
        std::string item;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, item, ',')) {
            tokens.push_back(item);
        }
        
        if (tokens.size() >= 6) {
            Book book(std::stoi(tokens[0]), tokens[1], tokens[2], tokens[3], tokens[4]);
            book.setAvailability(tokens[5] == "1");
            return book;
        }
        return Book();
    }
};

// Member class
class Member {
private:
    int memberId;
    std::string name;
    std::string email;
    std::string phone;
    std::string address;
    std::vector<int> checkedOutBooks;
    std::string membershipDate;

public:
    Member() : memberId(0) {}
    
    Member(int id, const std::string& n, const std::string& e, 
           const std::string& p, const std::string& addr) 
        : memberId(id), name(n), email(e), phone(p), address(addr) {
        membershipDate = getCurrentDate();
    }

    // Getters
    int getId() const { return memberId; }
    std::string getName() const { return name; }
    std::string getEmail() const { return email; }
    std::string getPhone() const { return phone; }
    std::string getAddress() const { return address; }
    std::string getMembershipDate() const { return membershipDate; }
    const std::vector<int>& getCheckedOutBooks() const { return checkedOutBooks; }

    // Setters
    void setName(const std::string& n) { name = n; }
    void setEmail(const std::string& e) { email = e; }
    void setPhone(const std::string& p) { phone = p; }
    void setAddress(const std::string& addr) { address = addr; }

    // Book management
    void checkOutBook(int bookId) {
        checkedOutBooks.push_back(bookId);
    }

    void returnBook(int bookId) {
        auto it = std::find(checkedOutBooks.begin(), checkedOutBooks.end(), bookId);
        if (it != checkedOutBooks.end()) {
            checkedOutBooks.erase(it);
        }
    }

    bool hasBook(int bookId) const {
        return std::find(checkedOutBooks.begin(), checkedOutBooks.end(), bookId) != checkedOutBooks.end();
    }

    // Display member information
    void displayInfo() const {
        std::cout << "ID: " << memberId << " | Name: " << name 
                  << " | Email: " << email << " | Phone: " << phone
                  << " | Books Checked Out: " << checkedOutBooks.size() << std::endl;
    }

    // Serialize to string for file storage
    std::string serialize() const {
        std::string result = std::to_string(memberId) + "," + name + "," + email + "," + 
                           phone + "," + address + "," + membershipDate + ",";
        
        for (size_t i = 0; i < checkedOutBooks.size(); ++i) {
            result += std::to_string(checkedOutBooks[i]);
            if (i < checkedOutBooks.size() - 1) result += ";";
        }
        
        return result;
    }

    // Deserialize from string
    static Member deserialize(const std::string& data) {
        std::stringstream ss(data);
        std::string item;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, item, ',')) {
            tokens.push_back(item);
        }
        
        if (tokens.size() >= 6) {
            Member member(std::stoi(tokens[0]), tokens[1], tokens[2], tokens[3], tokens[4]);
            member.membershipDate = tokens[5];
            
            // Parse checked out books
            if (tokens.size() > 6 && !tokens[6].empty()) {
                std::stringstream bookStream(tokens[6]);
                std::string bookId;
                while (std::getline(bookStream, bookId, ';')) {
                    member.checkedOutBooks.push_back(std::stoi(bookId));
                }
            }
            
            return member;
        }
        return Member();
    }
};

// Transaction class
class Transaction {
private:
    int transactionId;
    int memberId;
    int bookId;
    std::string checkoutDate;
    std::string dueDate;
    std::string returnDate;
    bool isReturned;
    double fine;

public:
    Transaction() : transactionId(0), memberId(0), bookId(0), isReturned(false), fine(0.0) {}
    
    Transaction(int tId, int mId, int bId) 
        : transactionId(tId), memberId(mId), bookId(bId), isReturned(false), fine(0.0) {
        checkoutDate = getCurrentDate();
        dueDate = getCurrentDate(); // In real implementation, add 14 days
    }

    // Getters
    int getTransactionId() const { return transactionId; }
    int getMemberId() const { return memberId; }
    int getBookId() const { return bookId; }
    std::string getCheckoutDate() const { return checkoutDate; }
    std::string getDueDate() const { return dueDate; }
    std::string getReturnDate() const { return returnDate; }
    bool getIsReturned() const { return isReturned; }
    double getFine() const { return fine; }

    // Return book and calculate fine if overdue
    void returnBook() {
        returnDate = getCurrentDate();
        isReturned = true;
        
        // Calculate fine for overdue books
        int overdueDays = daysBetween(dueDate, returnDate);
        if (overdueDays > 0) {
            fine = overdueDays * 1.0; // $1 per day
        }
    }

    bool isOverdue() const {
        if (isReturned) return false;
        return daysBetween(getCurrentDate(), dueDate) > 0;
    }

    // Display transaction information
    void displayInfo() const {
        std::cout << "Transaction ID: " << transactionId 
                  << " | Member ID: " << memberId << " | Book ID: " << bookId
                  << " | Checkout: " << checkoutDate << " | Due: " << dueDate;
        
        if (isReturned) {
            std::cout << " | Returned: " << returnDate;
            if (fine > 0) {
                std::cout << " | Fine: $" << std::fixed << std::setprecision(2) << fine;
            }
        } else if (isOverdue()) {
            std::cout << " | OVERDUE";
        }
        std::cout << std::endl;
    }

    // Serialize to string for file storage
    std::string serialize() const {
        return std::to_string(transactionId) + "," + std::to_string(memberId) + "," + 
               std::to_string(bookId) + "," + checkoutDate + "," + dueDate + "," + 
               returnDate + "," + (isReturned ? "1" : "0") + "," + std::to_string(fine);
    }

    // Deserialize from string
    static Transaction deserialize(const std::string& data) {
        std::stringstream ss(data);
        std::string item;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, item, ',')) {
            tokens.push_back(item);
        }
        
        if (tokens.size() >= 8) {
            Transaction trans;
            trans.transactionId = std::stoi(tokens[0]);
            trans.memberId = std::stoi(tokens[1]);
            trans.bookId = std::stoi(tokens[2]);
            trans.checkoutDate = tokens[3];
            trans.dueDate = tokens[4];
            trans.returnDate = tokens[5];
            trans.isReturned = (tokens[6] == "1");
            trans.fine = std::stod(tokens[7]);
            return trans;
        }
        return Transaction();
    }
};

// Library Management System class
class LibrarySystem {
private:
    std::vector<Book> books;
    std::vector<Member> members;
    std::vector<Transaction> transactions;
    int nextBookId;
    int nextMemberId;
    int nextTransactionId;

public:
    LibrarySystem() : nextBookId(1), nextMemberId(1), nextTransactionId(1) {}

    // Book Management
    void addBook(const std::string& title, const std::string& author, 
                 const std::string& isbn, const std::string& genre) {
        books.emplace_back(nextBookId++, title, author, isbn, genre);
        std::cout << "Book added successfully!" << std::endl;
    }

    void removeBook(int bookId) {
        auto it = std::find_if(books.begin(), books.end(), 
                              [bookId](const Book& book) { return book.getId() == bookId; });
        
        if (it != books.end()) {
            if (it->getAvailability()) {
                books.erase(it);
                std::cout << "Book removed successfully!" << std::endl;
            } else {
                std::cout << "Cannot remove book - it's currently checked out!" << std::endl;
            }
        } else {
            std::cout << "Book not found!" << std::endl;
        }
    }

    void displayBooks() const {
        std::cout << "\n=== LIBRARY BOOKS ===" << std::endl;
        for (const auto& book : books) {
            book.displayInfo();
        }
        std::cout << std::endl;
    }

    // Member Management
    void addMember(const std::string& name, const std::string& email, 
                   const std::string& phone, const std::string& address) {
        members.emplace_back(nextMemberId++, name, email, phone, address);
        std::cout << "Member registered successfully! Member ID: " << (nextMemberId - 1) << std::endl;
    }

    void removeMember(int memberId) {
        auto it = std::find_if(members.begin(), members.end(), 
                              [memberId](const Member& member) { return member.getId() == memberId; });
        
        if (it != members.end()) {
            if (it->getCheckedOutBooks().empty()) {
                members.erase(it);
                std::cout << "Member removed successfully!" << std::endl;
            } else {
                std::cout << "Cannot remove member - they have books checked out!" << std::endl;
            }
        } else {
            std::cout << "Member not found!" << std::endl;
        }
    }

    void displayMembers() const {
        std::cout << "\n=== LIBRARY MEMBERS ===" << std::endl;
        for (const auto& member : members) {
            member.displayInfo();
        }
        std::cout << std::endl;
    }

    // Transaction Management
    bool checkOutBook(int memberId, int bookId) {
        auto memberIt = std::find_if(members.begin(), members.end(), 
                                    [memberId](const Member& m) { return m.getId() == memberId; });
        auto bookIt = std::find_if(books.begin(), books.end(), 
                                  [bookId](const Book& b) { return b.getId() == bookId; });

        if (memberIt == members.end()) {
            std::cout << "Member not found!" << std::endl;
            return false;
        }

        if (bookIt == books.end()) {
            std::cout << "Book not found!" << std::endl;
            return false;
        }

        if (!bookIt->getAvailability()) {
            std::cout << "Book is not available!" << std::endl;
            return false;
        }

        // Check if member has too many books (limit: 5)
        if (memberIt->getCheckedOutBooks().size() >= 5) {
            std::cout << "Member has reached the maximum book limit (5 books)!" << std::endl;
            return false;
        }

        // Create transaction
        transactions.emplace_back(nextTransactionId++, memberId, bookId);
        
        // Update book and member
        bookIt->setAvailability(false);
        const_cast<Member&>(*memberIt).checkOutBook(bookId);

        std::cout << "Book checked out successfully!" << std::endl;
        return true;
    }

    bool returnBook(int memberId, int bookId) {
        auto memberIt = std::find_if(members.begin(), members.end(), 
                                    [memberId](const Member& m) { return m.getId() == memberId; });
        auto bookIt = std::find_if(books.begin(), books.end(), 
                                  [bookId](const Book& b) { return b.getId() == bookId; });

        if (memberIt == members.end()) {
            std::cout << "Member not found!" << std::endl;
            return false;
        }

        if (bookIt == books.end()) {
            std::cout << "Book not found!" << std::endl;
            return false;
        }

        if (!memberIt->hasBook(bookId)) {
            std::cout << "Member doesn't have this book checked out!" << std::endl;
            return false;
        }

        // Find and update transaction
        auto transIt = std::find_if(transactions.begin(), transactions.end(), 
                                   [memberId, bookId](const Transaction& t) { 
                                       return t.getMemberId() == memberId && 
                                              t.getBookId() == bookId && 
                                              !t.getIsReturned(); 
                                   });

        if (transIt != transactions.end()) {
            const_cast<Transaction&>(*transIt).returnBook();
            double fine = transIt->getFine();
            
            if (fine > 0) {
                std::cout << "Book returned. Fine: $" << std::fixed << std::setprecision(2) << fine << std::endl;
            } else {
                std::cout << "Book returned successfully!" << std::endl;
            }
        }

        // Update book and member
        bookIt->setAvailability(true);
        const_cast<Member&>(*memberIt).returnBook(bookId);

        return true;
    }

    void displayTransactions() const {
        std::cout << "\n=== TRANSACTION HISTORY ===" << std::endl;
        for (const auto& transaction : transactions) {
            transaction.displayInfo();
        }
        std::cout << std::endl;
    }

    void displayOverdueBooks() const {
        std::cout << "\n=== OVERDUE BOOKS ===" << std::endl;
        bool found = false;
        
        for (const auto& transaction : transactions) {
            if (transaction.isOverdue()) {
                transaction.displayInfo();
                found = true;
            }
        }
        
        if (!found) {
            std::cout << "No overdue books!" << std::endl;
        }
        std::cout << std::endl;
    }

    // Search functionality
    void searchBooks(const std::string& query) const {
        std::cout << "\n=== SEARCH RESULTS ===" << std::endl;
        bool found = false;
        
        for (const auto& book : books) {
            if (book.getTitle().find(query) != std::string::npos ||
                book.getAuthor().find(query) != std::string::npos ||
                book.getGenre().find(query) != std::string::npos) {
                book.displayInfo();
                found = true;
            }
        }
        
        if (!found) {
            std::cout << "No books found matching: " << query << std::endl;
        }
        std::cout << std::endl;
    }

    // Database persistence
    void saveToFile() const {
        // Save books
        std::ofstream bookFile("books.txt");
        if (bookFile.is_open()) {
            for (const auto& book : books) {
                bookFile << book.serialize() << std::endl;
            }
            bookFile.close();
        }

        // Save members
        std::ofstream memberFile("members.txt");
        if (memberFile.is_open()) {
            for (const auto& member : members) {
                memberFile << member.serialize() << std::endl;
            }
            memberFile.close();
        }

        // Save transactions
        std::ofstream transFile("transactions.txt");
        if (transFile.is_open()) {
            for (const auto& transaction : transactions) {
                transFile << transaction.serialize() << std::endl;
            }
            transFile.close();
        }

        // Save counters
        std::ofstream counterFile("counters.txt");
        if (counterFile.is_open()) {
            counterFile << nextBookId << "," << nextMemberId << "," << nextTransactionId << std::endl;
            counterFile.close();
        }

        std::cout << "Data saved to files successfully!" << std::endl;
    }

    void loadFromFile() {
        books.clear();
        members.clear();
        transactions.clear();

        // Load books
        std::ifstream bookFile("books.txt");
        if (bookFile.is_open()) {
            std::string line;
            while (std::getline(bookFile, line)) {
                if (!line.empty()) {
                    books.push_back(Book::deserialize(line));
                }
            }
            bookFile.close();
        }

        // Load members
        std::ifstream memberFile("members.txt");
        if (memberFile.is_open()) {
            std::string line;
            while (std::getline(memberFile, line)) {
                if (!line.empty()) {
                    members.push_back(Member::deserialize(line));
                }
            }
            memberFile.close();
        }

        // Load transactions
        std::ifstream transFile("transactions.txt");
        if (transFile.is_open()) {
            std::string line;
            while (std::getline(transFile, line)) {
                if (!line.empty()) {
                    transactions.push_back(Transaction::deserialize(line));
                }
            }
            transFile.close();
        }

        // Load counters
        std::ifstream counterFile("counters.txt");
        if (counterFile.is_open()) {
            std::string line;
            if (std::getline(counterFile, line)) {
                std::stringstream ss(line);
                std::string item;
                std::vector<std::string> tokens;
                
                while (std::getline(ss, item, ',')) {
                    tokens.push_back(item);
                }
                
                if (tokens.size() >= 3) {
                    nextBookId = std::stoi(tokens[0]);
                    nextMemberId = std::stoi(tokens[1]);
                    nextTransactionId = std::stoi(tokens[2]);
                }
            }
            counterFile.close();
        }

        std::cout << "Data loaded from files successfully!" << std::endl;
    }

    // Statistics
    void displayStatistics() const {
        std::cout << "\n=== LIBRARY STATISTICS ===" << std::endl;
        std::cout << "Total Books: " << books.size() << std::endl;
        
        int availableBooks = 0;
        for (const auto& book : books) {
            if (book.getAvailability()) availableBooks++;
        }
        
        std::cout << "Available Books: " << availableBooks << std::endl;
        std::cout << "Checked Out Books: " << (books.size() - availableBooks) << std::endl;
        std::cout << "Total Members: " << members.size() << std::endl;
        std::cout << "Total Transactions: " << transactions.size() << std::endl;
        
        int overdueCount = 0;
        for (const auto& transaction : transactions) {
            if (transaction.isOverdue()) overdueCount++;
        }
        
        std::cout << "Overdue Books: " << overdueCount << std::endl;
        std::cout << std::endl;
    }
};

// Main function with menu system
int main() {
    LibrarySystem library;
    library.loadFromFile(); // Load existing data
    
    int choice;
    
    // Add some sample data if starting fresh
    if (true) { // Set to false to skip sample data
        library.addBook("The Great Gatsby", "F. Scott Fitzgerald", "978-0-7432-7356-5", "Fiction");
        library.addBook("To Kill a Mockingbird", "Harper Lee", "978-0-06-112008-4", "Fiction");
        library.addBook("1984", "George Orwell", "978-0-452-28423-4", "Dystopian");
        library.addBook("Pride and Prejudice", "Jane Austen", "978-0-14-143951-8", "Romance");
        library.addBook("The Catcher in the Rye", "J.D. Salinger", "978-0-316-76948-0", "Fiction");
        
        library.addMember("John Doe", "john@email.com", "555-0123", "123 Main St");
        library.addMember("Jane Smith", "jane@email.com", "555-0456", "456 Oak Ave");
        library.addMember("Bob Johnson", "bob@email.com", "555-0789", "789 Pine Rd");
    }
    
    while (true) {
        std::cout << "\n========== LIBRARY MANAGEMENT SYSTEM ==========" << std::endl;
        std::cout << "1.  Add Book" << std::endl;
        std::cout << "2.  Remove Book" << std::endl;
        std::cout << "3.  Display All Books" << std::endl;
        std::cout << "4.  Search Books" << std::endl;
        std::cout << "5.  Add Member" << std::endl;
        std::cout << "6.  Remove Member" << std::endl;
        std::cout << "7.  Display All Members" << std::endl;
        std::cout << "8.  Check Out Book" << std::endl;
        std::cout << "9.  Return Book" << std::endl;
        std::cout << "10. Display Transactions" << std::endl;
        std::cout << "11. Display Overdue Books" << std::endl;
        std::cout << "12. Display Statistics" << std::endl;
        std::cout << "13. Save Data to Files" << std::endl;
        std::cout << "14. Load Data from Files" << std::endl;
        std::cout << "0.  Exit" << std::endl;
        std::cout << "===============================================" << std::endl;
        std::cout << "Enter your choice: ";
        
        std::cin >> choice;
        std::cin.ignore(); // Clear newline from input buffer
        
        switch (choice) {
            case 1: {
                std::string title, author, isbn, genre;
                std::cout << "Enter book title: ";
                std::getline(std::cin, title);
                std::cout << "Enter author: ";
                std::getline(std::cin, author);
                std::cout << "Enter ISBN: ";
                std::getline(std::cin, isbn);
                std::cout << "Enter genre: ";
                std::getline(std::cin, genre);
                library.addBook(title, author, isbn, genre);
                break;
            }
            case 2: {
                int bookId;
                std::cout << "Enter book ID to remove: ";
                std::cin >> bookId;
                library.removeBook(bookId);
                break;
            }
            case 3:
                library.displayBooks();
                break;
            case 4: {
                std::string query;
                std::cout << "Enter search query (title/author/genre): ";
                std::getline(std::cin, query);
                library.searchBooks(query);
                break;
            }
            case 5: {
                std::string name, email, phone, address;
                std::cout << "Enter member name: ";
                std::getline(std::cin, name);
                std::cout << "Enter email: ";
                std::getline(std::cin, email);
                std::cout << "Enter phone: ";
                std::getline(std::cin, phone);
                std::cout << "Enter address: ";
                std::getline(std::cin, address);
                library.addMember(name, email, phone, address);
                break;
            }
            case 6: {
                int memberId;
                std::cout << "Enter member ID to remove: ";
                std::cin >> memberId;
                library.removeMember(memberId);
                break;
            }
            case 7:
                library.displayMembers();
                break;
            case 8: {
                int memberId, bookId;
                std::cout << "Enter member ID: ";
                std::cin >> memberId;
                std::cout << "Enter book ID: ";
                std::cin >> bookId;
                library.checkOutBook(memberId, bookId);
                break;
            }
            case 9: {
                int memberId, bookId;
                std::cout << "Enter member ID: ";
                std::cin >> memberId;
                std::cout << "Enter book ID: ";
                std::cin >> bookId;
                library.returnBook(memberId, bookId);
                break;
            }
            case 10:
                library.displayTransactions();
                break;
            case 11:
                library.displayOverdueBooks();
                break;
            case 12:
                library.displayStatistics();
                break;
            case 13:
                library.saveToFile();
                break;
            case 14:
                library.loadFromFile();
                break;
            case 0:
                library.saveToFile(); // Auto-save on exit
                std::cout << "Thank you for using the Library Management System!" << std::endl;
                return 0;
            default:
                std::cout << "Invalid choice! Please try again." << std::endl;
        }
    }
    
    return 0;
}