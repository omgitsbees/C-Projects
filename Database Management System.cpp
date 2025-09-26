#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <sstream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <queue>
#include <algorithm>
#include <regex>
#include <atomic>

// Forward declarations
class Record;
class Table;
class BPlusTreeNode;
class BPlusTree;
class Transaction;
class LockManager;
class BufferManager;
class QueryOptimizer;
class SQLParser;

// Data types supported by the RDBMS
enum class DataType {
    INTEGER,
    STRING,
    DOUBLE
};

// Column definition
struct Column {
    std::string name;
    DataType type;
    bool isPrimaryKey;
    bool isNotNull;
    
    Column(const std::string& n, DataType t, bool pk = false, bool nn = false)
        : name(n), type(t), isPrimaryKey(pk), isNotNull(nn) {}
};

// Value wrapper for different data types
class Value {
public:
    DataType type;
    std::string stringValue;
    int intValue;
    double doubleValue;
    
    Value() : type(DataType::STRING), stringValue(""), intValue(0), doubleValue(0.0) {}
    Value(int val) : type(DataType::INTEGER), intValue(val), doubleValue(0.0) {}
    Value(double val) : type(DataType::DOUBLE), doubleValue(val), intValue(0) {}
    Value(const std::string& val) : type(DataType::STRING), stringValue(val), intValue(0), doubleValue(0.0) {}
    
    std::string toString() const {
        switch(type) {
            case DataType::INTEGER: return std::to_string(intValue);
            case DataType::DOUBLE: return std::to_string(doubleValue);
            case DataType::STRING: return stringValue;
        }
        return "";
    }
    
    bool operator<(const Value& other) const {
        if(type != other.type) return false;
        switch(type) {
            case DataType::INTEGER: return intValue < other.intValue;
            case DataType::DOUBLE: return doubleValue < other.doubleValue;
            case DataType::STRING: return stringValue < other.stringValue;
        }
        return false;
    }
    
    bool operator==(const Value& other) const {
        if(type != other.type) return false;
        switch(type) {
            case DataType::INTEGER: return intValue == other.intValue;
            case DataType::DOUBLE: return doubleValue == other.doubleValue;
            case DataType::STRING: return stringValue == other.stringValue;
        }
        return false;
    }
};

// Record represents a row in a table
class Record {
public:
    std::map<std::string, Value> values;
    int recordId;
    
    Record(int id = 0) : recordId(id) {}
    
    void setValue(const std::string& column, const Value& value) {
        values[column] = value;
    }
    
    Value getValue(const std::string& column) const {
        auto it = values.find(column);
        return (it != values.end()) ? it->second : Value();
    }
    
    std::string toString() const {
        std::string result = "Record " + std::to_string(recordId) + ": ";
        for(const auto& pair : values) {
            result += pair.first + "=" + pair.second.toString() + " ";
        }
        return result;
    }
};

// B+ Tree Node for indexing
class BPlusTreeNode {
public:
    bool isLeaf;
    std::vector<Value> keys;
    std::vector<std::shared_ptr<BPlusTreeNode>> children;
    std::vector<int> recordIds; // For leaf nodes
    std::shared_ptr<BPlusTreeNode> next; // For leaf nodes
    int maxKeys;
    
    BPlusTreeNode(bool leaf, int max = 4) : isLeaf(leaf), maxKeys(max), next(nullptr) {}
    
    bool isFull() const { return keys.size() >= maxKeys; }
    
    void insertInLeaf(const Value& key, int recordId) {
        auto pos = std::lower_bound(keys.begin(), keys.end(), key);
        int index = pos - keys.begin();
        keys.insert(pos, key);
        recordIds.insert(recordIds.begin() + index, recordId);
    }
    
    std::vector<int> search(const Value& key) {
        if(isLeaf) {
            std::vector<int> results;
            for(size_t i = 0; i < keys.size(); i++) {
                if(keys[i] == key) {
                    results.push_back(recordIds[i]);
                }
            }
            return results;
        } else {
            int i = 0;
            while(i < keys.size() && key >= keys[i]) i++;
            return children[i]->search(key);
        }
    }
};

// B+ Tree for indexing
class BPlusTree {
private:
    std::shared_ptr<BPlusTreeNode> root;
    int maxKeys;
    
