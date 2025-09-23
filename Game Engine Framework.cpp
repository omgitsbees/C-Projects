#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <algorithm>
#include <thread>
#include <cmath>

// Forward declarations
class Entity;
class Component;
class System;

// Component base class
class Component {
public:
    virtual ~Component() = default;
    virtual void update(float deltaTime) {}
    virtual void render() {}
};

// Transform Component
class Transform : public Component {
public:
    float x = 0, y = 0, z = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
    
    Transform(float x = 0, float y = 0, float z = 0) 
        : x(x), y(y), z(z) {}
    
    void translate(float dx, float dy, float dz) {
        x += dx; y += dy; z += dz;
    }
    
    void rotate(float rx, float ry, float rz) {
        rotX += rx; rotY += ry; rotZ += rz;
    }
};

// Renderer Component
class Renderer : public Component {
private:
    std::string model;
    std::string texture;
    bool visible;
    
public:
    Renderer(const std::string& model = "cube", const std::string& texture = "default")
        : model(model), texture(texture), visible(true) {}
    
    void render() override {
        if (visible) {
            std::cout << "Rendering " << model << " with texture: " << texture << std::endl;
        }
    }
    
    void setVisible(bool v) { visible = v; }
    void setModel(const std::string& m) { model = m; }
    void setTexture(const std::string& t) { texture = t; }
};

// Physics Component
class Physics : public Component {
public:
    float velX = 0, velY = 0, velZ = 0;
    float mass = 1.0f;
    bool hasGravity = false;
    
    Physics(float mass = 1.0f, bool gravity = false) 
        : mass(mass), hasGravity(gravity) {}
    
    void update(float deltaTime) override {
        if (hasGravity) {
            velY -= 9.8f * deltaTime; // Apply gravity
        }
    }
    
    void applyForce(float fx, float fy, float fz) {
        velX += fx / mass;
        velY += fy / mass;
        velZ += fz / mass;
    }
};

// Audio Component
class Audio : public Component {
private:
    std::string soundFile;
    bool isPlaying = false;
    float volume = 1.0f;
    
public:
    Audio(const std::string& file) : soundFile(file) {}
    
    void play() {
        isPlaying = true;
        std::cout << "Playing audio: " << soundFile << " at volume " << volume << std::endl;
    }
    
    void stop() {
        isPlaying = false;
        std::cout << "Stopped audio: " << soundFile << std::endl;
    }
    
    void setVolume(float v) { volume = v; }
    bool playing() const { return isPlaying; }
};

// Entity class
class Entity {
private:
    static int nextId;
    int id;
    std::unordered_map<std::string, std::unique_ptr<Component>> components;
    bool active = true;
    
public:
    Entity() : id(nextId++) {}
    
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        components[typeid(T).name()] = std::move(component);
        return ptr;
    }
    
    template<typename T>
    T* getComponent() {
        auto it = components.find(typeid(T).name());
        if (it != components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }
    
    template<typename T>
    bool hasComponent() {
        return components.find(typeid(T).name()) != components.end();
    }
    
    void update(float deltaTime) {
        if (!active) return;
        for (auto& [type, component] : components) {
            component->update(deltaTime);
        }
    }
    
    void render() {
        if (!active) return;
        for (auto& [type, component] : components) {
            component->render();
        }
    }
    
    int getId() const { return id; }
    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
};

int Entity::nextId = 0;

// System base class
class System {
public:
    virtual ~System() = default;
    virtual void update(float deltaTime, std::vector<std::unique_ptr<Entity>>& entities) = 0;
};

// Movement System
class MovementSystem : public System {
public:
    void update(float deltaTime, std::vector<std::unique_ptr<Entity>>& entities) override {
        for (auto& entity : entities) {
            if (!entity->isActive()) continue;
            
            auto* transform = entity->getComponent<Transform>();
            auto* physics = entity->getComponent<Physics>();
            
            if (transform && physics) {
                transform->translate(physics->velX * deltaTime, 
                                   physics->velY * deltaTime, 
                                   physics->velZ * deltaTime);
            }
        }
    }
};

