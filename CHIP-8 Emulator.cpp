#include "chip8.hpp"
#include <iostream>
#include <random>
#include <cstring>
#include <iomanip>

// CHIP-8 fontset (0-F, 5 bytes each)
static const uint8_t FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

Chip8::Chip8() {
    reset();
}

void Chip8::reset() {
    memory.fill(0);
    V.fill(0);
    stack.fill(0);
    display.fill(0);
    keys.fill(false);
    
    I = 0;
    pc = PROGRAM_START_ADDR;
    sp = 0;
    delayTimer = 0;
    soundTimer = 0;
    drawFlag = true;
    waitingForKey = false;
    keyRegister = 0;
    
    loadFontset();
}

void Chip8::loadFontset() {
    for (int i = 0; i < 80; ++i) {
        memory[FONT_START_ADDR + i] = FONTSET[i];
    }
}

bool Chip8::loadROM(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM: " << filepath << std::endl;
        return false;
    }
    
    std::streamsize size = file.tellg();
    if (size > (MEMORY_SIZE - PROGRAM_START_ADDR)) {
        std::cerr << "ROM too large: " << size << " bytes" << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(&memory[PROGRAM_START_ADDR]), size);
    file.close();
    
    std::cout << "Loaded ROM: " << filepath << " (" << size << " bytes)" << std::endl;
    return true;
}

void Chip8::emulateCycle() {
    // Handle waiting for key press
    if (waitingForKey) {
        for (int i = 0; i < KEY_COUNT; ++i) {
            if (keys[i]) {
                V[keyRegister] = i;
                waitingForKey = false;
                break;
            }
        }
        return;
    }
    
    // Fetch opcode
    uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
    
    // Execute opcode
    executeOpcode(opcode);
}

void Chip8::updateTimers() {
    if (delayTimer > 0) {
        --delayTimer;
    }
    
    if (soundTimer > 0) {
        --soundTimer;
    }
}

void Chip8::setKey(uint8_t key, bool pressed) {
    if (key < KEY_COUNT) {
        keys[key] = pressed;
    }
}

void Chip8::executeOpcode(uint16_t opcode) {
    // Extract common nibbles
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    uint8_t n = opcode & 0x000F;
    uint16_t nnn = opcode & 0x0FFF;
    
    // Increment PC (some instructions will modify it)
    pc += 2;
    
    // Decode and execute
    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0: op_00E0(); break;
                case 0x00EE: op_00EE(); break;
                default:
                    std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            }
            break;
        case 0x1000: op_1nnn(nnn); break;
        case 0x2000: op_2nnn(nnn); break;
        case 0x3000: op_3xkk(x, kk); break;
        case 0x4000: op_4xkk(x, kk); break;
        case 0x5000: op_5xy0(x, y); break;
        case 0x6000: op_6xkk(x, kk); break;
        case 0x7000: op_7xkk(x, kk); break;
        case 0x8000:
            switch (opcode & 0x000F) {
                case 0x0: op_8xy0(x, y); break;
                case 0x1: op_8xy1(x, y); break;
                case 0x2: op_8xy2(x, y); break;
                case 0x3: op_8xy3(x, y); break;
                case 0x4: op_8xy4(x, y); break;
                case 0x5: op_8xy5(x, y); break;
                case 0x6: op_8xy6(x, y); break;
                case 0x7: op_8xy7(x, y); break;
                case 0xE: op_8xyE(x, y); break;
                default:
                    std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            }
            break;
        case 0x9000: op_9xy0(x, y); break;
        case 0xA000: op_Annn(nnn); break;
        case 0xB000: op_Bnnn(nnn); break;
        case 0xC000: op_Cxkk(x, kk); break;
        case 0xD000: op_Dxyn(x, y, n); break;
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x9E: op_Ex9E(x); break;
                case 0xA1: op_ExA1(x); break;
                default:
                    std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x07: op_Fx07(x); break;
                case 0x0A: op_Fx0A(x); break;
                case 0x15: op_Fx15(x); break;
                case 0x18: op_Fx18(x); break;
                case 0x1E: op_Fx1E(x); break;
                case 0x29: op_Fx29(x); break;
                case 0x33: op_Fx33(x); break;
                case 0x55: op_Fx55(x); break;
                case 0x65: op_Fx65(x); break;
                default:
                    std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            }
            break;
        default:
            std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
    }
}