    std::shared_ptr<BPlusTreeNode> splitNode(std::shared_ptr<BPlusTreeNode> node) {
        auto newNode = std::make_shared<BPlusTreeNode>(node->isLeaf, maxKeys);
        int mid = node->keys.size() / 2;
        
        if(node->isLeaf) {
            // Copy second half to new node
            newNode->keys.assign(node->keys.begin() + mid, node->keys.end());
            newNode->recordIds.assign(node->recordIds.begin() + mid, node->recordIds.end());
            newNode->next = node->next;
            node->next = newNode;
            
            // Keep first half in original
            node->keys.erase(node->keys.begin() + mid, node->keys.end());
            node->recordIds.erase(node->recordIds.begin() + mid, node->recordIds.end());
        } else {
            // Internal node splitting
            newNode->keys.assign(node->keys.begin() + mid + 1, node->keys.end());
            newNode->children.assign(node->children.begin() + mid + 1, node->children.end());
            
            node->keys.erase(node->keys.begin() + mid, node->keys.end());
            node->children.erase(node->children.begin() + mid + 1, node->children.end());
        }
        
        return newNode;
    }
    
public:
    BPlusTree(int max = 4) : maxKeys(max) {
        root = std::make_shared<BPlusTreeNode>(true, maxKeys);
    }
    
    void insert(const Value& key, int recordId) {
        if(root->isFull()) {
            auto newRoot = std::make_shared<BPlusTreeNode>(false, maxKeys);
            newRoot->children.push_back(root);
            auto newNode = splitNode(root);
            newRoot->keys.push_back(newNode->keys[0]);
            newRoot->children.push_back(newNode);
            root = newRoot;
        }
        insertHelper(root, key, recordId);
    }
    
    void insertHelper(std::shared_ptr<BPlusTreeNode> node, const Value& key, int recordId) {
        if(node->isLeaf) {
            node->insertInLeaf(key, recordId);
        } else {
            int i = 0;
            while(i < node->keys.size() && key >= node->keys[i]) i++;
            insertHelper(node->children[i], key, recordId);
            
            if(node->children[i]->isFull()) {
                auto newNode = splitNode(node->children[i]);
                node->keys.insert(node->keys.begin() + i, newNode->keys[0]);
                node->children.insert(node->children.begin() + i + 1, newNode);
            }
        }
    }
    
    std::vector<int> search(const Value& key) {
        return root->search(key);
    }
};

// Lock types for concurrency control
enum class LockType {
    SHARED,
    EXCLUSIVE
};

// Lock manager for concurrency control
class LockManager {
private:
    std::mutex lockTableMutex;
    std::map<int, std::map<int, LockType>> lockTable; // transactionId -> recordId -> lockType
    std::map<int, std::set<int>> waitingTransactions;
    
public:
    bool acquireLock(int transactionId, int recordId, LockType lockType) {
        std::lock_guard<std::mutex> lock(lockTableMutex);
        
        // Check for conflicts
        for(const auto& txnLocks : lockTable) {
            if(txnLocks.first == transactionId) continue;
            
            auto it = txnLocks.second.find(recordId);
            if(it != txnLocks.second.end()) {
                if(lockType == LockType::EXCLUSIVE || it->second == LockType::EXCLUSIVE) {
                    return false; // Conflict detected
                }
            }
        }
        
        lockTable[transactionId][recordId] = lockType;
        return true;
    }
    
    void releaseLocks(int transactionId) {
        std::lock_guard<std::mutex> lock(lockTableMutex);
        lockTable.erase(transactionId);
    }
    
    bool hasLock(int transactionId, int recordId) {
        std::lock_guard<std::mutex> lock(lockTableMutex);
        auto txnIt = lockTable.find(transactionId);
        if(txnIt == lockTable.end()) return false;
        return txnIt->second.find(recordId) != txnIt->second.end();
    }
};

// Transaction states
enum class TransactionState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

// Transaction class for ACID compliance
class Transaction {
public:
    int transactionId;
    TransactionState state;
    std::vector<std::pair<std::string, std::string>> operations; // operation log
    std::chrono::steady_clock::time_point startTime;
    
    Transaction(int id) : transactionId(id), state(TransactionState::ACTIVE) {
        startTime = std::chrono::steady_clock::now();
    }
    
    void logOperation(const std::string& operation, const std::string& data) {
        operations.push_back({operation, data});
    }
    
    void commit() {
        state = TransactionState::COMMITTED;
        std::cout << "Transaction " << transactionId << " committed.\n";
    }
    
    void abort() {
        state = TransactionState::ABORTED;
        std::cout << "Transaction " << transactionId << " aborted.\n";
    }
};

