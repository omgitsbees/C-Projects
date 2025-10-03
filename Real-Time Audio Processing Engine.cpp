#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <cmath>
#include <algorithm>
#include <string>
#include <map>
#include <functional>
#include <chrono>
#include <iomanip>

// ============================================================================
// CORE AUDIO TYPES AND CONSTANTS
// ============================================================================

constexpr int SAMPLE_RATE = 44100;
constexpr int BUFFER_SIZE = 512;
constexpr int MAX_CHANNELS = 8;

using AudioBuffer = std::vector<float>;
using StereoBuffer = std::pair<AudioBuffer, AudioBuffer>;

// ============================================================================
// MIDI MESSAGE STRUCTURE
// ============================================================================

struct MidiMessage {
    enum Type {
        NOTE_ON,
        NOTE_OFF,
        CONTROL_CHANGE,
        PITCH_BEND,
        PROGRAM_CHANGE
    };
    
    Type type;
    int channel;
    int note;
    int velocity;
    int controller;
    int value;
    double timestamp;
    
    static MidiMessage noteOn(int ch, int n, int vel, double time = 0.0) {
        return {NOTE_ON, ch, n, vel, 0, 0, time};
    }
    
    static MidiMessage noteOff(int ch, int n, double time = 0.0) {
        return {NOTE_OFF, ch, n, 0, 0, 0, time};
    }
    
    static MidiMessage controlChange(int ch, int ctrl, int val, double time = 0.0) {
        return {CONTROL_CHANGE, ch, 0, 0, ctrl, val, time};
    }
};

// ============================================================================
// AUDIO PLUGIN INTERFACE (VST/AU-style)
// ============================================================================

class AudioPlugin {
public:
    virtual ~AudioPlugin() = default;
    
    virtual std::string getName() const = 0;
    virtual void prepare(int sampleRate, int maxBufferSize) = 0;
    virtual void process(float* inputL, float* inputR, float* outputL, float* outputR, int numSamples) = 0;
    virtual void processMidi(const MidiMessage& msg) {}
    virtual void setParameter(int index, float value) {}
    virtual float getParameter(int index) const { return 0.0f; }
    virtual int getNumParameters() const { return 0; }
    virtual std::string getParameterName(int index) const { return ""; }
    virtual void reset() {}
    
    bool isBypassed() const { return bypassed; }
    void setBypassed(bool b) { bypassed = b; }
    
protected:
    bool bypassed = false;
};

// ============================================================================
// BUILT-IN EFFECT PLUGINS
// ============================================================================

// Delay/Echo Effect
class DelayPlugin : public AudioPlugin {
    std::vector<float> delayBufferL, delayBufferR;
    int writePos = 0;
    float delayTime = 0.5f;  // seconds
    float feedback = 0.4f;
    float mix = 0.3f;
    int delaySamples;
    
public:
    std::string getName() const override { return "Delay"; }
    
    void prepare(int sampleRate, int maxBufferSize) override {
        int maxDelay = sampleRate * 2; // 2 seconds max
        delayBufferL.resize(maxDelay, 0.0f);
        delayBufferR.resize(maxDelay, 0.0f);
        updateDelaySamples(sampleRate);
    }
    
    void process(float* inL, float* inR, float* outL, float* outR, int n) override {
        if (bypassed) {
            std::copy(inL, inL + n, outL);
            std::copy(inR, inR + n, outR);
            return;
        }
        
        for (int i = 0; i < n; ++i) {
            int readPos = (writePos - delaySamples + delayBufferL.size()) % delayBufferL.size();
            
            float delayedL = delayBufferL[readPos];
            float delayedR = delayBufferR[readPos];
            
            outL[i] = inL[i] * (1.0f - mix) + delayedL * mix;
            outR[i] = inR[i] * (1.0f - mix) + delayedR * mix;
            
            delayBufferL[writePos] = inL[i] + delayedL * feedback;
            delayBufferR[writePos] = inR[i] + delayedR * feedback;
            
            writePos = (writePos + 1) % delayBufferL.size();
        }
    }
    
    void setParameter(int idx, float val) override {
        switch (idx) {
            case 0: delayTime = val; updateDelaySamples(SAMPLE_RATE); break;
            case 1: feedback = val; break;
            case 2: mix = val; break;
        }
    }
    
    float getParameter(int idx) const override {
        switch (idx) {
            case 0: return delayTime;
            case 1: return feedback;
            case 2: return mix;
            default: return 0.0f;
        }
    }
    