// Opcode implementations
void Chip8::op_00E0() { display.fill(0); drawFlag = true; }
void Chip8::op_00EE() { pc = stack[--sp]; }
void Chip8::op_1nnn(uint16_t addr) { pc = addr; }
void Chip8::op_2nnn(uint16_t addr) { stack[sp++] = pc; pc = addr; }
void Chip8::op_3xkk(uint8_t x, uint8_t kk) { if (V[x] == kk) pc += 2; }
void Chip8::op_4xkk(uint8_t x, uint8_t kk) { if (V[x] != kk) pc += 2; }
void Chip8::op_5xy0(uint8_t x, uint8_t y) { if (V[x] == V[y]) pc += 2; }
void Chip8::op_6xkk(uint8_t x, uint8_t kk) { V[x] = kk; }
void Chip8::op_7xkk(uint8_t x, uint8_t kk) { V[x] += kk; }
void Chip8::op_8xy0(uint8_t x, uint8_t y) { V[x] = V[y]; }
void Chip8::op_8xy1(uint8_t x, uint8_t y) { V[x] |= V[y]; }
void Chip8::op_8xy2(uint8_t x, uint8_t y) { V[x] &= V[y]; }
void Chip8::op_8xy3(uint8_t x, uint8_t y) { V[x] ^= V[y]; }

void Chip8::op_8xy4(uint8_t x, uint8_t y) {
    uint16_t sum = V[x] + V[y];
    V[0xF] = (sum > 0xFF) ? 1 : 0;
    V[x] = sum & 0xFF;
}

void Chip8::op_8xy5(uint8_t x, uint8_t y) {
    V[0xF] = (V[x] > V[y]) ? 1 : 0;
    V[x] -= V[y];
}

void Chip8::op_8xy6(uint8_t x, uint8_t y) {
    V[0xF] = V[x] & 0x1;
    V[x] >>= 1;
}

void Chip8::op_8xy7(uint8_t x, uint8_t y) {
    V[0xF] = (V[y] > V[x]) ? 1 : 0;
    V[x] = V[y] - V[x];
}

void Chip8::op_8xyE(uint8_t x, uint8_t y) {
    V[0xF] = (V[x] & 0x80) >> 7;
    V[x] <<= 1;
}

void Chip8::op_9xy0(uint8_t x, uint8_t y) { if (V[x] != V[y]) pc += 2; }
void Chip8::op_Annn(uint16_t addr) { I = addr; }
void Chip8::op_Bnnn(uint16_t addr) { pc = addr + V[0]; }

void Chip8::op_Cxkk(uint8_t x, uint8_t kk) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    V[x] = dis(gen) & kk;
}

void Chip8::op_Dxyn(uint8_t x, uint8_t y, uint8_t n) {
    uint8_t xPos = V[x] % DISPLAY_WIDTH;
    uint8_t yPos = V[y] % DISPLAY_HEIGHT;
    V[0xF] = 0;
    
    for (int row = 0; row < n; ++row) {
        uint8_t spriteByte = memory[I + row];
        
        for (int col = 0; col < 8; ++col) {
            if ((spriteByte & (0x80 >> col)) != 0) {
                int px = (xPos + col) % DISPLAY_WIDTH;
                int py = (yPos + row) % DISPLAY_HEIGHT;
                int idx = py * DISPLAY_WIDTH + px;
                
                if (display[idx] == 1) V[0xF] = 1;
                display[idx] ^= 1;
            }
        }
    }
    
    drawFlag = true;
}

