#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <filesystem>
 
// Data types supported by our database
enum class DataType {
    INTEGER,
    STRING,
    DOUBLE
};

// Column definition
struct Column {
    std::string name;
    DataType type;
    
    Column(const std::string& n, DataType t) : name(n), type(t) {}
};

// Value wrapper for different data types
class Value {
private:
    DataType type;
    std::string stringValue;
    int intValue;
    double doubleValue;

public:
    Value() : type(DataType::STRING), stringValue(""), intValue(0), doubleValue(0.0) {}
    
    Value(const std::string& val) : type(DataType::STRING), stringValue(val), intValue(0), doubleValue(0.0) {}
    Value(int val) : type(DataType::INTEGER), stringValue(""), intValue(val), doubleValue(0.0) {}
    Value(double val) : type(DataType::DOUBLE), stringValue(""), intValue(0), doubleValue(val) {}
    
    DataType getType() const { return type; }
    
    std::string asString() const {
        switch (type) {
            case DataType::STRING: return stringValue;
            case DataType::INTEGER: return std::to_string(intValue);
            case DataType::DOUBLE: return std::to_string(doubleValue);
        }
        return "";
    }
    
    int asInt() const {
        switch (type) {
            case DataType::INTEGER: return intValue;
            case DataType::STRING: return std::stoi(stringValue);
            case DataType::DOUBLE: return static_cast<int>(doubleValue);
        }
        return 0;
    }
    
    double asDouble() const {
        switch (type) {
            case DataType::DOUBLE: return doubleValue;
            case DataType::INTEGER: return static_cast<double>(intValue);
            case DataType::STRING: return std::stod(stringValue);
        }
        return 0.0;
    }
    
    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        switch (type) {
            case DataType::STRING: return stringValue == other.stringValue;
            case DataType::INTEGER: return intValue == other.intValue;
            case DataType::DOUBLE: return doubleValue == other.doubleValue;
        }
        return false;
    }
};

// Record (row) in a table
using Record = std::vector<Value>;

// Table class
class Table {
private:
    std::string tableName;
    std::vector<Column> columns;
    std::vector<Record> records;
    std::string dataDir;

public:
    Table(const std::string& name, const std::string& dir = "data/") 
        : tableName(name), dataDir(dir) {
        loadFromFile();
    }
    
    void addColumn(const std::string& name, DataType type) {
        columns.emplace_back(name, type);
    }
    
    bool insert(const Record& record) {
        if (record.size() != columns.size()) {
            std::cout << "Error: Record size doesn't match table schema\n";
            return false;
        }
        
        // Validate data types
        for (size_t i = 0; i < record.size(); ++i) {
            if (record[i].getType() != columns[i].type) {
                std::cout << "Error: Data type mismatch in column " << columns[i].name << "\n";
                return false;
            }
        }
        
        records.push_back(record);
        saveToFile();
        return true;
    }
    
    bool update(const std::string& columnName, const Value& oldValue, const Value& newValue) {
        int columnIndex = getColumnIndex(columnName);
        if (columnIndex == -1) {
            std::cout << "Error: Column " << columnName << " not found\n";
            return false;
        }
        
        bool updated = false;
        for (auto& record : records) {
            if (record[columnIndex] == oldValue) {
                if (newValue.getType() != columns[columnIndex].type) {
                    std::cout << "Error: Data type mismatch\n";
                    return false;
                }
                record[columnIndex] = newValue;
                updated = true;
            }
        }
        
        if (updated) {
            saveToFile();
            std::cout << "Records updated successfully\n";
        } else {
            std::cout << "No records matched the condition\n";
        }
        
        return updated;
    }
    
    bool deleteRecords(const std::string& columnName, const Value& value) {
        int columnIndex = getColumnIndex(columnName);
        if (columnIndex == -1) {
            std::cout << "Error: Column " << columnName << " not found\n";
            return false;
        }
        
        auto originalSize = records.size();
        records.erase(
            std::remove_if(records.begin(), records.end(),
                [columnIndex, &value](const Record& record) {
                    return record[columnIndex] == value;
                }),
            records.end()
        );
        
        bool deleted = records.size() < originalSize;
        if (deleted) {
            saveToFile();
            std::cout << "Records deleted successfully\n";
        } else {
            std::cout << "No records matched the condition\n";
        }
        
        return deleted;
    }
    