    int getNumParameters() const override { return 3; }
    
    std::string getParameterName(int idx) const override {
        const char* names[] = {"Delay Time", "Feedback", "Mix"};
        return idx < 3 ? names[idx] : "";
    }
    
private:
    void updateDelaySamples(int sr) {
        delaySamples = static_cast<int>(delayTime * sr);
    }
};

// Reverb Effect (Simple Schroeder Reverb)
class ReverbPlugin : public AudioPlugin {
    struct AllpassFilter {
        std::vector<float> buffer;
        int pos = 0;
        
        void prepare(int delaySamples) {
            buffer.resize(delaySamples, 0.0f);
        }
        
        float process(float input, float gain) {
            float delayed = buffer[pos];
            float output = -input + delayed;
            buffer[pos] = input + delayed * gain;
            pos = (pos + 1) % buffer.size();
            return output;
        }
    };
    
    std::vector<AllpassFilter> allpassFiltersL, allpassFiltersR;
    std::vector<float> combBuffersL[4], combBuffersR[4];
    int combPos[4] = {0};
    float roomSize = 0.5f;
    float damping = 0.5f;
    float mix = 0.3f;
    
public:
    std::string getName() const override { return "Reverb"; }
    
    void prepare(int sampleRate, int maxBufferSize) override {
        // Comb filter delays (in samples)
        int combDelays[] = {1557, 1617, 1491, 1422};
        for (int i = 0; i < 4; ++i) {
            combBuffersL[i].resize(combDelays[i], 0.0f);
            combBuffersR[i].resize(combDelays[i], 0.0f);
        }
        
        // Allpass filter delays
        int allpassDelays[] = {225, 556, 441, 341};
        allpassFiltersL.resize(4);
        allpassFiltersR.resize(4);
        for (int i = 0; i < 4; ++i) {
            allpassFiltersL[i].prepare(allpassDelays[i]);
            allpassFiltersR[i].prepare(allpassDelays[i]);
        }
    }
    
    void process(float* inL, float* inR, float* outL, float* outR, int n) override {
        if (bypassed) {
            std::copy(inL, inL + n, outL);
            std::copy(inR, inR + n, outR);
            return;
        }
        
        for (int i = 0; i < n; ++i) {
            float combOutL = 0.0f, combOutR = 0.0f;
            
            // Process comb filters
            for (int c = 0; c < 4; ++c) {
                combOutL += combBuffersL[c][combPos[c]];
                combOutR += combBuffersR[c][combPos[c]];
                
                combBuffersL[c][combPos[c]] = inL[i] + combBuffersL[c][combPos[c]] * roomSize * 0.84f;
                combBuffersR[c][combPos[c]] = inR[i] + combBuffersR[c][combPos[c]] * roomSize * 0.84f;
                
                combPos[c] = (combPos[c] + 1) % combBuffersL[c].size();
            }
            
            combOutL *= 0.25f;
            combOutR *= 0.25f;
            
            // Process allpass filters
            for (auto& ap : allpassFiltersL) {
                combOutL = ap.process(combOutL, 0.5f);
            }
            for (auto& ap : allpassFiltersR) {
                combOutR = ap.process(combOutR, 0.5f);
            }
            
            outL[i] = inL[i] * (1.0f - mix) + combOutL * mix;
            outR[i] = inR[i] * (1.0f - mix) + combOutR * mix;
        }
    }
    
    void setParameter(int idx, float val) override {
        switch (idx) {
            case 0: roomSize = val; break;
            case 1: damping = val; break;
            case 2: mix = val; break;
        }
    }
    
    float getParameter(int idx) const override {
        switch (idx) {
            case 0: return roomSize;
            case 1: return damping;
            case 2: return mix;
            default: return 0.0f;
        }
    }
    
    int getNumParameters() const override { return 3; }
    
    std::string getParameterName(int idx) const override {
        const char* names[] = {"Room Size", "Damping", "Mix"};
        return idx < 3 ? names[idx] : "";
    }
};

// Distortion Effect
class DistortionPlugin : public AudioPlugin {
    float drive = 0.5f;
    float tone = 0.5f;
    float mix = 1.0f;
    
public:
    std::string getName() const override { return "Distortion"; }
    
    void prepare(int sampleRate, int maxBufferSize) override {}
    