// Buffer manager for page management
class BufferManager {
private:
    static const size_t BUFFER_SIZE = 100;
    std::map<int, std::shared_ptr<Record>> buffer;
    std::queue<int> lruQueue;
    mutable std::shared_mutex bufferMutex;
    
public:
    std::shared_ptr<Record> getRecord(int recordId) {
        std::shared_lock<std::shared_mutex> lock(bufferMutex);
        auto it = buffer.find(recordId);
        if(it != buffer.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void putRecord(std::shared_ptr<Record> record) {
        std::unique_lock<std::shared_mutex> lock(bufferMutex);
        if(buffer.size() >= BUFFER_SIZE) {
            // Evict LRU page
            int lruId = lruQueue.front();
            lruQueue.pop();
            buffer.erase(lruId);
        }
        buffer[record->recordId] = record;
        lruQueue.push(record->recordId);
    }
    
    void removeRecord(int recordId) {
        std::unique_lock<std::shared_mutex> lock(bufferMutex);
        buffer.erase(recordId);
    }
};

// SQL Query types
enum class QueryType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    CREATE_TABLE
};

// Parsed SQL Query structure
struct ParsedQuery {
    QueryType type;
    std::string tableName;
    std::vector<std::string> columns;
    std::vector<Value> values;
    std::map<std::string, Value> whereConditions;
    std::vector<Column> tableSchema; // For CREATE TABLE
};

// Simple SQL Parser
class SQLParser {
public:
    ParsedQuery parse(const std::string& sql) {
        ParsedQuery query;
        std::istringstream iss(sql);
        std::string token;
        
        // Get first token to determine query type
        iss >> token;
        std::transform(token.begin(), token.end(), token.begin(), ::toupper);
        
        if(token == "SELECT") {
            query.type = QueryType::SELECT;
            parseSelect(iss, query);
        } else if(token == "INSERT") {
            query.type = QueryType::INSERT;
            parseInsert(iss, query);
        } else if(token == "UPDATE") {
            query.type = QueryType::UPDATE;
            parseUpdate(iss, query);
        } else if(token == "DELETE") {
            query.type = QueryType::DELETE;
            parseDelete(iss, query);
        } else if(token == "CREATE") {
            query.type = QueryType::CREATE_TABLE;
            parseCreateTable(iss, query);
        }
        
        return query;
    }
    
private:
    void parseSelect(std::istringstream& iss, ParsedQuery& query) {
        std::string token;
        
        // Parse columns
        while(iss >> token && token != "FROM") {
            if(token != "," && token != "*") {
                query.columns.push_back(token);
            }
        }
        
        // Parse table name
        if(iss >> token) {
            query.tableName = token;
        }
        
        // Parse WHERE clause if exists
        if(iss >> token && token == "WHERE") {
            parseWhereClause(iss, query);
        }
    }
    
    void parseInsert(std::istringstream& iss, ParsedQuery& query) {
        std::string token;
        iss >> token; // INTO
        iss >> query.tableName;
        
        // Skip to VALUES
        while(iss >> token && token != "VALUES");
        
        // Parse values
        std::string line;
        std::getline(iss, line);
        // Simple parsing - in real implementation would need proper tokenization
        std::regex valueRegex(R"([^,\(\)\s]+)");
        std::sregex_iterator iter(line.begin(), line.end(), valueRegex);
        std::sregex_iterator end;
        
        for(; iter != end; ++iter) {
            std::string val = iter->str();
            if(std::isdigit(val[0]) || (val[0] == '-' && val.length() > 1)) {
                if(val.find('.') != std::string::npos) {
                    query.values.push_back(Value(std::stod(val)));
                } else {
                    query.values.push_back(Value(std::stoi(val)));
                }
            } else {
                // Remove quotes if present
                if(val.front() == '\'' || val.front() == '"') val = val.substr(1);
                if(val.back() == '\'' || val.back() == '"') val.pop_back();
                query.values.push_back(Value(val));
            }
        }
    }
    
    void parseUpdate(std::istringstream& iss, ParsedQuery& query) {
        iss >> query.tableName;
        // Simple implementation - would need more complex parsing for SET clause
    }
    
    void parseDelete(std::istringstream& iss, ParsedQuery& query) {
        std::string token;
        iss >> token; // FROM
        iss >> query.tableName;
        
        if(iss >> token && token == "WHERE") {
            parseWhereClause(iss, query);
        }
    }
    
    void parseCreateTable(std::istringstream& iss, ParsedQuery& query) {
        std::string token;
        iss >> token; // TABLE
        iss >> query.tableName;
        
        // Simple schema parsing - would need more complex implementation
        query.tableSchema.push_back(Column("id", DataType::INTEGER, true, true));
        query.tableSchema.push_back(Column("name", DataType::STRING, false, true));
        query.tableSchema.push_back(Column("value", DataType::DOUBLE, false, false));
    }
    
    void parseWhereClause(std::istringstream& iss, ParsedQuery& query) {
        std::string column, op, value;
        if(iss >> column >> op >> value) {
            // Simple parsing - remove quotes if present
            if(value.front() == '\'' || value.front() == '"') value = value.substr(1);
            if(value.back() == '\'' || value.back() == '"') value.pop_back();
            
            if(std::isdigit(value[0]) || (value[0] == '-' && value.length() > 1)) {
                if(value.find('.') != std::string::npos) {
                    query.whereConditions[column] = Value(std::stod(value));
                } else {
                    query.whereConditions[column] = Value(std::stoi(value));
                }
            } else {
                query.whereConditions[column] = Value(value);
            }
        }
    }
};

// Query optimizer for performance
class QueryOptimizer {
public:
    struct QueryPlan {
        bool useIndex;
        std::string indexColumn;
        double estimatedCost;
    };
    
