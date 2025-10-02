#include "chip8.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>

constexpr int SCALE = 10;
constexpr int SCREEN_WIDTH = Chip8::DISPLAY_WIDTH * SCALE;
constexpr int SCREEN_HEIGHT = Chip8::DISPLAY_HEIGHT * SCALE;
constexpr int CYCLES_PER_FRAME = 10;
constexpr int FPS = 60;
constexpr int FRAME_DELAY = 1000 / FPS;

// Keyboard mapping: CHIP-8 uses hex keypad 0-F
// We map to QWERTY keyboard
const SDL_Scancode KEYMAP[16] = {
    SDL_SCANCODE_X,    // 0
    SDL_SCANCODE_1,    // 1
    SDL_SCANCODE_2,    // 2
    SDL_SCANCODE_3,    // 3
    SDL_SCANCODE_Q,    // 4
    SDL_SCANCODE_W,    // 5
    SDL_SCANCODE_E,    // 6
    SDL_SCANCODE_A,    // 7
    SDL_SCANCODE_S,    // 8
    SDL_SCANCODE_D,    // 9
    SDL_SCANCODE_Z,    // A
    SDL_SCANCODE_C,    // B
    SDL_SCANCODE_4,    // C
    SDL_SCANCODE_R,    // D
    SDL_SCANCODE_F,    // E
    SDL_SCANCODE_V     // F
};

class AudioBeeper {
public:
    AudioBeeper() : deviceId(0), isPlaying(false) {
        SDL_AudioSpec spec;
        spec.freq = 44100;
        spec.format = AUDIO_S16SYS;
        spec.channels = 1;
        spec.samples = 2048;
        spec.callback = audioCallback;
        spec.userdata = this;
        
        deviceId = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        if (deviceId == 0) {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        }
    }
    
    ~AudioBeeper() {
        if (deviceId != 0) {
            SDL_CloseAudioDevice(deviceId);
        }
    }
    
    void start() {
        if (deviceId != 0 && !isPlaying) {
            SDL_PauseAudioDevice(deviceId, 0);
            isPlaying = true;
        }
    }
    
    void stop() {
        if (deviceId != 0 && isPlaying) {
            SDL_PauseAudioDevice(deviceId, 1);
            isPlaying = false;
        }
    }
    
private:
    SDL_AudioDeviceID deviceId;
    bool isPlaying;
    float phase = 0.0f;
    
    static void audioCallback(void* userdata, Uint8* stream, int len) {
        AudioBeeper* beeper = static_cast<AudioBeeper*>(userdata);
        Sint16* buffer = reinterpret_cast<Sint16*>(stream);
        int samples = len / sizeof(Sint16);
        
        const float frequency = 440.0f; // A4 note
        const float sampleRate = 44100.0f;
        const float amplitude = 3000;
        
        for (int i = 0; i < samples; ++i) {
            buffer[i] = static_cast<Sint16>(amplitude * std::sin(beeper->phase));
            beeper->phase += 2.0f * M_PI * frequency / sampleRate;
            if (beeper->phase >= 2.0f * M_PI) {
                beeper->phase -= 2.0f * M_PI;
            }
        }
    }
};

void renderDisplay(SDL_Renderer* renderer, const Chip8& chip8) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    const auto& display = chip8.getDisplay();
    for (int y = 0; y < Chip8::DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < Chip8::DISPLAY_WIDTH; ++x) {
            if (display[y * Chip8::DISPLAY_WIDTH + x] != 0) {
                SDL_Rect rect = { x * SCALE, y * SCALE, SCALE, SCALE };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
    
    SDL_RenderPresent(renderer);
}

void handleInput(Chip8& chip8, const Uint8* keyState) {
    for (int i = 0; i < 16; ++i) {
        chip8.setKey(i, keyState[KEYMAP[i]]);
    }
}

void printHelp() {
    std::cout << "\n=== CHIP-8 Emulator ===" << std::endl;
    std::cout << "Usage: chip8_emulator <rom_file>" << std::endl;
    std::cout << "\nKeyboard Layout:" << std::endl;
    std::cout << "  Original CHIP-8      Modern Keyboard" << std::endl;
    std::cout << "  1 2 3 C              1 2 3 4" << std::endl;
    std::cout << "  4 5 6 D      --->    Q W E R" << std::endl;
    std::cout << "  7 8 9 E              A S D F" << std::endl;
    std::cout << "  A 0 B F              Z X C V" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  ESC       - Quit" << std::endl;
    std::cout << "  P         - Pause/Resume" << std::endl;
    std::cout << "  O         - Step one instruction (when paused)" << std::endl;
    std::cout << "  I         - Print state info" << std::endl;
    std::cout << "  F5        - Save state" << std::endl;
    std::cout << "  F9        - Load state" << std::endl;
    std::cout << "  R         - Reset" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Initialize CHIP-8
    Chip8 chip8;
    if (!chip8.loadROM(argv[1])) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Initialize audio
    AudioBeeper beeper;
    
    printHelp();
    
    // Main loop
    bool running = true;
    bool paused = false;
    bool wasBeeping = false;
    SDL_Event event;
    
    auto lastTimerUpdate = std::chrono::steady_clock::now();
    
    while (running) {
        auto frameStart = std::chrono::steady_clock::now();
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_p:
                        paused = !paused;
                        std::cout << (paused ? "Paused" : "Resumed") << std::endl;
                        break;
                    case SDLK_o:
                        if (paused) {
                            chip8.emulateCycle();
                            std::cout << "Step executed" << std::endl;
                        }
                        break;
                    case SDLK_i:
                        chip8.printState();
                        break;
                    case SDLK_F5:
                        if (chip8.saveState("savestate.ch8")) {
                            std::cout << "State saved" << std::endl;
                        } else {
                            std::cout << "Failed to save state" << std::endl;
                        }
                        break;
                    case SDLK_F9:
                        if (chip8.loadState("savestate.ch8")) {
                            std::cout << "State loaded" << std::endl;
                        } else {
                            std::cout << "Failed to load state" << std::endl;
                        }
                        break;
                    case SDLK_r:
                        chip8.reset();
                        chip8.loadROM(argv[1]);
                        std::cout << "Reset" << std::endl;
                        break;
                }
            }
        }
        
        // Update input
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        handleInput(chip8, keyState);
        
        // Emulate cycles
        if (!paused) {
            for (int i = 0; i < CYCLES_PER_FRAME; ++i) {
                chip8.emulateCycle();
            }
            
            // Update timers at 60Hz
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimerUpdate);
            if (elapsed.count() >= 16) { // ~60Hz
                chip8.updateTimers();
                lastTimerUpdate = now;
            }
        }
        
        // Handle audio
        bool isBeeping = chip8.isBeeping();
        if (isBeeping && !wasBeeping) {
            beeper.start();
        } else if (!isBeeping && wasBeeping) {
            beeper.stop();
        }
        wasBeeping = isBeeping;
        
        // Render
        if (chip8.needsRedraw()) {
            renderDisplay(renderer, chip8);
            chip8.clearDrawFlag();
        }
        
        // Frame limiting
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (frameDuration.count() < FRAME_DELAY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY - frameDuration.count()));
        }
    }
    
    // Cleanup
    beeper.stop();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}