    void process(float* inL, float* inR, float* outL, float* outR, int n) override {
        if (bypassed) {
            std::copy(inL, inL + n, outL);
            std::copy(inR, inR + n, outR);
            return;
        }
        
        float driveAmount = 1.0f + drive * 20.0f;
        
        for (int i = 0; i < n; ++i) {
            float distL = std::tanh(inL[i] * driveAmount);
            float distR = std::tanh(inR[i] * driveAmount);
            
            outL[i] = inL[i] * (1.0f - mix) + distL * mix;
            outR[i] = inR[i] * (1.0f - mix) + distR * mix;
        }
    }
    
    void setParameter(int idx, float val) override {
        switch (idx) {
            case 0: drive = val; break;
            case 1: tone = val; break;
            case 2: mix = val; break;
        }
    }
    
    float getParameter(int idx) const override {
        switch (idx) {
            case 0: return drive;
            case 1: return tone;
            case 2: return mix;
            default: return 0.0f;
        }
    }
    
    int getNumParameters() const override { return 3; }
    
    std::string getParameterName(int idx) const override {
        const char* names[] = {"Drive", "Tone", "Mix"};
        return idx < 3 ? names[idx] : "";
    }
};

// ============================================================================
// AUDIO TRACK
// ============================================================================

class AudioTrack {
    std::string name;
    std::vector<std::unique_ptr<AudioPlugin>> pluginChain;
    AudioBuffer recordBuffer;
    bool recording = false;
    bool muted = false;
    bool soloed = false;
    float volume = 1.0f;
    float pan = 0.0f; // -1.0 (left) to 1.0 (right)
    
    AudioBuffer tempL, tempR;
    
public:
    AudioTrack(const std::string& trackName) : name(trackName) {
        tempL.resize(BUFFER_SIZE);
        tempR.resize(BUFFER_SIZE);
    }
    
    void addPlugin(std::unique_ptr<AudioPlugin> plugin) {
        plugin->prepare(SAMPLE_RATE, BUFFER_SIZE);
        pluginChain.push_back(std::move(plugin));
    }
    
    void process(float* inputL, float* inputR, float* outputL, float* outputR, int numSamples) {
        // Copy input to temp buffers
        std::copy(inputL, inputL + numSamples, tempL.begin());
        std::copy(inputR, inputR + numSamples, tempR.begin());
        
        // Process through plugin chain
        for (auto& plugin : pluginChain) {
            plugin->process(tempL.data(), tempR.data(), tempL.data(), tempR.data(), numSamples);
        }
        
        // Apply volume and pan
        float leftGain = volume * std::sqrt((1.0f - pan) * 0.5f + 0.5f);
        float rightGain = volume * std::sqrt((1.0f + pan) * 0.5f + 0.5f);
        
        for (int i = 0; i < numSamples; ++i) {
            if (!muted) {
                outputL[i] += tempL[i] * leftGain;
                outputR[i] += tempR[i] * rightGain;
            }
        }
        
        // Record if enabled
        if (recording) {
            recordBuffer.insert(recordBuffer.end(), tempL.begin(), tempL.begin() + numSamples);
        }
    }
    
    void setVolume(float v) { volume = std::clamp(v, 0.0f, 2.0f); }
    void setPan(float p) { pan = std::clamp(p, -1.0f, 1.0f); }
    void setMuted(bool m) { muted = m; }
    void setSoloed(bool s) { soloed = s; }
    void setRecording(bool r) { recording = r; }
    
    bool isMuted() const { return muted; }
    bool isSoloed() const { return soloed; }
    bool isRecording() const { return recording; }
    float getVolume() const { return volume; }
    float getPan() const { return pan; }
    std::string getName() const { return name; }
    
    const std::vector<std::unique_ptr<AudioPlugin>>& getPlugins() const { return pluginChain; }
    AudioPlugin* getPlugin(size_t index) {
        return index < pluginChain.size() ? pluginChain[index].get() : nullptr;
    }
};

// ============================================================================
// AUDIO ENGINE
// ============================================================================

class AudioEngine {
    std::vector<std::unique_ptr<AudioTrack>> tracks;
    std::queue<MidiMessage> midiQueue;
    std::mutex midiMutex;
    std::atomic<bool> running{false};
    std::atomic<double> currentTime{0.0};
    
    AudioBuffer masterOutL, masterOutR;
    float masterVolume = 1.0f;
    
public:
    AudioEngine() {
        masterOutL.resize(BUFFER_SIZE, 0.0f);
        masterOutR.resize(BUFFER_SIZE, 0.0f);
    }
    
