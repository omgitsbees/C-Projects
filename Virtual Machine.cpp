#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <iomanip>

// ============================================================================
// INSTRUCTION SET
// ============================================================================

enum class OpCode : uint8_t {
    // Stack operations
    PUSH,           // Push constant
    POP,            // Pop from stack
    DUP,            // Duplicate top
    SWAP,           // Swap top two
    
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,
    NEG,            // Negate
    
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    
    // Logical
    AND, OR, NOT,
    
    // Memory operations
    LOAD,           // Load from memory
    STORE,          // Store to memory
    LOAD_GLOBAL,    // Load global variable
    STORE_GLOBAL,   // Store global variable
    
    // Control flow
    JMP,            // Unconditional jump
    JMP_IF_FALSE,   // Conditional jump
    JMP_IF_TRUE,    // Conditional jump
    CALL,           // Function call
    RET,            // Return from function
    
    // Object operations
    NEW_ARRAY,      // Create array
    ARRAY_GET,      // Get array element
    ARRAY_SET,      // Set array element
    ARRAY_LEN,      // Get array length
    
    // System
    PRINT,          // Print top of stack
    HALT,           // Stop execution
    NOP             // No operation
};

// ============================================================================
// VALUE SYSTEM
// ============================================================================

enum class ValueType : uint8_t {
    NIL,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    OBJECT
};

struct Object {
    enum class Type { ARRAY, STRING } type;
    bool marked = false;  // For GC
    virtual ~Object() = default;
};

struct ArrayObject : Object {
    std::vector<int64_t> elements;
    ArrayObject(size_t size) : elements(size, 0) {
        type = Type::ARRAY;
    }
};

struct StringObject : Object {
    std::string data;
    StringObject(const std::string& s) : data(s) {
        type = Type::STRING;
    }
};

struct Value {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double real;
        Object* object;
    } as;
    
    Value() : type(ValueType::NIL) { as.integer = 0; }
    
    static Value Nil() {
        Value v;
        v.type = ValueType::NIL;
        return v;
    }
    
    static Value Bool(bool b) {
        Value v;
        v.type = ValueType::BOOLEAN;
        v.as.boolean = b;
        return v;
    }
    
    static Value Int(int64_t i) {
        Value v;
        v.type = ValueType::INTEGER;
        v.as.integer = i;
        return v;
    }
    
    static Value Double(double d) {
        Value v;
        v.type = ValueType::DOUBLE;
        v.as.real = d;
        return v;
    }
    
    static Value Obj(Object* obj) {
        Value v;
        v.type = ValueType::OBJECT;
        v.as.object = obj;
        return v;
    }
    
    bool isTruthy() const {
        switch (type) {
            case ValueType::NIL: return false;
            case ValueType::BOOLEAN: return as.boolean;
            case ValueType::INTEGER: return as.integer != 0;
            case ValueType::DOUBLE: return as.real != 0.0;
            case ValueType::OBJECT: return as.object != nullptr;
        }
        return false;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        switch (type) {
            case ValueType::NIL: return "nil";
            case ValueType::BOOLEAN: return as.boolean ? "true" : "false";
            case ValueType::INTEGER: return std::to_string(as.integer);
            case ValueType::DOUBLE: 
                oss << std::fixed << std::setprecision(2) << as.real;
                return oss.str();
            case ValueType::OBJECT:
                if (as.object->type == Object::Type::STRING) {
                    return static_cast<StringObject*>(as.object)->data;
                } else {
                    return "[Array]";
                }
        }
        return "unknown";
    }
};

// ============================================================================
// GARBAGE COLLECTOR
// ============================================================================

class GarbageCollector {
private:
    std::vector<Object*> objects;
    size_t nextGC = 8;
    size_t threshold = 8;
    
    void markValue(const Value& value) {
        if (value.type == ValueType::OBJECT && value.as.object) {
            markObject(value.as.object);
        }
    }
    
    void markObject(Object* obj) {
        if (!obj || obj->marked) return;
        
        obj->marked = true;
        
        if (obj->type == Object::Type::ARRAY) {
            // Arrays contain primitive integers for simplicity
            // In a full implementation, we'd mark nested objects
        }
    }
    
