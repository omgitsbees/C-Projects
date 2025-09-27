#include "neural_framework.h"
#include "advanced_layers.h"
#include "cuda_kernels.h"
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>

// Implement Variable::backward()
void Variable::backward() {
    if (!requires_grad_) return;
    
    if (grad_->data().empty() || std::all_of(grad_->data().begin(), grad_->data().end(), 
                                            [](float x) { return x == 0.0f; })) {
        std::fill(grad_->data().begin(), grad_->data().end(), 1.0f);
    }
    
    if (grad_fn_) {
        grad_fn_->backward(*this);
    }
}

// Advanced optimizers
class Adam {
private:
    float learning_rate_;
    float beta1_, beta2_;
    float epsilon_;
    int step_;
    std::unordered_map<Variable*, Tensor> m_, v_; // momentum and velocity
    
public:
    Adam(float learning_rate = 0.001f, float beta1 = 0.9f, 
         float beta2 = 0.999f, float epsilon = 1e-8f)
        : learning_rate_(learning_rate), beta1_(beta1), beta2_(beta2), 
          epsilon_(epsilon), step_(0) {}
    
    void step(Sequential& model) {
        step_++;
        auto params = model.parameters();
        
        for (auto param : params) {
            if (!param->requires_grad()) continue;
            
            // Initialize momentum and velocity if needed
            if (m_.find(param) == m_.end()) {
                m_[param] = Tensor(param->data().shape());
                v_[param] = Tensor(param->data().shape());
            }
            
            auto& m = m_[param];
            auto& v = v_[param];
            const auto& grad = param->grad();
            
            // Update biased first moment estimate
            for (int i = 0; i < m.size(); ++i) {
                m.data()[i] = beta1_ * m.data()[i] + (1.0f - beta1_) * grad.data()[i];
            }
            
            // Update biased second raw moment estimate
            for (int i = 0; i < v.size(); ++i) {
                v.data()[i] = beta2_ * v.data()[i] + 
                             (1.0f - beta2_) * grad.data()[i] * grad.data()[i];
            }
            
            // Compute bias-corrected first moment estimate
            float beta1_correction = 1.0f - std::pow(beta1_, step_);
            float beta2_correction = 1.0f - std::pow(beta2_, step_);
            
            // Update parameters
            for (int i = 0; i < param->data().size(); ++i) {
                float m_hat = m.data()[i] / beta1_correction;
                float v_hat = v.data()[i] / beta2_correction;
                param->data().data()[i] -= learning_rate_ * m_hat / 
                                          (std::sqrt(v_hat) + epsilon_);
            }
        }
    }
    
    void zero_grad(Sequential& model) {
        auto params = model.parameters();
        for (auto param : params) {
            param->zero_grad();
        }
    }
};

// Learning rate scheduler
class StepLR {
private:
    float initial_lr_;
    float gamma_;
    int step_size_;
    int current_step_;
    
public:
    StepLR(float initial_lr, int step_size, float gamma = 0.1f)
        : initial_lr_(initial_lr), gamma_(gamma), step_size_(step_size), current_step_(0) {}
    
    float get_lr() {
        int num_decays = current_step_ / step_size_;
        return initial_lr_ * std::pow(gamma_, num_decays);
    }
    
    void step() { current_step_++; }
};

// Data loading utilities
class DataLoader {
private:
    std::vector<Tensor> inputs_;
    std::vector<Tensor> targets_;
    int batch_size_;
    bool shuffle_;
    std::vector<int> indices_;
    int current_idx_;
    
public:
    DataLoader(const std::vector<Tensor>& inputs, const std::vector<Tensor>& targets,
               int batch_size = 32, bool shuffle = true)
        : inputs_(inputs), targets_(targets), batch_size_(batch_size), 
          shuffle_(shuffle), current_idx_(0) {
        
        indices_.resize(inputs_.size());
        std::iota(indices_.begin(), indices_.end(), 0);
        
        if (shuffle_) {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(indices_.begin(), indices_.end(), g);
        }
    }
    
    bool has_next() const {
        return current_idx_ < indices_.size();
    }
    
    std::pair<std::vector<Tensor>, std::vector<Tensor>> next_batch() {
        std::vector<Tensor> batch_inputs, batch_targets;
        
        int end_idx = std::min(current_idx_ + batch_size_, (int)indices_.size());
        
        for (int i = current_idx_; i < end_idx; ++i) {
            batch_inputs.push_back(inputs_[indices_[i]]);
            batch_targets.push_back(targets_[indices_[i]]);
        }
        
        current_idx_ = end_idx;
        return {batch_inputs, batch_targets};
    }
    