    void select(const std::vector<std::string>& selectColumns = {}, 
                const std::string& whereColumn = "", 
                const Value& whereValue = Value()) const {
        
        std::vector<int> columnIndices;
        
        // If no columns specified, select all
        if (selectColumns.empty()) {
            for (size_t i = 0; i < columns.size(); ++i) {
                columnIndices.push_back(i);
            }
        } else {
            for (const auto& colName : selectColumns) {
                int index = getColumnIndex(colName);
                if (index == -1) {
                    std::cout << "Error: Column " << colName << " not found\n";
                    return;
                }
                columnIndices.push_back(index);
            }
        }
        
        // Print header
        for (size_t i = 0; i < columnIndices.size(); ++i) {
            std::cout << columns[columnIndices[i]].name;
            if (i < columnIndices.size() - 1) std::cout << "\t";
        }
        std::cout << "\n" << std::string(40, '-') << "\n";
        
        // Print records
        int whereColumnIndex = whereColumn.empty() ? -1 : getColumnIndex(whereColumn);
        
        for (const auto& record : records) {
            // Apply WHERE condition if specified
            if (whereColumnIndex != -1 && !(record[whereColumnIndex] == whereValue)) {
                continue;
            }
            
            for (size_t i = 0; i < columnIndices.size(); ++i) {
                std::cout << record[columnIndices[i]].asString();
                if (i < columnIndices.size() - 1) std::cout << "\t";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    
    void showSchema() const {
        std::cout << "Table: " << tableName << "\n";
        std::cout << "Columns:\n";
        for (const auto& col : columns) {
            std::cout << "  " << col.name << " (";
            switch (col.type) {
                case DataType::INTEGER: std::cout << "INTEGER"; break;
                case DataType::STRING: std::cout << "STRING"; break;
                case DataType::DOUBLE: std::cout << "DOUBLE"; break;
            }
            std::cout << ")\n";
        }
        std::cout << "Records: " << records.size() << "\n\n";
    }

private:
    int getColumnIndex(const std::string& columnName) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i].name == columnName) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    
    void saveToFile() {
        std::filesystem::create_directories(dataDir);
        std::ofstream file(dataDir + tableName + ".db");
        
        if (!file.is_open()) {
            std::cout << "Error: Could not open file for writing\n";
            return;
        }
        
        // Save schema
        file << columns.size() << "\n";
        for (const auto& col : columns) {
            file << col.name << " " << static_cast<int>(col.type) << "\n";
        }
        
        // Save records
        file << records.size() << "\n";
        for (const auto& record : records) {
            for (size_t i = 0; i < record.size(); ++i) {
                file << record[i].asString();
                if (i < record.size() - 1) file << "|";
            }
            file << "\n";
        }
        
        file.close();
    }
    
    void loadFromFile() {
        std::ifstream file(dataDir + tableName + ".db");
        
        if (!file.is_open()) {
            return; // File doesn't exist, new table
        }
        
        // Load schema
        size_t numColumns;
        file >> numColumns;
        
        columns.clear();
        for (size_t i = 0; i < numColumns; ++i) {
            std::string colName;
            int typeInt;
            file >> colName >> typeInt;
            columns.emplace_back(colName, static_cast<DataType>(typeInt));
        }
        
        // Load records
        size_t numRecords;
        file >> numRecords;
        file.ignore(); // Skip newline
        
        records.clear();
        for (size_t i = 0; i < numRecords; ++i) {
            std::string line;
            std::getline(file, line);
            
            Record record;
            std::stringstream ss(line);
            std::string value;
            
            size_t colIndex = 0;
            while (std::getline(ss, value, '|') && colIndex < columns.size()) {
                switch (columns[colIndex].type) {
                    case DataType::STRING:
                        record.emplace_back(value);
                        break;
                    case DataType::INTEGER:
                        record.emplace_back(std::stoi(value));
                        break;
                    case DataType::DOUBLE:
                        record.emplace_back(std::stod(value));
                        break;
                }
                colIndex++;
            }
            
            if (record.size() == columns.size()) {
                records.push_back(record);
            }
        }
        
        file.close();
    }
};

// Database Engine class
class DatabaseEngine {
private:
    std::map<std::string, Table*> tables;
    std::string dataDir;

public:
    DatabaseEngine(const std::string& dir = "data/") : dataDir(dir) {}
    
    ~DatabaseEngine() {
        for (auto& pair : tables) {
            delete pair.second;
        }
    }
    
    bool createTable(const std::string& tableName, const std::vector<std::pair<std::string, DataType>>& schema) {
        if (tables.find(tableName) != tables.end()) {
            std::cout << "Error: Table " << tableName << " already exists\n";
            return false;
        }
        
        Table* table = new Table(tableName, dataDir);
        for (const auto& col : schema) {
            table->addColumn(col.first, col.second);
        }
        
        tables[tableName] = table;
        std::cout << "Table " << tableName << " created successfully\n";
        return true;
    }
    
    Table* getTable(const std::string& tableName) {
        auto it = tables.find(tableName);
        if (it != tables.end()) {
            return it->second;
        }
        
        // Try to load from file
        Table* table = new Table(tableName, dataDir);
        tables[tableName] = table;
        return table;
    }
    
    void listTables() const {
        std::cout << "Available tables:\n";
        for (const auto& pair : tables) {
            std::cout << "  " << pair.first << "\n";
        }
        std::cout << "\n";
    }
    