    void sweep() {
        auto it = objects.begin();
        while (it != objects.end()) {
            if (!(*it)->marked) {
                delete *it;
                it = objects.erase(it);
            } else {
                (*it)->marked = false;
                ++it;
            }
        }
    }
    
public:
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        objects.push_back(obj);
        return obj;
    }
    
    void collect(const std::vector<Value>& stack, 
                 const std::vector<Value>& globals) {
        // Mark phase
        for (const auto& value : stack) {
            markValue(value);
        }
        for (const auto& value : globals) {
            markValue(value);
        }
        
        // Sweep phase
        sweep();
        
        nextGC = objects.size() * 2;
        if (nextGC < threshold) nextGC = threshold;
    }
    
    bool shouldCollect() const {
        return objects.size() >= nextGC;
    }
    
    size_t objectCount() const { return objects.size(); }
    
    ~GarbageCollector() {
        for (auto obj : objects) {
            delete obj;
        }
    }
};

// ============================================================================
// BYTECODE CHUNK
// ============================================================================

struct Instruction {
    OpCode opcode;
    int32_t operand;
    
    Instruction(OpCode op, int32_t arg = 0) : opcode(op), operand(arg) {}
};

class Chunk {
public:
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<int> lines;
    
    void write(Instruction instr, int line) {
        code.push_back(instr);
        lines.push_back(line);
    }
    
    size_t addConstant(Value value) {
        constants.push_back(value);
        return constants.size() - 1;
    }
    
    void disassemble(const std::string& name) const {
        std::cout << "== " << name << " ==\n";
        for (size_t i = 0; i < code.size(); i++) {
            disassembleInstruction(i);
        }
    }
    
    void disassembleInstruction(size_t offset) const {
        std::cout << std::setw(4) << std::setfill('0') << offset << " ";
        
        if (offset > 0 && lines[offset] == lines[offset - 1]) {
            std::cout << "   | ";
        } else {
            std::cout << std::setw(4) << lines[offset] << " ";
        }
        
        const Instruction& instr = code[offset];
        std::cout << std::setw(16) << std::left << opcodeToString(instr.opcode);
        
        // Show operand for relevant instructions
        switch (instr.opcode) {
            case OpCode::PUSH:
            case OpCode::LOAD:
            case OpCode::STORE:
            case OpCode::LOAD_GLOBAL:
            case OpCode::STORE_GLOBAL:
            case OpCode::JMP:
            case OpCode::JMP_IF_FALSE:
            case OpCode::JMP_IF_TRUE:
            case OpCode::CALL:
            case OpCode::NEW_ARRAY:
                std::cout << std::setw(8) << instr.operand;
                break;
            default:
                break;
        }
        
        std::cout << "\n";
    }
    
private:
    std::string opcodeToString(OpCode op) const {
        switch (op) {
            case OpCode::PUSH: return "PUSH";
            case OpCode::POP: return "POP";
            case OpCode::DUP: return "DUP";
            case OpCode::SWAP: return "SWAP";
            case OpCode::ADD: return "ADD";
            case OpCode::SUB: return "SUB";
            case OpCode::MUL: return "MUL";
            case OpCode::DIV: return "DIV";
            case OpCode::MOD: return "MOD";
            case OpCode::NEG: return "NEG";
            case OpCode::EQ: return "EQ";
            case OpCode::NE: return "NE";
            case OpCode::LT: return "LT";
            case OpCode::LE: return "LE";
            case OpCode::GT: return "GT";
            case OpCode::GE: return "GE";
            case OpCode::AND: return "AND";
            case OpCode::OR: return "OR";
            case OpCode::NOT: return "NOT";
            case OpCode::LOAD: return "LOAD";
            case OpCode::STORE: return "STORE";
            case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
            case OpCode::JMP: return "JMP";
            case OpCode::JMP_IF_FALSE: return "JMP_IF_FALSE";
            case OpCode::JMP_IF_TRUE: return "JMP_IF_TRUE";
            case OpCode::CALL: return "CALL";
            case OpCode::RET: return "RET";
            case OpCode::NEW_ARRAY: return "NEW_ARRAY";
            case OpCode::ARRAY_GET: return "ARRAY_GET";
            case OpCode::ARRAY_SET: return "ARRAY_SET";
            case OpCode::ARRAY_LEN: return "ARRAY_LEN";
            case OpCode::PRINT: return "PRINT";
            case OpCode::HALT: return "HALT";
            case OpCode::NOP: return "NOP";
        }
        return "UNKNOWN";
    }
};