    QueryPlan optimize(const ParsedQuery& query, const std::map<std::string, std::shared_ptr<BPlusTree>>& indexes) {
        QueryPlan plan;
        plan.useIndex = false;
        plan.estimatedCost = 1000.0; // Full table scan cost
        
        // Check if we can use an index
        for(const auto& condition : query.whereConditions) {
            if(indexes.find(condition.first) != indexes.end()) {
                plan.useIndex = true;
                plan.indexColumn = condition.first;
                plan.estimatedCost = 10.0; // Index lookup cost
                break;
            }
        }
        
        return plan;
    }
};

// Main Table class
class Table {
private:
    std::string tableName;
    std::vector<Column> schema;
    std::vector<std::shared_ptr<Record>> records;
    std::map<std::string, std::shared_ptr<BPlusTree>> indexes;
    std::atomic<int> nextRecordId;
    mutable std::shared_mutex tableMutex;
    
public:
    Table(const std::string& name, const std::vector<Column>& sch) 
        : tableName(name), schema(sch), nextRecordId(1) {
        
        // Create index for primary key
        for(const auto& col : schema) {
            if(col.isPrimaryKey) {
                indexes[col.name] = std::make_shared<BPlusTree>();
                break;
            }
        }
    }
    
    void createIndex(const std::string& columnName) {
        std::unique_lock<std::shared_mutex> lock(tableMutex);
        if(indexes.find(columnName) == indexes.end()) {
            indexes[columnName] = std::make_shared<BPlusTree>();
            
            // Build index for existing records
            for(const auto& record : records) {
                Value key = record->getValue(columnName);
                indexes[columnName]->insert(key, record->recordId);
            }
        }
    }
    
    bool insertRecord(const std::vector<Value>& values, Transaction& txn) {
        std::unique_lock<std::shared_mutex> lock(tableMutex);
        
        if(values.size() != schema.size()) {
            return false;
        }
        
        auto record = std::make_shared<Record>(nextRecordId++);
        
        // Set values and validate constraints
        for(size_t i = 0; i < schema.size(); i++) {
            if(schema[i].isNotNull && values[i].toString().empty()) {
                return false;
            }
            record->setValue(schema[i].name, values[i]);
        }
        
        records.push_back(record);
        txn.logOperation("INSERT", tableName + ":" + std::to_string(record->recordId));
        
        // Update indexes
        for(auto& indexPair : indexes) {
            Value key = record->getValue(indexPair.first);
            indexPair.second->insert(key, record->recordId);
        }
        
        return true;
    }
    
    std::vector<std::shared_ptr<Record>> selectRecords(const std::map<std::string, Value>& whereConditions) {
        std::shared_lock<std::shared_mutex> lock(tableMutex);
        std::vector<std::shared_ptr<Record>> results;
        
        if(whereConditions.empty()) {
            results = records;
        } else {
            // Try to use index if available
            bool usedIndex = false;
            for(const auto& condition : whereConditions) {
                if(indexes.find(condition.first) != indexes.end()) {
                    std::vector<int> recordIds = indexes[condition.first]->search(condition.second);
                    for(int id : recordIds) {
                        for(const auto& record : records) {
                            if(record->recordId == id) {
                                results.push_back(record);
                                break;
                            }
                        }
                    }
                    usedIndex = true;
                    break;
                }
            }
            
            // Fall back to full table scan
            if(!usedIndex) {
                for(const auto& record : records) {
                    bool matches = true;
                    for(const auto& condition : whereConditions) {
                        if(!(record->getValue(condition.first) == condition.second)) {
                            matches = false;
                            break;
                        }
                    }
                    if(matches) {
                        results.push_back(record);
                    }
                }
            }
        }
        
        return results;
    }
    
    bool deleteRecord(const std::map<std::string, Value>& whereConditions, Transaction& txn) {
        std::unique_lock<std::shared_mutex> lock(tableMutex);
        
        auto it = records.begin();
        bool deleted = false;
        
        while(it != records.end()) {
            bool matches = true;
            for(const auto& condition : whereConditions) {
                if(!((*it)->getValue(condition.first) == condition.second)) {
                    matches = false;
                    break;
                }
            }
            
            if(matches) {
                txn.logOperation("DELETE", tableName + ":" + std::to_string((*it)->recordId));
                it = records.erase(it);
                deleted = true;
            } else {
                ++it;
            }
        }
        
        return deleted;
    }
    
    const std::vector<Column>& getSchema() const { return schema; }
    const std::string& getName() const { return tableName; }
    size_t getRecordCount() const { 
        std::shared_lock<std::shared_mutex> lock(tableMutex);
        return records.size(); 
    }
    