// Collision System
class CollisionSystem : public System {
private:
    float distance(Transform* a, Transform* b) {
        float dx = a->x - b->x;
        float dy = a->y - b->y;
        float dz = a->z - b->z;
        return sqrt(dx*dx + dy*dy + dz*dz);
    }
    
public:
    void update(float deltaTime, std::vector<std::unique_ptr<Entity>>& entities) override {
        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                auto* t1 = entities[i]->getComponent<Transform>();
                auto* t2 = entities[j]->getComponent<Transform>();
                
                if (t1 && t2 && distance(t1, t2) < 2.0f) {
                    std::cout << "Collision between entity " << entities[i]->getId() 
                             << " and entity " << entities[j]->getId() << std::endl;
                }
            }
        }
    }
};

// Resource Manager
template<typename T>
class ResourceManager {
private:
    std::unordered_map<std::string, std::unique_ptr<T>> resources;
    
public:
    T* load(const std::string& name, std::unique_ptr<T> resource) {
        T* ptr = resource.get();
        resources[name] = std::move(resource);
        return ptr;
    }
    
    T* get(const std::string& name) {
        auto it = resources.find(name);
        return (it != resources.end()) ? it->second.get() : nullptr;
    }
    
    void unload(const std::string& name) {
        resources.erase(name);
    }
    
    void clear() {
        resources.clear();
    }
};

// Input Manager
class InputManager {
private:
    std::unordered_map<int, bool> keys;
    std::unordered_map<int, std::function<void()>> keyCallbacks;
    
public:
    void setKey(int keyCode, bool pressed) {
        keys[keyCode] = pressed;
        if (pressed && keyCallbacks.count(keyCode)) {
            keyCallbacks[keyCode]();
        }
    }
    
    bool isKeyPressed(int keyCode) {
        return keys[keyCode];
    }
    
    void bindKey(int keyCode, std::function<void()> callback) {
        keyCallbacks[keyCode] = callback;
    }
    
    void simulateKeyPress(int keyCode) {
        std::cout << "Key " << keyCode << " pressed" << std::endl;
        setKey(keyCode, true);
    }
};

// Time Manager
class TimeManager {
private:
    std::chrono::high_resolution_clock::time_point lastFrame;
    float deltaTime = 0.0f;
    float totalTime = 0.0f;
    int frameCount = 0;
    
public:
    TimeManager() : lastFrame(std::chrono::high_resolution_clock::now()) {}
    
    void update() {
        auto now = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        totalTime += deltaTime;
        frameCount++;
    }
    
    float getDeltaTime() const { return deltaTime; }
    float getTotalTime() const { return totalTime; }
    float getFPS() const { return frameCount / totalTime; }
};

// Game Engine Core
class GameEngine {
private:
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<std::unique_ptr<System>> systems;
    ResourceManager<std::string> textureManager;
    ResourceManager<std::string> audioManager;
    InputManager inputManager;
    TimeManager timeManager;
    bool running = false;
    
public:
    GameEngine() {
        // Add default systems
        systems.push_back(std::make_unique<MovementSystem>());
        systems.push_back(std::make_unique<CollisionSystem>());
    }
    
    Entity* createEntity() {
        entities.push_back(std::make_unique<Entity>());
        return entities.back().get();
    }
    