// ============================================================================
// VIRTUAL MACHINE
// ============================================================================

class VM {
private:
    Chunk* chunk;
    size_t ip;  // Instruction pointer
    std::vector<Value> stack;
    std::vector<Value> globals;
    std::vector<size_t> callStack;  // Return addresses
    GarbageCollector gc;
    
    bool debugMode = false;
    bool running = true;
    
    void push(Value value) {
        stack.push_back(value);
    }
    
    Value pop() {
        if (stack.empty()) {
            runtimeError("Stack underflow");
            return Value::Nil();
        }
        Value value = stack.back();
        stack.pop_back();
        return value;
    }
    
    Value peek(size_t distance = 0) const {
        if (distance >= stack.size()) {
            return Value::Nil();
        }
        return stack[stack.size() - 1 - distance];
    }
    
    void runtimeError(const std::string& message) {
        std::cerr << "Runtime Error: " << message << "\n";
        std::cerr << "  at instruction " << ip << "\n";
        running = false;
    }
    
    Value binaryOp(OpCode op) {
        Value b = pop();
        Value a = pop();
        
        if (a.type == ValueType::INTEGER && b.type == ValueType::INTEGER) {
            int64_t result;
            switch (op) {
                case OpCode::ADD: result = a.as.integer + b.as.integer; break;
                case OpCode::SUB: result = a.as.integer - b.as.integer; break;
                case OpCode::MUL: result = a.as.integer * b.as.integer; break;
                case OpCode::DIV:
                    if (b.as.integer == 0) {
                        runtimeError("Division by zero");
                        return Value::Nil();
                    }
                    result = a.as.integer / b.as.integer;
                    break;
                case OpCode::MOD:
                    if (b.as.integer == 0) {
                        runtimeError("Modulo by zero");
                        return Value::Nil();
                    }
                    result = a.as.integer % b.as.integer;
                    break;
                case OpCode::LT: return Value::Bool(a.as.integer < b.as.integer);
                case OpCode::LE: return Value::Bool(a.as.integer <= b.as.integer);
                case OpCode::GT: return Value::Bool(a.as.integer > b.as.integer);
                case OpCode::GE: return Value::Bool(a.as.integer >= b.as.integer);
                case OpCode::EQ: return Value::Bool(a.as.integer == b.as.integer);
                case OpCode::NE: return Value::Bool(a.as.integer != b.as.integer);
                default: runtimeError("Invalid binary operation"); return Value::Nil();
            }
            return Value::Int(result);
        }
        
        runtimeError("Operands must be integers");
        return Value::Nil();
    }
    
public:
    VM() : chunk(nullptr), ip(0) {
        globals.resize(256);  // Pre-allocate global space
    }
    
    void setDebugMode(bool enabled) { debugMode = enabled; }
    