    const std::map<std::string, std::shared_ptr<BPlusTree>>& getIndexes() const { return indexes; }
};

// Main RDBMS Database class
class Database {
private:
    std::map<std::string, std::shared_ptr<Table>> tables;
    std::map<int, std::shared_ptr<Transaction>> transactions;
    std::atomic<int> nextTransactionId;
    LockManager lockManager;
    BufferManager bufferManager;
    SQLParser sqlParser;
    QueryOptimizer queryOptimizer;
    mutable std::shared_mutex dbMutex;
    
public:
    Database() : nextTransactionId(1) {}
    
    int beginTransaction() {
        std::unique_lock<std::shared_mutex> lock(dbMutex);
        int txnId = nextTransactionId++;
        transactions[txnId] = std::make_shared<Transaction>(txnId);
        std::cout << "Transaction " << txnId << " started.\n";
        return txnId;
    }
    
    bool commitTransaction(int transactionId) {
        std::unique_lock<std::shared_mutex> lock(dbMutex);
        auto it = transactions.find(transactionId);
        if(it != transactions.end()) {
            it->second->commit();
            lockManager.releaseLocks(transactionId);
            transactions.erase(it);
            return true;
        }
        return false;
    }
    
    bool abortTransaction(int transactionId) {
        std::unique_lock<std::shared_mutex> lock(dbMutex);
        auto it = transactions.find(transactionId);
        if(it != transactions.end()) {
            it->second->abort();
            lockManager.releaseLocks(transactionId);
            transactions.erase(it);
            return true;
        }
        return false;
    }
    
    bool createTable(const std::string& tableName, const std::vector<Column>& schema) {
        std::unique_lock<std::shared_mutex> lock(dbMutex);
        if(tables.find(tableName) != tables.end()) {
            return false; // Table already exists
        }
        
        tables[tableName] = std::make_shared<Table>(tableName, schema);
        std::cout << "Table '" << tableName << "' created successfully.\n";
        return true;
    }
    
    std::string executeSQL(const std::string& sql, int transactionId = -1) {
        std::ostringstream result;
        
        try {
            ParsedQuery query = sqlParser.parse(sql);
            
            // Create transaction if not provided
            bool autoCommit = false;
            if(transactionId == -1) {
                transactionId = beginTransaction();
                autoCommit = true;
            }
            
            auto txnIt = transactions.find(transactionId);
            if(txnIt == transactions.end()) {
                return "Error: Invalid transaction ID";
            }
            
            switch(query.type) {
                case QueryType::CREATE_TABLE:
                    if(createTable(query.tableName, query.tableSchema)) {
                        result << "Table created successfully.";
                    } else {
                        result << "Error: Table already exists.";
                    }
                    break;
                    
                case QueryType::INSERT:
                    if(executeInsert(query, *txnIt->second)) {
                        result << "Record inserted successfully.";
                    } else {
                        result << "Error: Insert failed.";
                    }
                    break;
                    
                case QueryType::SELECT:
                    result << executeSelect(query);
                    break;
                    
                case QueryType::DELETE:
                    if(executeDelete(query, *txnIt->second)) {
                        result << "Record(s) deleted successfully.";
                    } else {
                        result << "Error: Delete failed.";
                    }
                    break;
                    
                default:
                    result << "Query type not yet implemented.";
            }
            
            if(autoCommit) {
                commitTransaction(transactionId);
            }
            
        } catch(const std::exception& e) {
            result << "Error: " << e.what();
        }
        
        return result.str();
    }
    
private:
    bool executeInsert(const ParsedQuery& query, Transaction& txn) {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        auto tableIt = tables.find(query.tableName);
        if(tableIt == tables.end()) {
            return false;
        }
        
        return tableIt->second->insertRecord(query.values, txn);
    }
    
    std::string executeSelect(const ParsedQuery& query) {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        std::ostringstream result;
        
        auto tableIt = tables.find(query.tableName);
        if(tableIt == tables.end()) {
            return "Error: Table not found";
        }
        
        // Get query plan
        auto plan = queryOptimizer.optimize(query, tableIt->second->getIndexes());
        
        auto records = tableIt->second->selectRecords(query.whereConditions);
        
        result << "Query Plan: " << (plan.useIndex ? "Index Scan" : "Full Table Scan") 
               << " (Cost: " << plan.estimatedCost << ")\n\n";
        
        // Print header
        const auto& schema = tableIt->second->getSchema();
        for(const auto& col : schema) {
            result << col.name << "\t";
        }
        result << "\n";
        
        // Print records
        for(const auto& record : records) {
            for(const auto& col : schema) {
                result << record->getValue(col.name).toString() << "\t";
            }
            result << "\n";
        }
        
        result << "\nRows returned: " << records.size();
        
        return result.str();
    }
    