    void start() {
        running = true;
        std::cout << "Audio Engine Started (Sample Rate: " << SAMPLE_RATE 
                  << " Hz, Buffer Size: " << BUFFER_SIZE << " samples)\n";
    }
    
    void stop() {
        running = false;
        std::cout << "Audio Engine Stopped\n";
    }
    
    AudioTrack* addTrack(const std::string& name) {
        tracks.push_back(std::make_unique<AudioTrack>(name));
        std::cout << "Added track: " << name << "\n";
        return tracks.back().get();
    }
    
    void processMidi(const MidiMessage& msg) {
        std::lock_guard<std::mutex> lock(midiMutex);
        midiQueue.push(msg);
    }
    
    void processAudio(float* inputL, float* inputR, int numSamples) {
        if (!running) return;
        
        // Clear master output
        std::fill(masterOutL.begin(), masterOutL.begin() + numSamples, 0.0f);
        std::fill(masterOutR.begin(), masterOutR.begin() + numSamples, 0.0f);
        
        // Process MIDI messages
        {
            std::lock_guard<std::mutex> lock(midiMutex);
            while (!midiQueue.empty()) {
                auto msg = midiQueue.front();
                midiQueue.pop();
                
                for (auto& track : tracks) {
                    for (auto& plugin : track->getPlugins()) {
                        plugin->processMidi(msg);
                    }
                }
            }
        }
        
        // Check if any tracks are soloed
        bool anySoloed = false;
        for (const auto& track : tracks) {
            if (track->isSoloed()) {
                anySoloed = true;
                break;
            }
        }
        
        // Process each track
        for (auto& track : tracks) {
            if (anySoloed && !track->isSoloed()) continue;
            
            track->process(inputL, inputR, masterOutL.data(), masterOutR.data(), numSamples);
        }
        
        // Apply master volume
        for (int i = 0; i < numSamples; ++i) {
            masterOutL[i] *= masterVolume;
            masterOutR[i] *= masterVolume;
        }
        
        currentTime += static_cast<double>(numSamples) / SAMPLE_RATE;
    }
    
    void setMasterVolume(float v) { masterVolume = std::clamp(v, 0.0f, 2.0f); }
    float getMasterVolume() const { return masterVolume; }
    
    AudioTrack* getTrack(size_t index) {
        return index < tracks.size() ? tracks[index].get() : nullptr;
    }
    
    size_t getNumTracks() const { return tracks.size(); }
    
    const AudioBuffer& getMasterOutputL() const { return masterOutL; }
    const AudioBuffer& getMasterOutputR() const { return masterOutR; }
    
    double getCurrentTime() const { return currentTime; }
    bool isRunning() const { return running; }
};

// ============================================================================
// DEMO / TEST FUNCTIONS
// ============================================================================

void printTrackInfo(AudioTrack* track) {
    std::cout << "\n  Track: " << track->getName() << "\n";
    std::cout << "    Volume: " << std::fixed << std::setprecision(2) 
              << track->getVolume() << " | Pan: " << track->getPan() << "\n";
    std::cout << "    Muted: " << (track->isMuted() ? "Yes" : "No")
              << " | Soloed: " << (track->isSoloed() ? "Yes" : "No") << "\n";
    std::cout << "    Plugins: " << track->getPlugins().size() << "\n";
    
    for (size_t i = 0; i < track->getPlugins().size(); ++i) {
        auto* plugin = track->getPlugin(i);
        std::cout << "      [" << i << "] " << plugin->getName() 
                  << (plugin->isBypassed() ? " (Bypassed)" : "") << "\n";
    }
}