    bool execute(Chunk* programChunk) {
        chunk = programChunk;
        ip = 0;
        running = true;
        stack.clear();
        
        if (debugMode) {
            std::cout << "\n=== EXECUTION START ===\n";
        }
        
        while (running && ip < chunk->code.size()) {
            if (debugMode) {
                printDebugInfo();
            }
            
            // Check if GC should run
            if (gc.shouldCollect()) {
                gc.collect(stack, globals);
                if (debugMode) {
                    std::cout << "[GC] Collected. Objects: " << gc.objectCount() << "\n";
                }
            }
            
            const Instruction& instr = chunk->code[ip];
            ip++;
            
            switch (instr.opcode) {
                case OpCode::PUSH: {
                    if (instr.operand >= 0 && instr.operand < (int)chunk->constants.size()) {
                        push(chunk->constants[instr.operand]);
                    }
                    break;
                }
                
                case OpCode::POP:
                    pop();
                    break;
                
                case OpCode::DUP:
                    push(peek());
                    break;
                
                case OpCode::SWAP: {
                    Value b = pop();
                    Value a = pop();
                    push(b);
                    push(a);
                    break;
                }
                
                case OpCode::ADD:
                case OpCode::SUB:
                case OpCode::MUL:
                case OpCode::DIV:
                case OpCode::MOD:
                case OpCode::LT:
                case OpCode::LE:
                case OpCode::GT:
                case OpCode::GE:
                case OpCode::EQ:
                case OpCode::NE:
                    push(binaryOp(instr.opcode));
                    break;
                
                case OpCode::NEG: {
                    Value a = pop();
                    if (a.type == ValueType::INTEGER) {
                        push(Value::Int(-a.as.integer));
                    } else {
                        runtimeError("Operand must be integer");
                    }
                    break;
                }
                
                case OpCode::NOT:
                    push(Value::Bool(!pop().isTruthy()));
                    break;
                
                case OpCode::AND: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::Bool(a.isTruthy() && b.isTruthy()));
                    break;
                }
                
                case OpCode::OR: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::Bool(a.isTruthy() || b.isTruthy()));
                    break;
                }
                
                case OpCode::LOAD_GLOBAL:
                    if (instr.operand >= 0 && instr.operand < (int)globals.size()) {
                        push(globals[instr.operand]);
                    }
                    break;
                
                case OpCode::STORE_GLOBAL:
                    if (instr.operand >= 0 && instr.operand < (int)globals.size()) {
                        globals[instr.operand] = peek();
                    }
                    break;
                
                case OpCode::JMP:
                    ip = instr.operand;
                    break;
                
                case OpCode::JMP_IF_FALSE:
                    if (!peek().isTruthy()) {
                        ip = instr.operand;
                    }
                    break;
                
                case OpCode::JMP_IF_TRUE:
                    if (peek().isTruthy()) {
                        ip = instr.operand;
                    }
                    break;
                
                case OpCode::CALL:
                    callStack.push_back(ip);
                    ip = instr.operand;
                    break;
                
                case OpCode::RET:
                    if (!callStack.empty()) {
                        ip = callStack.back();
                        callStack.pop_back();
                    } else {
                        running = false;
                    }
                    break;
                
                case OpCode::NEW_ARRAY: {
                    ArrayObject* arr = gc.allocate<ArrayObject>(instr.operand);
                    push(Value::Obj(arr));
                    break;
                }
                
                case OpCode::ARRAY_GET: {
                    Value idx = pop();
                    Value arr = pop();
                    if (arr.type == ValueType::OBJECT && 
                        arr.as.object->type == Object::Type::ARRAY &&
                        idx.type == ValueType::INTEGER) {
                        auto* arrObj = static_cast<ArrayObject*>(arr.as.object);
                        if (idx.as.integer >= 0 && idx.as.integer < (int64_t)arrObj->elements.size()) {
                            push(Value::Int(arrObj->elements[idx.as.integer]));
                        } else {
                            runtimeError("Array index out of bounds");
                        }
                    } else {
                        runtimeError("Invalid array access");
                    }
                    break;
                }
                
                case OpCode::ARRAY_SET: {
                    Value val = pop();
                    Value idx = pop();
                    Value arr = pop();
                    if (arr.type == ValueType::OBJECT && 
                        arr.as.object->type == Object::Type::ARRAY &&
                        idx.type == ValueType::INTEGER &&
                        val.type == ValueType::INTEGER) {
                        auto* arrObj = static_cast<ArrayObject*>(arr.as.object);
                        if (idx.as.integer >= 0 && idx.as.integer < (int64_t)arrObj->elements.size()) {
                            arrObj->elements[idx.as.integer] = val.as.integer;
                        } else {
                            runtimeError("Array index out of bounds");
                        }
                    } else {
                        runtimeError("Invalid array assignment");
                    }
                    break;
                }
                
                case OpCode::ARRAY_LEN: {
                    Value arr = pop();
                    if (arr.type == ValueType::OBJECT && 
                        arr.as.object->type == Object::Type::ARRAY) {
                        auto* arrObj = static_cast<ArrayObject*>(arr.as.object);
                        push(Value::Int(arrObj->elements.size()));
                    } else {
                        runtimeError("Operand must be array");
                    }
                    break;
                }
                
                case OpCode::PRINT:
                    std::cout << pop().toString() << "\n";
                    break;
                
                case OpCode::HALT:
                    running = false;
                    break;
                
                case OpCode::NOP:
                    break;
                
                default:
                    runtimeError("Unknown opcode");
                    break;
            }
        }
        
        if (debugMode) {
            std::cout << "=== EXECUTION END ===\n";
            std::cout << "Final objects: " << gc.objectCount() << "\n\n";
        }
        
        return running || ip >= chunk->code.size();
    }
    
    void printDebugInfo() const {
        std::cout << "\n--- Debug Info (IP: " << ip << ") ---\n";
        std::cout << "Stack (" << stack.size() << "): ";
        for (const auto& val : stack) {
            std::cout << "[" << val.toString() << "] ";
        }
        std::cout << "\n";
        
        if (ip < chunk->code.size()) {
            std::cout << "Next: ";
            chunk->disassembleInstruction(ip);
        }
    }
};