    void reset() {
        current_idx_ = 0;
        if (shuffle_) {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(indices_.begin(), indices_.end(), g);
        }
    }
    
    int size() const { return indices_.size(); }
};

// Model checkpointing
class ModelCheckpoint {
public:
    static void save(const Sequential& model, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filename);
        }
        
        auto params = const_cast<Sequential&>(model).parameters();
        
        // Write number of parameters
        size_t num_params = params.size();
        file.write(reinterpret_cast<const char*>(&num_params), sizeof(num_params));
        
        // Write each parameter
        for (const auto& param : params) {
            const auto& shape = param->data().shape();
            const auto& data = param->data().data();
            
            // Write shape
            size_t shape_size = shape.size();
            file.write(reinterpret_cast<const char*>(&shape_size), sizeof(shape_size));
            file.write(reinterpret_cast<const char*>(shape.data()), 
                      shape_size * sizeof(int));
            
            // Write data
            size_t data_size = data.size();
            file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
            file.write(reinterpret_cast<const char*>(data.data()), 
                      data_size * sizeof(float));
        }
        
        file.close();
    }
    
    static void load(Sequential& model, const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for reading: " + filename);
        }
        
        auto params = model.parameters();
        
        // Read number of parameters
        size_t num_params;
        file.read(reinterpret_cast<char*>(&num_params), sizeof(num_params));
        
        if (num_params != params.size()) {
            throw std::runtime_error("Parameter count mismatch in checkpoint");
        }
        
        // Read each parameter
        for (size_t i = 0; i < num_params; ++i) {
            // Read shape
            size_t shape_size;
            file.read(reinterpret_cast<char*>(&shape_size), sizeof(shape_size));
            
            std::vector<int> shape(shape_size);
            file.read(reinterpret_cast<char*>(shape.data()), 
                     shape_size * sizeof(int));
            
            // Read data
            size_t data_size;
            file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
            
            std::vector<float> data(data_size);
            file.read(reinterpret_cast<char*>(data.data()), 
                     data_size * sizeof(float));
            
            // Update parameter
            if (params[i]->data().shape() != shape) {
                throw std::runtime_error("Shape mismatch in checkpoint");
            }
            
            params[i]->data().data() = data;
        }
        
        file.close();
    }
};

// Example 1: CNN for image classification (MNIST-like)
void test_cnn_classification() {
    std::cout << "Testing CNN for image classification...\n";
    
    // Create CNN model: Conv -> ReLU -> MaxPool -> Conv -> ReLU -> MaxPool -> FC -> FC
    Sequential model;
    model.add<Conv2DLayer>(1, 32, 3, 1, 1);    // 28x28x1 -> 28x28x32
    model.add<MaxPool2DLayer>(2);              // 28x28x32 -> 14x14x32
    model.add<Conv2DLayer>(32, 64, 3, 1, 1);   // 14x14x32 -> 14x14x64
    model.add<MaxPool2DLayer>(2);              // 14x14x64 -> 7x7x64
    model.add<LinearLayer>(7*7*64, 128);       // Flatten and FC
    model.add<LinearLayer>(128, 10);           // Output layer
    
    // Generate dummy MNIST-like data
    std::vector<Tensor> inputs, targets;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 1000; ++i) {
        Tensor image({1, 1, 28, 28});
        for (auto& pixel : image.data()) {
            pixel = dist(gen);
        }
        
        Tensor label({1, 10});
        std::fill(label.data().begin(), label.data().end(), 0.0f);
        label.data()[i % 10] = 1.0f; // One-hot encoding
        
        inputs.push_back(image);
        targets.push_back(label);
    }
    
    // Training setup
    Adam optimizer(0.001f);
    DataLoader train_loader(inputs, targets, 32, true);
    StepLR scheduler(0.001f, 100, 0.5f);
    
    // Training loop
    int epochs = 5;
    for (int epoch = 0; epoch < epochs; ++epoch) {
        train_loader.reset();
        float epoch_loss = 0.0f;
        int batches = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (train_loader.has_next()) {
            auto batch = train_loader.next_batch();
            
            optimizer.zero_grad(model);
            
            float batch_loss = 0.0f;
            for (size_t i = 0; i < batch.first.size(); ++i) {
                Variable input(batch.first[i], false);
                Variable target(batch.second[i], false);
                
                // Forward pass
                Variable output = model.forward(input);
                
                // Apply softmax (simplified)
                // In practice, you'd use CrossEntropyLoss
                Variable loss = MSELoss::forward(output, target);
                batch_loss += loss.data()({0});
                
                // Backward pass
                loss.backward();
            }
            
            optimizer.step(model);
            epoch_loss += batch_loss / batch.first.size();
            batches++;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        scheduler.step();
        
        std::cout << "Epoch " << epoch + 1 << "/" << epochs 
                  << ", Loss: " << epoch_loss / batches
                  << ", Time: " << duration.count() << "ms"
                  << ", LR: " << scheduler.get_lr() << std::endl;
    }
    
    // Save model
    ModelCheckpoint::save(model, "cnn_model.bin");
    std::cout << "Model saved to cnn_model.bin\n";
}