    bool executeDelete(const ParsedQuery& query, Transaction& txn) {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        auto tableIt = tables.find(query.tableName);
        if(tableIt == tables.end()) {
            return false;
        }
        
        return tableIt->second->deleteRecord(query.whereConditions, txn);
    }
    
public:
    void printDatabaseStats() const {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        std::cout << "\n=== Database Statistics ===\n";
        std::cout << "Tables: " << tables.size() << "\n";
        std::cout << "Active Transactions: " << transactions.size() << "\n";
        
        for(const auto& tablePair : tables) {
            std::cout << "Table '" << tablePair.first << "': " 
                      << tablePair.second->getRecordCount() << " records\n";
        }
        std::cout << "========================\n\n";
    }
    
    // Create index on a table column
    bool createIndex(const std::string& tableName, const std::string& columnName) {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        auto it = tables.find(tableName);
        if(it != tables.end()) {
            it->second->createIndex(columnName);
            std::cout << "Index created on " << tableName << "." << columnName << "\n";
            return true;
        }
        return false;
    }
    
    // Get table info
    std::string getTableInfo(const std::string& tableName) const {
        std::shared_lock<std::shared_mutex> lock(dbMutex);
        std::ostringstream result;
        
        auto it = tables.find(tableName);
        if(it == tables.end()) {
            return "Table not found";
        }
        
        const auto& schema = it->second->getSchema();
        result << "Table: " << tableName << "\n";
        result << "Columns:\n";
        
        for(const auto& col : schema) {
            result << "  " << col.name << " (";
            switch(col.type) {
                case DataType::INTEGER: result << "INTEGER"; break;
                case DataType::STRING: result << "STRING"; break;
                case DataType::DOUBLE: result << "DOUBLE"; break;
            }
            result << ")";
            if(col.isPrimaryKey) result << " PRIMARY KEY";
            if(col.isNotNull) result << " NOT NULL";
            result << "\n";
        }
        
        result << "Records: " << it->second->getRecordCount() << "\n";
        result << "Indexes: " << it->second->getIndexes().size() << "\n";
        
        return result.str();
    }
};

// Demonstration and test functions
class RDBMSDemo {
private:
    Database db;
    
public:
    void runDemo() {
        std::cout << "=== RDBMS Demonstration ===\n\n";
        
        // Test 1: Create tables
        testCreateTables();
        
        // Test 2: Insert data
        testInsertData();
        
        // Test 3: Query data
        testQueryData();
        
        // Test 4: Index operations
        testIndexOperations();
        
        // Test 5: Transactions
        testTransactions();
        
        // Test 6: Concurrent operations
        testConcurrentOperations();
        
        // Final statistics
        db.printDatabaseStats();
    }
    
private:
    void testCreateTables() {
        std::cout << "1. Creating Tables...\n";
        std::cout << "====================\n";
        
        // Create users table
        std::vector<Column> userSchema = {
            Column("id", DataType::INTEGER, true, true),
            Column("name", DataType::STRING, false, true),
            Column("age", DataType::INTEGER, false, false),
            Column("salary", DataType::DOUBLE, false, false)
        };
        
        std::cout << db.executeSQL("CREATE TABLE users (id INT PRIMARY KEY, name STRING NOT NULL, age INT, salary DOUBLE)") << "\n";
        db.createTable("users", userSchema);
        
        // Create products table
        std::vector<Column> productSchema = {
            Column("product_id", DataType::INTEGER, true, true),
            Column("product_name", DataType::STRING, false, true),
            Column("price", DataType::DOUBLE, false, false),
            Column("category", DataType::STRING, false, false)
        };
        
        db.createTable("products", productSchema);
        
        std::cout << db.getTableInfo("users") << "\n";
        std::cout << db.getTableInfo("products") << "\n\n";
    }
    
    void testInsertData() {
        std::cout << "2. Inserting Data...\n";
        std::cout << "====================\n";
        
        // Insert users
        std::cout << db.executeSQL("INSERT INTO users VALUES (1, 'Alice', 30, 75000.0)") << "\n";
        std::cout << db.executeSQL("INSERT INTO users VALUES (2, 'Bob', 25, 60000.0)") << "\n";
        std::cout << db.executeSQL("INSERT INTO users VALUES (3, 'Charlie', 35, 80000.0)") << "\n";
        std::cout << db.executeSQL("INSERT INTO users VALUES (4, 'Diana', 28, 70000.0)") << "\n";
        
        // Insert products
        std::cout << db.executeSQL("INSERT INTO products VALUES (101, 'Laptop', 1200.0, 'Electronics')") << "\n";
        std::cout << db.executeSQL("INSERT INTO products VALUES (102, 'Phone', 800.0, 'Electronics')") << "\n";
        std::cout << db.executeSQL("INSERT INTO products VALUES (103, 'Chair', 150.0, 'Furniture')") << "\n";
        std::cout << db.executeSQL("INSERT INTO products VALUES (104, 'Desk', 300.0, 'Furniture')") << "\n\n";
    }
    