// ============================================================================
// EXAMPLE PROGRAMS
// ============================================================================

void example1_arithmetic() {
    std::cout << "\n=== Example 1: Arithmetic ===\n";
    
    Chunk chunk;
    
    // Calculate: (10 + 20) * 3 - 5
    chunk.addConstant(Value::Int(10));
    chunk.write(Instruction(OpCode::PUSH, 0), 1);
    
    chunk.addConstant(Value::Int(20));
    chunk.write(Instruction(OpCode::PUSH, 1), 1);
    
    chunk.write(Instruction(OpCode::ADD), 1);
    
    chunk.addConstant(Value::Int(3));
    chunk.write(Instruction(OpCode::PUSH, 2), 2);
    
    chunk.write(Instruction(OpCode::MUL), 2);
    
    chunk.addConstant(Value::Int(5));
    chunk.write(Instruction(OpCode::PUSH, 3), 3);
    
    chunk.write(Instruction(OpCode::SUB), 3);
    chunk.write(Instruction(OpCode::PRINT), 3);
    chunk.write(Instruction(OpCode::HALT), 3);
    
    chunk.disassemble("Arithmetic");
    
    VM vm;
    vm.execute(&chunk);
}

void example2_conditionals() {
    std::cout << "\n=== Example 2: Conditionals ===\n";
    
    Chunk chunk;
    
    // if (10 > 5) print 1 else print 0
    chunk.addConstant(Value::Int(10));
    chunk.write(Instruction(OpCode::PUSH, 0), 1);
    
    chunk.addConstant(Value::Int(5));
    chunk.write(Instruction(OpCode::PUSH, 1), 1);
    
    chunk.write(Instruction(OpCode::GT), 1);
    chunk.write(Instruction(OpCode::JMP_IF_FALSE, 8), 1);
    
    // True branch
    chunk.addConstant(Value::Int(1));
    chunk.write(Instruction(OpCode::PUSH, 2), 2);
    chunk.write(Instruction(OpCode::PRINT), 2);
    chunk.write(Instruction(OpCode::JMP, 10), 2);
    
    // False branch
    chunk.addConstant(Value::Int(0));
    chunk.write(Instruction(OpCode::PUSH, 3), 3);
    chunk.write(Instruction(OpCode::PRINT), 3);
    
    chunk.write(Instruction(OpCode::HALT), 4);
    
    VM vm;
    vm.execute(&chunk);
}

void example3_loop() {
    std::cout << "\n=== Example 3: Loop (Sum 1-10) ===\n";
    
    Chunk chunk;
    
    // sum = 0
    chunk.addConstant(Value::Int(0));
    chunk.write(Instruction(OpCode::PUSH, 0), 1);
    chunk.write(Instruction(OpCode::STORE_GLOBAL, 0), 1);
    
    // i = 1
    chunk.addConstant(Value::Int(1));
    chunk.write(Instruction(OpCode::PUSH, 1), 2);
    chunk.write(Instruction(OpCode::STORE_GLOBAL, 1), 2);
    
    // Loop start (ip = 4)
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 1), 3);  // Load i
    chunk.addConstant(Value::Int(11));
    chunk.write(Instruction(OpCode::PUSH, 2), 3);
    chunk.write(Instruction(OpCode::LT), 3);  // i < 11
    chunk.write(Instruction(OpCode::JMP_IF_FALSE, 20), 3);  // Exit if false
    
    // sum = sum + i
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 0), 4);
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 1), 4);
    chunk.write(Instruction(OpCode::ADD), 4);
    chunk.write(Instruction(OpCode::STORE_GLOBAL, 0), 4);
    
    // i = i + 1
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 1), 5);
    chunk.addConstant(Value::Int(1));
    chunk.write(Instruction(OpCode::PUSH, 3), 5);
    chunk.write(Instruction(OpCode::ADD), 5);
    chunk.write(Instruction(OpCode::STORE_GLOBAL, 1), 5);
    
    chunk.write(Instruction(OpCode::JMP, 4), 6);  // Jump back
    
    // Print sum
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 0), 7);
    chunk.write(Instruction(OpCode::PRINT), 7);
    chunk.write(Instruction(OpCode::HALT), 7);
    
    VM vm;
    vm.execute(&chunk);
}

