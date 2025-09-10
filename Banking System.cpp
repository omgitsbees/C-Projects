#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <sstream>

class Transaction {
private:
    std::string type;
    double amount;
    double balanceAfter;
    std::string timestamp;
    
public:
    Transaction(const std::string& t, double amt, double balance) 
        : type(t), amount(amt), balanceAfter(balance) {
        // Get current timestamp
        time_t now = time(0);
        char* timeStr = ctime(&now);
        timestamp = std::string(timeStr);
        timestamp.pop_back(); // Remove newline character
    }
    
    void display() const {
        std::cout << std::left << std::setw(12) << type 
                  << " $" << std::setw(10) << std::fixed << std::setprecision(2) << amount
                  << " Balance: $" << std::setw(10) << balanceAfter
                  << " " << timestamp << std::endl;
    }
    
    std::string getType() const { return type; }
    double getAmount() const { return amount; }
    double getBalanceAfter() const { return balanceAfter; }
    std::string getTimestamp() const { return timestamp; }
};

class BankAccount {
private:
    std::string accountNumber;
    std::string accountHolder;
    double balance;
    std::vector<Transaction> transactionHistory;
    
public:
    BankAccount(const std::string& accNum, const std::string& holder, double initialBalance = 0.0)
        : accountNumber(accNum), accountHolder(holder), balance(initialBalance) {
        if (initialBalance > 0) {
            transactionHistory.emplace_back("OPENING", initialBalance, balance);
        }
    }
    
    bool deposit(double amount) {
        if (amount <= 0) {
            std::cout << "Error: Deposit amount must be positive!\n";
            return false;
        }
        
        balance += amount;
        transactionHistory.emplace_back("DEPOSIT", amount, balance);
        std::cout << "Successfully deposited $" << std::fixed << std::setprecision(2) 
                  << amount << std::endl;
        std::cout << "New balance: $" << balance << std::endl;
        return true;
    }
    
    bool withdraw(double amount) {
        if (amount <= 0) {
            std::cout << "Error: Withdrawal amount must be positive!\n";
            return false;
        }
        
        if (amount > balance) {
            std::cout << "Error: Insufficient funds! Current balance: $" 
                      << std::fixed << std::setprecision(2) << balance << std::endl;
            return false;
        }
        
        balance -= amount;
        transactionHistory.emplace_back("WITHDRAW", amount, balance);
        std::cout << "Successfully withdrew $" << std::fixed << std::setprecision(2) 
                  << amount << std::endl;
        std::cout << "New balance: $" << balance << std::endl;
        return true;
    }
    
    void checkBalance() const {
        std::cout << "\n=== Account Balance ===" << std::endl;
        std::cout << "Account Number: " << accountNumber << std::endl;
        std::cout << "Account Holder: " << accountHolder << std::endl;
        std::cout << "Current Balance: $" << std::fixed << std::setprecision(2) 
                  << balance << std::endl;
    }
    
    void viewTransactionHistory() const {
        std::cout << "\n=== Transaction History ===" << std::endl;
        std::cout << "Account: " << accountNumber << " (" << accountHolder << ")\n";
        
        if (transactionHistory.empty()) {
            std::cout << "No transactions found.\n";
            return;
        }
        
        std::cout << std::string(70, '-') << std::endl;
        std::cout << std::left << std::setw(12) << "Type" 
                  << std::setw(12) << "Amount" 
                  << std::setw(15) << "Balance" 
                  << "Timestamp" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        for (const auto& transaction : transactionHistory) {
            transaction.display();
        }
        std::cout << std::string(70, '-') << std::endl;
    }
    
    // Getters
    std::string getAccountNumber() const { return accountNumber; }
    std::string getAccountHolder() const { return accountHolder; }
    double getBalance() const { return balance; }
    size_t getTransactionCount() const { return transactionHistory.size(); }
};

class BankingSystem {
private:
    std::vector<BankAccount> accounts;
    