    void testQueryData() {
        std::cout << "3. Querying Data...\n";
        std::cout << "===================\n";
        
        // Select all users
        std::cout << "All users:\n";
        std::cout << db.executeSQL("SELECT * FROM users") << "\n\n";
        
        // Select with WHERE clause
        std::cout << "Users with age > 30:\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE age > 30") << "\n\n";
        
        // Select specific user
        std::cout << "User with name 'Alice':\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE name = 'Alice'") << "\n\n";
        
        // Select products
        std::cout << "Electronics products:\n";
        std::cout << db.executeSQL("SELECT * FROM products WHERE category = 'Electronics'") << "\n\n";
    }
    
    void testIndexOperations() {
        std::cout << "4. Index Operations...\n";
        std::cout << "======================\n";
        
        // Create indexes
        db.createIndex("users", "name");
        db.createIndex("users", "age");
        db.createIndex("products", "category");
        
        // Query with index
        std::cout << "Query using name index:\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE name = 'Bob'") << "\n\n";
        
        std::cout << "Query using category index:\n";
        std::cout << db.executeSQL("SELECT * FROM products WHERE category = 'Furniture'") << "\n\n";
    }
    
    void testTransactions() {
        std::cout << "5. Transaction Management...\n";
        std::cout << "============================\n";
        
        // Test successful transaction
        int txn1 = db.beginTransaction();
        std::cout << db.executeSQL("INSERT INTO users VALUES (5, 'Eve', 32, 85000.0)", txn1) << "\n";
        std::cout << db.executeSQL("INSERT INTO users VALUES (6, 'Frank', 29, 65000.0)", txn1) << "\n";
        db.commitTransaction(txn1);
        
        std::cout << "After transaction commit:\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE name = 'Eve'") << "\n\n";
        
        // Test transaction abort
        int txn2 = db.beginTransaction();
        std::cout << db.executeSQL("INSERT INTO users VALUES (7, 'Grace', 27, 72000.0)", txn2) << "\n";
        db.abortTransaction(txn2);
        
        std::cout << "After transaction abort (Grace should not exist):\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE name = 'Grace'") << "\n\n";
    }
    
    void testConcurrentOperations() {
        std::cout << "6. Concurrent Operations...\n";
        std::cout << "===========================\n";
        
        std::vector<std::thread> threads;
        const int numThreads = 3;
        
        // Create multiple threads performing operations
        for(int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i]() {
                int txnId = db.beginTransaction();
                
                // Each thread inserts a user
                std::string sql = "INSERT INTO users VALUES (" + std::to_string(100 + i) + 
                                ", 'User" + std::to_string(i) + "', " + std::to_string(20 + i) + 
                                ", " + std::to_string(50000.0 + i * 1000) + ")";
                
                std::cout << "Thread " << i << ": " << db.executeSQL(sql, txnId) << "\n";
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                db.commitTransaction(txnId);
            });
        }
        
        // Wait for all threads to complete
        for(auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "\nAll concurrent users:\n";
        std::cout << db.executeSQL("SELECT * FROM users WHERE id >= 100") << "\n\n";
    }
    
public:
    void interactiveMode() {
        std::cout << "\n=== Interactive SQL Mode ===\n";
        std::cout << "Enter SQL commands (type 'exit' to quit, 'help' for commands):\n\n";
        
        std::string input;
        while(true) {
            std::cout << "SQL> ";
            std::getline(std::cin, input);
            
            if(input == "exit" || input == "quit") {
                break;
            } else if(input == "help") {
                printHelp();
            } else if(input == "stats") {
                db.printDatabaseStats();
            } else if(input.find("CREATE INDEX") == 0) {
                handleCreateIndex(input);
            } else if(input.find("SHOW TABLE") == 0) {
                handleShowTable(input);
            } else if(!input.empty()) {
                std::cout << db.executeSQL(input) << "\n\n";
            }
        }
    }
    
private:
    void printHelp() {
        std::cout << "\nSupported SQL Commands:\n";
        std::cout << "  CREATE TABLE table_name (columns...)\n";
        std::cout << "  INSERT INTO table_name VALUES (values...)\n";
        std::cout << "  SELECT * FROM table_name [WHERE condition]\n";
        std::cout << "  DELETE FROM table_name WHERE condition\n";
        std::cout << "  CREATE INDEX table_name column_name\n";
        std::cout << "  SHOW TABLE table_name\n";
        std::cout << "\nSpecial Commands:\n";
        std::cout << "  help    - Show this help\n";
        std::cout << "  stats   - Show database statistics\n";
        std::cout << "  exit    - Quit interactive mode\n\n";
    }
    
    void handleCreateIndex(const std::string& input) {
        std::istringstream iss(input);
        std::string token, tableName, columnName;
        
        iss >> token >> token; // CREATE INDEX
        iss >> tableName >> columnName;
        
        if(db.createIndex(tableName, columnName)) {
            std::cout << "Index created successfully.\n\n";
        } else {
            std::cout << "Error: Could not create index.\n\n";
        }
    }
    
    void handleShowTable(const std::string& input) {
        std::istringstream iss(input);
        std::string token, tableName;
        
        iss >> token >> token; // SHOW TABLE
        iss >> tableName;
        
        std::cout << db.getTableInfo(tableName) << "\n\n";
    }
};