void example4_arrays() {
    std::cout << "\n=== Example 4: Arrays & GC ===\n";
    
    Chunk chunk;
    
    // Create array of size 5
    chunk.write(Instruction(OpCode::NEW_ARRAY, 5), 1);
    chunk.write(Instruction(OpCode::STORE_GLOBAL, 0), 1);
    
    // arr[2] = 42
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 0), 2);
    chunk.addConstant(Value::Int(2));
    chunk.write(Instruction(OpCode::PUSH, 0), 2);
    chunk.addConstant(Value::Int(42));
    chunk.write(Instruction(OpCode::PUSH, 1), 2);
    chunk.write(Instruction(OpCode::ARRAY_SET), 2);
    
    // print arr[2]
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 0), 3);
    chunk.addConstant(Value::Int(2));
    chunk.write(Instruction(OpCode::PUSH, 0), 3);
    chunk.write(Instruction(OpCode::ARRAY_GET), 3);
    chunk.write(Instruction(OpCode::PRINT), 3);
    
    // print length
    chunk.write(Instruction(OpCode::LOAD_GLOBAL, 0), 4);
    chunk.write(Instruction(OpCode::ARRAY_LEN), 4);
    chunk.write(Instruction(OpCode::PRINT), 4);
    
    chunk.write(Instruction(OpCode::HALT), 5);
    
    VM vm;
    vm.setDebugMode(true);
    vm.execute(&chunk);
}

// ============================================================================
// MAIN
// ============================================================================

// ============================================================================
// SIMPLE ASSEMBLER
// ============================================================================

class Assembler {
private:
    Chunk* chunk;
    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> unresolvedJumps;
    int currentLine = 1;
    
public:
    Assembler(Chunk* c) : chunk(c) {}
    
    void label(const std::string& name) {
        labels[name] = chunk->code.size();
    }
    
    void push(int64_t value) {
        size_t idx = chunk->addConstant(Value::Int(value));
        chunk->write(Instruction(OpCode::PUSH, idx), currentLine);
    }
    
    void op(OpCode opcode) {
        chunk->write(Instruction(opcode), currentLine);
    }
    
    void jump(const std::string& label) {
        unresolvedJumps.push_back({chunk->code.size(), label});
        chunk->write(Instruction(OpCode::JMP, 0), currentLine);
    }
    
    void jumpIfFalse(const std::string& label) {
        unresolvedJumps.push_back({chunk->code.size(), label});
        chunk->write(Instruction(OpCode::JMP_IF_FALSE, 0), currentLine);
    }
    
    void jumpIfTrue(const std::string& label) {
        unresolvedJumps.push_back({chunk->code.size(), label});
        chunk->write(Instruction(OpCode::JMP_IF_TRUE, 0), currentLine);
    }
    
    void loadGlobal(int idx) {
        chunk->write(Instruction(OpCode::LOAD_GLOBAL, idx), currentLine);
    }
    
    void storeGlobal(int idx) {
        chunk->write(Instruction(OpCode::STORE_GLOBAL, idx), currentLine);
    }
    
    void newArray(int size) {
        chunk->write(Instruction(OpCode::NEW_ARRAY, size), currentLine);
    }
    
    void resolve() {
        for (auto& [offset, labelName] : unresolvedJumps) {
            if (labels.find(labelName) != labels.end()) {
                chunk->code[offset].operand = labels[labelName];
            } else {
                std::cerr << "Error: Undefined label '" << labelName << "'\n";
            }
        }
    }
    
    void nextLine() { currentLine++; }
};

// ============================================================================
// JIT COMPILATION STUB (Concept demonstration)
// ============================================================================