void Chip8::op_Ex9E(uint8_t x) { if (keys[V[x]]) pc += 2; }
void Chip8::op_ExA1(uint8_t x) { if (!keys[V[x]]) pc += 2; }
void Chip8::op_Fx07(uint8_t x) { V[x] = delayTimer; }

void Chip8::op_Fx0A(uint8_t x) {
    waitingForKey = true;
    keyRegister = x;
    pc -= 2; // Stay on this instruction
}

void Chip8::op_Fx15(uint8_t x) { delayTimer = V[x]; }
void Chip8::op_Fx18(uint8_t x) { soundTimer = V[x]; }
void Chip8::op_Fx1E(uint8_t x) { I += V[x]; }
void Chip8::op_Fx29(uint8_t x) { I = FONT_START_ADDR + (V[x] * 5); }

void Chip8::op_Fx33(uint8_t x) {
    uint8_t val = V[x];
    memory[I + 2] = val % 10;
    val /= 10;
    memory[I + 1] = val % 10;
    val /= 10;
    memory[I] = val % 10;
}

void Chip8::op_Fx55(uint8_t x) {
    for (int i = 0; i <= x; ++i) {
        memory[I + i] = V[i];
    }
}

void Chip8::op_Fx65(uint8_t x) {
    for (int i = 0; i <= x; ++i) {
        V[i] = memory[I + i];
    }
}

bool Chip8::saveState(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(memory.data()), MEMORY_SIZE);
    file.write(reinterpret_cast<const char*>(V.data()), REGISTER_COUNT);
    file.write(reinterpret_cast<const char*>(&I), sizeof(I));
    file.write(reinterpret_cast<const char*>(&pc), sizeof(pc));
    file.write(reinterpret_cast<const char*>(stack.data()), STACK_SIZE * sizeof(uint16_t));
    file.write(reinterpret_cast<const char*>(&sp), sizeof(sp));
    file.write(reinterpret_cast<const char*>(&delayTimer), sizeof(delayTimer));
    file.write(reinterpret_cast<const char*>(&soundTimer), sizeof(soundTimer));
    file.write(reinterpret_cast<const char*>(display.data()), DISPLAY_WIDTH * DISPLAY_HEIGHT);
    
    file.close();
    return true;
}

bool Chip8::loadState(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.read(reinterpret_cast<char*>(memory.data()), MEMORY_SIZE);
    file.read(reinterpret_cast<char*>(V.data()), REGISTER_COUNT);
    file.read(reinterpret_cast<char*>(&I), sizeof(I));
    file.read(reinterpret_cast<char*>(&pc), sizeof(pc));
    file.read(reinterpret_cast<char*>(stack.data()), STACK_SIZE * sizeof(uint16_t));
    file.read(reinterpret_cast<char*>(&sp), sizeof(sp));
    file.read(reinterpret_cast<char*>(&delayTimer), sizeof(delayTimer));
    file.read(reinterpret_cast<char*>(&soundTimer), sizeof(soundTimer));
    file.read(reinterpret_cast<char*>(display.data()), DISPLAY_WIDTH * DISPLAY_HEIGHT);
    
    drawFlag = true;
    file.close();
    return true;
}

void Chip8::printState() const {
    std::cout << "\n=== CHIP-8 State ===" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << pc << std::endl;
    std::cout << "I:  0x" << std::hex << std::setw(4) << std::setfill('0') << I << std::endl;
    std::cout << "SP: 0x" << std::hex << (int)sp << std::endl;
    
    std::cout << "\nRegisters:" << std::endl;
    for (int i = 0; i < 16; ++i) {
        std::cout << "V" << std::hex << i << ": 0x" << std::setw(2) << std::setfill('0') 
                  << (int)V[i] << "  ";
        if ((i + 1) % 4 == 0) std::cout << std::endl;
    }
    
    std::cout << "\nTimers:" << std::endl;
    std::cout << "Delay: " << (int)delayTimer << ", Sound: " << (int)soundTimer << std::endl;
}