    int findAccountIndex(const std::string& accountNumber) {
        for (size_t i = 0; i < accounts.size(); ++i) {
            if (accounts[i].getAccountNumber() == accountNumber) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    
public:
    void createAccount() {
        std::string accountNumber, accountHolder;
        double initialDeposit;
        
        std::cout << "\n=== Create New Account ===" << std::endl;
        std::cout << "Enter account number: ";
        std::cin >> accountNumber;
        
        // Check if account already exists
        if (findAccountIndex(accountNumber) != -1) {
            std::cout << "Error: Account number already exists!\n";
            return;
        }
        
        std::cin.ignore(); // Clear input buffer
        std::cout << "Enter account holder name: ";
        std::getline(std::cin, accountHolder);
        
        std::cout << "Enter initial deposit (0 or more): $";
        std::cin >> initialDeposit;
        
        if (initialDeposit < 0) {
            std::cout << "Error: Initial deposit cannot be negative!\n";
            return;
        }
        
        accounts.emplace_back(accountNumber, accountHolder, initialDeposit);
        std::cout << "Account created successfully!\n";
        std::cout << "Account Number: " << accountNumber << std::endl;
        std::cout << "Account Holder: " << accountHolder << std::endl;
        std::cout << "Initial Balance: $" << std::fixed << std::setprecision(2) 
                  << initialDeposit << std::endl;
    }
    
    void performDeposit() {
        std::string accountNumber;
        double amount;
        
        std::cout << "\n=== Deposit Money ===" << std::endl;
        std::cout << "Enter account number: ";
        std::cin >> accountNumber;
        
        int index = findAccountIndex(accountNumber);
        if (index == -1) {
            std::cout << "Error: Account not found!\n";
            return;
        }
        
        std::cout << "Enter deposit amount: $";
        std::cin >> amount;
        
        accounts[index].deposit(amount);
    }
    
    void performWithdraw() {
        std::string accountNumber;
        double amount;
        
        std::cout << "\n=== Withdraw Money ===" << std::endl;
        std::cout << "Enter account number: ";
        std::cin >> accountNumber;
        
        int index = findAccountIndex(accountNumber);
        if (index == -1) {
            std::cout << "Error: Account not found!\n";
            return;
        }
        
        std::cout << "Enter withdrawal amount: $";
        std::cin >> amount;
        
        accounts[index].withdraw(amount);
    }
    
    void checkBalance() {
        std::string accountNumber;
        
        std::cout << "\n=== Check Balance ===" << std::endl;
        std::cout << "Enter account number: ";
        std::cin >> accountNumber;
        
        int index = findAccountIndex(accountNumber);
        if (index == -1) {
            std::cout << "Error: Account not found!\n";
            return;
        }
        
        accounts[index].checkBalance();
    }
    
    void viewTransactionHistory() {
        std::string accountNumber;
        
        std::cout << "\n=== Transaction History ===" << std::endl;
        std::cout << "Enter account number: ";
        std::cin >> accountNumber;
        
        int index = findAccountIndex(accountNumber);
        if (index == -1) {
            std::cout << "Error: Account not found!\n";
            return;
        }
        
        accounts[index].viewTransactionHistory();
    }
    
    void listAllAccounts() {
        std::cout << "\n=== All Accounts ===" << std::endl;
        
        if (accounts.empty()) {
            std::cout << "No accounts found.\n";
            return;
        }
        
        std::cout << std::string(60, '-') << std::endl;
        std::cout << std::left << std::setw(15) << "Account #" 
                  << std::setw(25) << "Holder Name" 
                  << std::setw(12) << "Balance" 
                  << "Transactions" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& account : accounts) {
            std::cout << std::left << std::setw(15) << account.getAccountNumber()
                      << std::setw(25) << account.getAccountHolder()
                      << "$" << std::setw(11) << std::fixed << std::setprecision(2) 
                      << account.getBalance()
                      << account.getTransactionCount() << std::endl;
        }
        std::cout << std::string(60, '-') << std::endl;
    }
    
    void displayMenu() {
        std::cout << "\n========== BANKING SYSTEM MENU ==========" << std::endl;
        std::cout << "1. Create New Account" << std::endl;
        std::cout << "2. Deposit Money" << std::endl;
        std::cout << "3. Withdraw Money" << std::endl;
        std::cout << "4. Check Balance" << std::endl;
        std::cout << "5. View Transaction History" << std::endl;
        std::cout << "6. List All Accounts" << std::endl;
        std::cout << "7. Exit" << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << "Enter your choice (1-7): ";
    }
    
    void run() {
        int choice;
        
        std::cout << "Welcome to the Simple Banking System!" << std::endl;
        
        while (true) {
            displayMenu();
            std::cin >> choice;
            
            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "Invalid input! Please enter a number between 1-7.\n";
                continue;
            }
            
            switch (choice) {
                case 1:
                    createAccount();
                    break;
                case 2:
                    performDeposit();
                    break;
                case 3:
                    performWithdraw();
                    break;
                case 4:
                    checkBalance();
                    break;
                case 5:
                    viewTransactionHistory();
                    break;
                case 6:
                    listAllAccounts();
                    break;
                case 7:
                    std::cout << "Thank you for using the Banking System. Goodbye!\n";
                    return;
                default:
                    std::cout << "Invalid choice! Please enter a number between 1-7.\n";
            }
            
            // Pause before showing menu again
            std::cout << "\nPress Enter to continue...";
            std::cin.ignore();
            std::cin.get();
        }
    }
};

int main() {
    BankingSystem bank;
    bank.run();
    return 0;
}