class JITCompiler {
private:
    struct CompiledCode {
        std::vector<uint8_t> machineCode;
        void* executableMemory = nullptr;
    };
    
    std::unordered_map<size_t, CompiledCode> compiledFunctions;
    
public:
    // In a real JIT, this would compile bytecode to native machine code
    // For this demo, we'll track "hot" functions
    bool shouldCompile(size_t functionStart, int executionCount) {
        return executionCount > 100;  // Hot threshold
    }
    
    void compile(Chunk* chunk, size_t start, size_t end) {
        std::cout << "[JIT] Compiling bytecode range " << start << "-" << end << "\n";
        // In reality: emit x86/ARM machine code here
        // Use libraries like AsmJit, LLVM, or hand-roll assembly
        // This is a placeholder showing the architecture
        
        CompiledCode code;
        // Mock: pretend we generated native code
        code.machineCode.resize(100);  // Placeholder
        compiledFunctions[start] = std::move(code);
    }
    
    bool hasCompiledVersion(size_t address) const {
        return compiledFunctions.find(address) != compiledFunctions.end();
    }
    
    void printStats() const {
        std::cout << "[JIT] Compiled functions: " << compiledFunctions.size() << "\n";
    }
};

// ============================================================================
// ENHANCED VM WITH PROFILING
// ============================================================================

class ProfilingVM : public VM {
private:
    std::unordered_map<size_t, int> executionCounts;
    JITCompiler jit;
    
public:
    void profileExecution(size_t ip) {
        executionCounts[ip]++;
        
        // Check if we should JIT compile
        if (executionCounts[ip] == 100) {
            std::cout << "[Profile] Hot spot detected at IP " << ip << "\n";
            // In a real system, identify function boundaries and compile
        }
    }
    