void simulateProcessing(AudioEngine& engine, int numBuffers) {
    std::cout << "\n=== Simulating " << numBuffers << " audio buffers ===\n";
    
    AudioBuffer inputL(BUFFER_SIZE), inputR(BUFFER_SIZE);
    
    for (int buf = 0; buf < numBuffers; ++buf) {
        // Generate test signal (sine wave)
        float freq = 440.0f;
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            float t = (buf * BUFFER_SIZE + i) / static_cast<float>(SAMPLE_RATE);
            float sample = std::sin(2.0f * M_PI * freq * t) * 0.3f;
            inputL[i] = sample;
            inputR[i] = sample;
        }
        
        engine.processAudio(inputL.data(), inputR.data(), BUFFER_SIZE);
        
        // Show progress every 10 buffers
        if ((buf + 1) % 10 == 0) {
            std::cout << "Processed " << (buf + 1) << " buffers (Time: " 
                      << std::fixed << std::setprecision(3) 
                      << engine.getCurrentTime() << "s)\n";
        }
    }
    
    // Calculate output RMS
    const auto& outL = engine.getMasterOutputL();
    const auto& outR = engine.getMasterOutputR();
    float rmsL = 0.0f, rmsR = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        rmsL += outL[i] * outL[i];
        rmsR += outR[i] * outR[i];
    }
    rmsL = std::sqrt(rmsL / BUFFER_SIZE);
    rmsR = std::sqrt(rmsR / BUFFER_SIZE);
    
    std::cout << "Final RMS Level - L: " << rmsL << ", R: " << rmsR << "\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "  Real-Time Audio Processing Engine\n";
    std::cout << "==============================================\n\n";
    
    AudioEngine engine;
    engine.start();
    
    // Create tracks with different effects
    std::cout << "\n--- Creating Tracks ---\n";
    
    auto* track1 = engine.addTrack("Lead Synth");
    track1->addPlugin(std::make_unique<DelayPlugin>());
    track1->addPlugin(std::make_unique<ReverbPlugin>());
    track1->setVolume(0.8f);
    track1->setPan(-0.3f);
    
    auto* track2 = engine.addTrack("Bass");
    track2->addPlugin(std::make_unique<DistortionPlugin>());
    track2->setVolume(0.9f);
    track2->setPan(0.0f);
    
    auto* track3 = engine.addTrack("Drums");
    track3->addPlugin(std::make_unique<ReverbPlugin>());
    track3->setVolume(1.0f);
    track3->setPan(0.2f);
    
    // Print track info
    std::cout << "\n--- Track Configuration ---";
    for (size_t i = 0; i < engine.getNumTracks(); ++i) {
        printTrackInfo(engine.getTrack(i));
    }
    
    // Set master volume
    engine.setMasterVolume(0.9f);
    std::cout << "\nMaster Volume: " << engine.getMasterVolume() << "\n";
    
    // Send MIDI messages
    std::cout << "\n--- Sending MIDI Messages ---\n";
    engine.processMidi(MidiMessage::noteOn(1, 60, 100, 0.0));
    std::cout << "Sent MIDI Note On: Channel 1, Note 60 (C4), Velocity 100\n";
    engine.processMidi(MidiMessage::noteOn(1, 64, 100, 0.0));
    std::cout << "Sent MIDI Note On: Channel 1, Note 64 (E4), Velocity 100\n";
    
    // Process audio buffers
    simulateProcessing(engine, 50);
    
    // Test plugin parameters
    std::cout << "\n--- Testing Plugin Parameters ---\n";
    if (auto* plugin = track1->getPlugin(0)) {
        std::cout << "Setting " << plugin->getName() << " parameters:\n";
        plugin->setParameter(0, 0.75f); // Delay time
        plugin->setParameter(1, 0.5f);  // Feedback
        plugin->setParameter(2, 0.4f);  // Mix
        
        for (int i = 0; i < plugin->getNumParameters(); ++i) {
            std::cout << "  " << plugin->getParameterName(i) << ": " 
                      << plugin->getParameter(i) << "\n";
        }
    }
    
    // Test bypass
    std::cout << "\n--- Testing Bypass ---\n";
    if (auto* plugin = track2->getPlugin(0)) {
        plugin->setBypassed(true);
        std::cout << plugin->getName() << " bypassed\n";
    }
    
    // Test solo/mute
    std::cout << "\n--- Testing Solo/Mute ---\n";
    track1->setSoloed(true);
    std::cout << "Track 1 soloed (only Track 1 will be heard)\n";
    simulateProcessing(engine, 10);
    
    track1->setSoloed(false);
    track3->setMuted(true);
    std::cout << "Track 3 muted\n";
    simulateProcessing(engine, 10);
    
    // Performance stats
    std::cout << "\n--- Performance Statistics ---\n";
    std::cout << "Total Tracks: " << engine.getNumTracks() << "\n";
    std::cout << "Total Processing Time: " << engine.getCurrentTime() << " seconds\n";
    std::cout << "Sample Rate: " << SAMPLE_RATE << " Hz\n";
    std::cout << "Buffer Size: " << BUFFER_SIZE << " samples\n";
    std::cout << "Latency: " << (BUFFER_SIZE * 1000.0 / SAMPLE_RATE) << " ms\n";
    
    engine.stop();
    
    std::cout << "\n==============================================\n";
    std::cout << "  Audio Engine Demo Complete!\n";
    std::cout << "==============================================\n";
    
    return 0;
}