std::string Chip8::disassemble(uint16_t opcode) const {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    uint8_t n = opcode & 0x000F;
    uint16_t nnn = opcode & 0x0FFF;
    
    char buf[64];
    
    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) return "CLS";
            if (opcode == 0x00EE) return "RET";
            break;
        case 0x1000: snprintf(buf, 64, "JP   0x%03X", nnn); return buf;
        case 0x2000: snprintf(buf, 64, "CALL 0x%03X", nnn); return buf;
        case 0x3000: snprintf(buf, 64, "SE   V%X, 0x%02X", x, kk); return buf;
        case 0x4000: snprintf(buf, 64, "SNE  V%X, 0x%02X", x, kk); return buf;
        case 0x5000: snprintf(buf, 64, "SE   V%X, V%X", x, y); return buf;
        case 0x6000: snprintf(buf, 64, "LD   V%X, 0x%02X", x, kk); return buf;
        case 0x7000: snprintf(buf, 64, "ADD  V%X, 0x%02X", x, kk); return buf;
        case 0x8000:
            switch (n) {
                case 0x0: snprintf(buf, 64, "LD   V%X, V%X", x, y); return buf;
                case 0x1: snprintf(buf, 64, "OR   V%X, V%X", x, y); return buf;
                case 0x2: snprintf(buf, 64, "AND  V%X, V%X", x, y); return buf;
                case 0x3: snprintf(buf, 64, "XOR  V%X, V%X", x, y); return buf;
                case 0x4: snprintf(buf, 64, "ADD  V%X, V%X", x, y); return buf;
                case 0x5: snprintf(buf, 64, "SUB  V%X, V%X", x, y); return buf;
                case 0x6: snprintf(buf, 64, "SHR  V%X", x); return buf;
                case 0x7: snprintf(buf, 64, "SUBN V%X, V%X", x, y); return buf;
                case 0xE: snprintf(buf, 64, "SHL  V%X", x); return buf;
            }
            break;
        case 0x9000: snprintf(buf, 64, "SNE  V%X, V%X", x, y); return buf;
        case 0xA000: snprintf(buf, 64, "LD   I, 0x%03X", nnn); return buf;
        case 0xB000: snprintf(buf, 64, "JP   V0, 0x%03X", nnn); return buf;
        case 0xC000: snprintf(buf, 64, "RND  V%X, 0x%02X", x, kk); return buf;
        case 0xD000: snprintf(buf, 64, "DRW  V%X, V%X, %d", x, y, n); return buf;
        case 0xE000:
            if (kk == 0x9E) { snprintf(buf, 64, "SKP  V%X", x); return buf; }
            if (kk == 0xA1) { snprintf(buf, 64, "SKNP V%X", x); return buf; }
            break;
        case 0xF000:
            switch (kk) {
                case 0x07: snprintf(buf, 64, "LD   V%X, DT", x); return buf;
                case 0x0A: snprintf(buf, 64, "LD   V%X, K", x); return buf;
                case 0x15: snprintf(buf, 64, "LD   DT, V%X", x); return buf;
                case 0x18: snprintf(buf, 64, "LD   ST, V%X", x); return buf;
                case 0x1E: snprintf(buf, 64, "ADD  I, V%X", x); return buf;
                case 0x29: snprintf(buf, 64, "LD   F, V%X", x); return buf;
                case 0x33: snprintf(buf, 64, "LD   B, V%X", x); return buf;
                case 0x55: snprintf(buf, 64, "LD   [I], V%X", x); return buf;
                case 0x65: snprintf(buf, 64, "LD   V%X, [I]", x); return buf;
            }
            break;
    }
    
    snprintf(buf, 64, "UNKNOWN 0x%04X", opcode);
    return buf;
}