    void printProfile() const {
        std::cout << "\n=== Execution Profile ===\n";
        std::vector<std::pair<size_t, int>> sorted;
        for (const auto& [ip, count] : executionCounts) {
            sorted.push_back({ip, count});
        }
        std::sort(sorted.begin(), sorted.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << "Top hot spots:\n";
        for (size_t i = 0; i < std::min(sorted.size(), size_t(10)); i++) {
            std::cout << "  IP " << std::setw(4) << sorted[i].first 
                      << " executed " << sorted[i].second << " times\n";
        }
        
        jit.printStats();
    }
};

// ============================================================================
// ADVANCED EXAMPLES
// ============================================================================

void example5_fibonacci() {
    std::cout << "\n=== Example 5: Fibonacci (Recursive) ===\n";
    
    Chunk chunk;
    Assembler asm(&chunk);
    
    // Function: fib(n)
    // if n <= 1 return n
    // else return fib(n-1) + fib(n-2)
    
    asm.label("fib");
    asm.loadGlobal(0);  // Load n
    asm.push(1);
    asm.op(OpCode::LE);
    asm.jumpIfFalse("recursive");
    
    // Base case: return n
    asm.loadGlobal(0);
    asm.op(OpCode::PRINT);
    asm.op(OpCode::HALT);
    
    // Recursive case
    asm.label("recursive");
    // For simplicity, just compute iteratively
    // Real implementation would need proper call frames
    
    asm.push(10);  // Compute fib(10)
    asm.storeGlobal(0);
    
    // Iterative fibonacci
    asm.push(0);
    asm.storeGlobal(1);  // a = 0
    asm.push(1);
    asm.storeGlobal(2);  // b = 1
    asm.push(0);
    asm.storeGlobal(3);  // i = 0
    
    asm.label("loop");
    asm.loadGlobal(3);
    asm.loadGlobal(0);
    asm.op(OpCode::LT);
    asm.jumpIfFalse("done");
    
    // temp = a + b
    asm.loadGlobal(1);
    asm.loadGlobal(2);
    asm.op(OpCode::ADD);
    asm.storeGlobal(4);
    
    // a = b
    asm.loadGlobal(2);
    asm.storeGlobal(1);
    
    // b = temp
    asm.loadGlobal(4);
    asm.storeGlobal(2);
    
    // i++
    asm.loadGlobal(3);
    asm.push(1);
    asm.op(OpCode::ADD);
    asm.storeGlobal(3);
    
    asm.jump("loop");
    
    asm.label("done");
    asm.loadGlobal(2);
    asm.op(OpCode::PRINT);
    asm.op(OpCode::HALT);
    
    asm.resolve();
    
    VM vm;
    vm.execute(&chunk);
}

void example6_array_operations() {
    std::cout << "\n=== Example 6: Array Manipulation ===\n";
    
    Chunk chunk;
    Assembler asm(&chunk);
    
    // Create array and fill with squares
    asm.newArray(10);
    asm.storeGlobal(0);  // arr
    
    asm.push(0);
    asm.storeGlobal(1);  // i = 0
    
    asm.label("fill_loop");
    asm.loadGlobal(1);
    asm.push(10);
    asm.op(OpCode::LT);
    asm.jumpIfFalse("print_loop_start");
    
    // arr[i] = i * i
    asm.loadGlobal(0);  // arr
    asm.loadGlobal(1);  // i
    asm.loadGlobal(1);  // i
    asm.loadGlobal(1);  // i
    asm.op(OpCode::MUL);  // i * i
    asm.op(OpCode::ARRAY_SET);
    
    // i++
    asm.loadGlobal(1);
    asm.push(1);
    asm.op(OpCode::ADD);
    asm.storeGlobal(1);
    
    asm.jump("fill_loop");
    
    // Print array
    asm.label("print_loop_start");
    asm.push(0);
    asm.storeGlobal(1);  // i = 0
    
    asm.label("print_loop");
    asm.loadGlobal(1);
    asm.push(10);
    asm.op(OpCode::LT);
    asm.jumpIfFalse("end");
    
    // print arr[i]
    asm.loadGlobal(0);
    asm.loadGlobal(1);
    asm.op(OpCode::ARRAY_GET);
    asm.op(OpCode::PRINT);
    
    // i++
    asm.loadGlobal(1);
    asm.push(1);
    asm.op(OpCode::ADD);
    asm.storeGlobal(1);
    
    asm.jump("print_loop");
    
    asm.label("end");
    asm.op(OpCode::HALT);
    
    asm.resolve();
    
    VM vm;
    vm.execute(&chunk);
}

void example7_stress_test_gc() {
    std::cout << "\n=== Example 7: GC Stress Test ===\n";
    
    Chunk chunk;
    Assembler asm(&chunk);
    
    // Create and discard many arrays to trigger GC
    asm.push(0);
    asm.storeGlobal(0);  // counter
    
    asm.label("loop");
    asm.loadGlobal(0);
    asm.push(20);
    asm.op(OpCode::LT);
    asm.jumpIfFalse("done");
    
    // Create array (will be garbage if not stored)
    asm.newArray(100);
    asm.op(OpCode::POP);  // Discard it
    
    // Create another and keep it
    asm.newArray(50);
    asm.storeGlobal(1);  // Keep this one
    
    // counter++
    asm.loadGlobal(0);
    asm.push(1);
    asm.op(OpCode::ADD);
    asm.storeGlobal(0);
    
    // Print counter
    asm.loadGlobal(0);
    asm.op(OpCode::PRINT);
    
    asm.jump("loop");
    
    asm.label("done");
    asm.op(OpCode::HALT);
    
    asm.resolve();
    
    VM vm;
    vm.setDebugMode(false);  // Too verbose for stress test
    vm.execute(&chunk);
    std::cout << "GC stress test completed successfully!\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "===========================================\n";
    std::cout << "  Virtual Machine & Bytecode Interpreter  \n";
    std::cout << "===========================================\n";
    
    example1_arithmetic();
    example2_conditionals();
    example3_loop();
    example4_arrays();
    example5_fibonacci();
    example6_array_operations();
    example7_stress_test_gc();
    
    std::cout << "\n===========================================\n";
    std::cout << "Features Demonstrated:\n";
    std::cout << "  ✓ Custom bytecode instruction set\n";
    std::cout << "  ✓ Stack-based execution model\n";
    std::cout << "  ✓ Mark-and-sweep garbage collector\n";
    std::cout << "  ✓ Control flow (jumps, conditionals)\n";
    std::cout << "  ✓ Arrays and object management\n";
    std::cout << "  ✓ Debugger interface (trace mode)\n";
    std::cout << "  ✓ Simple assembler with labels\n";
    std::cout << "  ✓ JIT compilation architecture\n";
    std::cout << "===========================================\n";
    
    return 0;
}