    void removeEntity(int entityId) {
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [entityId](const std::unique_ptr<Entity>& e) {
                return e->getId() == entityId;
            }), entities.end());
    }
    
    template<typename T, typename... Args>
    void addSystem(Args&&... args) {
        systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }
    
    void loadTexture(const std::string& name, const std::string& path) {
        textureManager.load(name, std::make_unique<std::string>(path));
        std::cout << "Loaded texture: " << name << " from " << path << std::endl;
    }
    
    void loadAudio(const std::string& name, const std::string& path) {
        audioManager.load(name, std::make_unique<std::string>(path));
        std::cout << "Loaded audio: " << name << " from " << path << std::endl;
    }
    
    InputManager& getInputManager() { return inputManager; }
    TimeManager& getTimeManager() { return timeManager; }
    
    void start() {
        running = true;
        std::cout << "Game Engine Started!" << std::endl;
        
        // Setup input bindings
        inputManager.bindKey(32, [this]() { // Space key
            std::cout << "Space pressed - Jump action!" << std::endl;
        });
        
        inputManager.bindKey(27, [this]() { // Escape key
            std::cout << "Escape pressed - Stopping engine..." << std::endl;
            stop();
        });
    }
    
    void update() {
        if (!running) return;
        
        timeManager.update();
        float deltaTime = timeManager.getDeltaTime();
        
        // Update all systems
        for (auto& system : systems) {
            system->update(deltaTime, entities);
        }
        
        // Update all entities
        for (auto& entity : entities) {
            entity->update(deltaTime);
        }
    }
    
    void render() {
        if (!running) return;
        
        std::cout << "\n--- Rendering Frame ---" << std::endl;
        for (auto& entity : entities) {
            entity->render();
        }
        std::cout << "FPS: " << timeManager.getFPS() << std::endl;
    }
    
    void stop() {
        running = false;
        std::cout << "Game Engine Stopped!" << std::endl;
    }
    
    bool isRunning() const { return running; }
};

// Example game implementation
int main() {
    GameEngine engine;
    
    // Load resources
    engine.loadTexture("player", "assets/player.png");
    engine.loadTexture("enemy", "assets/enemy.png");
    engine.loadAudio("jump", "assets/jump.wav");
    engine.loadAudio("music", "assets/background.mp3");
    
    // Create player entity
    Entity* player = engine.createEntity();
    player->addComponent<Transform>(0, 0, 0);
    player->addComponent<Physics>(1.0f, true);
    player->addComponent<Renderer>("player_model", "player");
    player->addComponent<Audio>("jump.wav");
    
    // Create enemy entity
    Entity* enemy = engine.createEntity();
    enemy->addComponent<Transform>(5, 0, 0);
    enemy->addComponent<Physics>(2.0f, false);
    enemy->addComponent<Renderer>("enemy_model", "enemy");
    
    // Setup player movement
    engine.getInputManager().bindKey(65, [player]() { // 'A' key
        auto* physics = player->getComponent<Physics>();
        if (physics) physics->applyForce(-5, 0, 0);
    });
    
    engine.getInputManager().bindKey(68, [player]() { // 'D' key
        auto* physics = player->getComponent<Physics>();
        if (physics) physics->applyForce(5, 0, 0);
    });
    
    // Start the engine
    engine.start();
    
    // Game loop simulation
    std::cout << "\n=== Game Loop Simulation ===" << std::endl;
    
    for (int frame = 0; frame < 5 && engine.isRunning(); ++frame) {
        std::cout << "\nFrame " << frame + 1 << ":" << std::endl;
        
        // Simulate some input
        if (frame == 1) engine.getInputManager().simulateKeyPress(65); // Move left
        if (frame == 2) engine.getInputManager().simulateKeyPress(32); // Jump
        if (frame == 3) engine.getInputManager().simulateKeyPress(68); // Move right
        
        // Apply some enemy movement
        auto* enemyPhysics = enemy->getComponent<Physics>();
        if (enemyPhysics && frame < 3) {
            enemyPhysics->applyForce(-1, 0, 0);
        }
        
        engine.update();
        engine.render();
        
        // Small delay to simulate frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    // Demonstrate audio system
    std::cout << "\n=== Audio System Demo ===" << std::endl;
    auto* playerAudio = player->getComponent<Audio>();
    if (playerAudio) {
        playerAudio->play();
        playerAudio->setVolume(0.8f);
        playerAudio->stop();
    }
    
    std::cout << "\n=== Engine Statistics ===" << std::endl;
    std::cout << "Total Runtime: " << engine.getTimeManager().getTotalTime() << " seconds" << std::endl;
    std::cout << "Average FPS: " << engine.getTimeManager().getFPS() << std::endl;
    
    return 0;
}