    bool dropTable(const std::string& tableName) {
        auto it = tables.find(tableName);
        if (it != tables.end()) {
            delete it->second;
            tables.erase(it);
        }
        
        // Remove file
        std::string filename = dataDir + tableName + ".db";
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
            std::cout << "Table " << tableName << " dropped successfully\n";
            return true;
        }
        
        std::cout << "Table " << tableName << " not found\n";
        return false;
    }
};

// Helper function to parse data type from string
DataType parseDataType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "int" || lower == "integer") return DataType::INTEGER;
    if (lower == "double" || lower == "float") return DataType::DOUBLE;
    return DataType::STRING;
}

// Demo function
void runDemo() {
    DatabaseEngine db;
    
    std::cout << "=== Simple Database Engine Demo ===\n\n";
    
    // Create a table
    std::vector<std::pair<std::string, DataType>> schema = {
        {"id", DataType::INTEGER},
        {"name", DataType::STRING},
        {"age", DataType::INTEGER},
        {"salary", DataType::DOUBLE}
    };
    
    db.createTable("employees", schema);
    
    Table* empTable = db.getTable("employees");
    empTable->showSchema();
    
    // Insert some records
    std::cout << "Inserting records...\n";
    empTable->insert({Value(1), Value("Alice Johnson"), Value(30), Value(75000.0)});
    empTable->insert({Value(2), Value("Bob Smith"), Value(25), Value(60000.0)});
    empTable->insert({Value(3), Value("Carol Davis"), Value(35), Value(85000.0)});
    empTable->insert({Value(4), Value("David Wilson"), Value(28), Value(70000.0)});
    
    // Select all records
    std::cout << "All employees:\n";
    empTable->select();
    
    // Select specific columns
    std::cout << "Names and salaries:\n";
    empTable->select({"name", "salary"});
    
    // Select with WHERE clause
    std::cout << "Employees with age 30:\n";
    empTable->select({}, "age", Value(30));
    
    // Update a record
    std::cout << "Updating Bob's salary...\n";
    empTable->update("name", Value("Bob Smith"), Value("Bob Smith"));
    empTable->update("salary", Value(60000.0), Value(65000.0));
    
    std::cout << "After update:\n";
    empTable->select({"name", "salary"});
    
    // Delete a record
    std::cout << "Deleting employee with id 3...\n";
    empTable->deleteRecords("id", Value(3));
    
    std::cout << "After deletion:\n";
    empTable->select();
    
    // Create another table
    std::vector<std::pair<std::string, DataType>> deptSchema = {
        {"dept_id", DataType::INTEGER},
        {"dept_name", DataType::STRING},
        {"budget", DataType::DOUBLE}
    };
    
    db.createTable("departments", deptSchema);
    Table* deptTable = db.getTable("departments");
    
    deptTable->insert({Value(1), Value("Engineering"), Value(500000.0)});
    deptTable->insert({Value(2), Value("Marketing"), Value(200000.0)});
    deptTable->insert({Value(3), Value("HR"), Value(150000.0)});
    
    std::cout << "Departments:\n";
    deptTable->select();
    
    db.listTables();
}

int main() {
    try {
        runDemo();
        
        std::cout << "\n=== Interactive Mode ===\n";
        std::cout << "Commands: create, insert, select, update, delete, schema, list, drop, quit\n\n";
        
        DatabaseEngine db;
        std::string command;
        
        while (true) {
            std::cout << "db> ";
            std::cin >> command;
            std::transform(command.begin(), command.end(), command.begin(), ::tolower);
            
            if (command == "quit" || command == "exit") {
                break;
            }
            else if (command == "create") {
                std::string tableName;
                int numColumns;
                std::cout << "Table name: ";
                std::cin >> tableName;
                std::cout << "Number of columns: ";
                std::cin >> numColumns;
                
                std::vector<std::pair<std::string, DataType>> schema;
                for (int i = 0; i < numColumns; ++i) {
                    std::string colName, typeStr;
                    std::cout << "Column " << (i + 1) << " name: ";
                    std::cin >> colName;
                    std::cout << "Column " << (i + 1) << " type (string/int/double): ";
                    std::cin >> typeStr;
                    schema.emplace_back(colName, parseDataType(typeStr));
                }
                
                db.createTable(tableName, schema);
            }
            else if (command == "list") {
                db.listTables();
            }
            else if (command == "schema") {
                std::string tableName;
                std::cout << "Table name: ";
                std::cin >> tableName;
                db.getTable(tableName)->showSchema();
            }
            else if (command == "select") {
                std::string tableName;
                std::cout << "Table name: ";
                std::cin >> tableName;
                db.getTable(tableName)->select();
            }
            else {
                std::cout << "Command not implemented in interactive mode. Use the demo to see all features.\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;

}