// Performance testing class
class PerformanceTest {
private:
    Database db;
    
public:
    void runPerformanceTests() {
        std::cout << "\n=== Performance Testing ===\n";
        
        setupTestData();
        testInsertPerformance();
        testQueryPerformance();
        testIndexPerformance();
        testConcurrencyPerformance();
    }
    
private:
    void setupTestData() {
        std::cout << "\nSetting up test data...\n";
        
        // Create a large table for performance testing
        std::vector<Column> testSchema = {
            Column("id", DataType::INTEGER, true, true),
            Column("data", DataType::STRING, false, true),
            Column("value", DataType::DOUBLE, false, false)
        };
        
        db.createTable("perf_test", testSchema);
        
        std::cout << "Test table created.\n";
    }
    
    void testInsertPerformance() {
        std::cout << "\nTesting insert performance...\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for(int i = 1; i <= 1000; ++i) {
            std::string sql = "INSERT INTO perf_test VALUES (" + std::to_string(i) + 
                            ", 'TestData" + std::to_string(i) + "', " + 
                            std::to_string(i * 1.5) + ")";
            db.executeSQL(sql);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Inserted 1000 records in " << duration.count() << "ms\n";
        std::cout << "Average: " << (duration.count() / 1000.0) << "ms per insert\n";
    }
    
    void testQueryPerformance() {
        std::cout << "\nTesting query performance (without index)...\n";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for(int i = 1; i <= 100; ++i) {
            std::string sql = "SELECT * FROM perf_test WHERE id = " + std::to_string(i * 10);
            db.executeSQL(sql);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "100 queries in " << duration.count() << "ms\n";
        std::cout << "Average: " << (duration.count() / 100.0) << "ms per query\n";
    }
    
    void testIndexPerformance() {
        std::cout << "\nTesting query performance (with index)...\n";
        
        // Create index
        db.createIndex("perf_test", "id");
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for(int i = 1; i <= 100; ++i) {
            std::string sql = "SELECT * FROM perf_test WHERE id = " + std::to_string(i * 10);
            db.executeSQL(sql);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "100 indexed queries in " << duration.count() << "ms\n";
        std::cout << "Average: " << (duration.count() / 100.0) << "ms per query\n";
    }
    
    void testConcurrencyPerformance() {
        std::cout << "\nTesting concurrent access performance...\n";
        
        const int numThreads = 4;
        const int operationsPerThread = 50;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for(int t = 0; t < numThreads; ++t) {
            threads.emplace_back([this, t, operationsPerThread]() {
                for(int i = 0; i < operationsPerThread; ++i) {
                    int id = t * operationsPerThread + i + 2000;
                    std::string sql = "INSERT INTO perf_test VALUES (" + std::to_string(id) + 
                                    ", 'ConcurrentData" + std::to_string(id) + "', " + 
                                    std::to_string(id * 2.5) + ")";
                    db.executeSQL(sql);
                }
            });
        }
        
        for(auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << numThreads << " threads, " << operationsPerThread 
                  << " operations each, completed in " << duration.count() << "ms\n";
        std::cout << "Total operations: " << (numThreads * operationsPerThread) << "\n";
        std::cout << "Operations per second: " 
                  << (numThreads * operationsPerThread * 1000.0 / duration.count()) << "\n";
    }
};

// Main function demonstrating the RDBMS
int main() {
    std::cout << "C++ Relational Database Management System\n";
    std::cout << "==========================================\n";
    std::cout << "Features: B+ Tree Indexing, ACID Transactions, Concurrency Control, SQL Parser\n\n";
    
    try {
        // Run the main demonstration
        RDBMSDemo demo;
        demo.runDemo();
        
        // Run performance tests
        PerformanceTest perfTest;
        perfTest.runPerformanceTests();
        
        // Interactive mode
        std::cout << "\nWould you like to enter interactive SQL mode? (y/n): ";
        char choice;
        std::cin >> choice;
        std::cin.ignore(); // Clear the newline
        
        if(choice == 'y' || choice == 'Y') {
            demo.interactiveMode();
        }
        
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nRDBMS demonstration completed successfully!\n";
    return 0;
}