// Example 2: RNN for sequence prediction
void test_rnn_sequence() {
    std::cout << "\nTesting RNN for sequence prediction...\n";
    
    // Create RNN model
    Sequential model;
    model.add<RNNLayer>(10, 50);        // input_size=10, hidden_size=50
    model.add<LinearLayer>(50, 1);      // output size=1
    
    // Generate sine wave sequence data
    std::vector<Tensor> sequences, targets;
    int seq_length = 20;
    int num_sequences = 500;
    
    for (int i = 0; i < num_sequences; ++i) {
        Tensor sequence({1, seq_length, 10});
        Tensor target({1, 1});
        
        float phase = (float)i / num_sequences * 2.0f * M_PI;
        
        // Generate sequence
        for (int t = 0; t < seq_length; ++t) {
            for (int f = 0; f < 10; ++f) {
                sequence({0, t, f}) = std::sin(phase + t * 0.1f + f * 0.05f);
            }
        }
        
        // Target is the next value in sequence
        target({0, 0}) = std::sin(phase + seq_length * 0.1f);
        
        sequences.push_back(sequence);
        targets.push_back(target);
    }
    
    // Training
    SGD optimizer(0.01f);
    
    for (int epoch = 0; epoch < 100; ++epoch) {
        float total_loss = 0.0f;
        
        for (size_t i = 0; i < sequences.size(); ++i) {
            optimizer.zero_grad(model);
            
            Variable input(sequences[i], false);
            Variable target(targets[i], false);
            
            // Process sequence (simplified - should process step by step)
            Variable output = model.forward(input);
            Variable loss = MSELoss::forward(output, target);
            
            total_loss += loss.data()({0});
            loss.backward();
            optimizer.step(model);
        }
        
        if (epoch % 20 == 0) {
            std::cout << "Epoch " << epoch << ", Loss: " << total_loss / sequences.size() << std::endl;
        }
    }
}

// Example 3: LSTM for text generation (simplified)
void test_lstm_text() {
    std::cout << "\nTesting LSTM for character-level text modeling...\n";
    
    // Create LSTM model
    Sequential model;
    model.add<LSTMLayer>(26, 128);      // 26 characters, hidden_size=128
    model.add<LinearLayer>(128, 26);    // Output vocabulary size
    
    // Generate dummy character sequence data
    std::vector<Tensor> sequences, targets;
    int seq_length = 50;
    int vocab_size = 26;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> char_dist(0, vocab_size - 1);
    
    for (int i = 0; i < 200; ++i) {
        Tensor sequence({1, vocab_size}); // One-hot encoded
        Tensor target({1, vocab_size});
        
        // Random character sequence
        std::fill(sequence.data().begin(), sequence.data().end(), 0.0f);
        std::fill(target.data().begin(), target.data().end(), 0.0f);
        
        int char_idx = char_dist(gen);
        int next_char = char_dist(gen);
        
        sequence.data()[char_idx] = 1.0f;
        target.data()[next_char] = 1.0f;
        
        sequences.push_back(sequence);
        targets.push_back(target);
    }
    
    // Training with regularization
    Adam optimizer(0.001f);
    
    for (int epoch = 0; epoch < 50; ++epoch) {
        float total_loss = 0.0f;
        
        for (size_t i = 0; i < sequences.size(); ++i) {
            optimizer.zero_grad(model);
            
            Variable input(sequences[i], false);
            Variable target(targets[i], false);
            
            Variable output = model.forward(input);
            Variable loss = MSELoss::forward(output, target);
            
            total_loss += loss.data()({0});
            loss.backward();
            optimizer.step(model);
        }
        
        if (epoch % 10 == 0) {
            std::cout << "Epoch " << epoch << ", Loss: " << total_loss / sequences.size() << std::endl;
        }
    }
}

int main() {
    try {
        // Test CUDA availability
        if (CudaDevice::getInstance().isAvailable()) {
            std::cout << "CUDA is available! GPU acceleration enabled.\n";
        } else {
            std::cout << "CUDA not available. Using CPU only.\n";
        }
        
        // Run all examples
        test_cnn_classification();
        test_rnn_sequence();
        test_lstm_text();
        
        std::cout << "\nAll tests completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
