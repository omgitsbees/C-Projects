#pragma once
#include "neural_framework.h"
#include <algorithm>

// Convolution 2D Layer
class Conv2DLayer : public Layer {
private:
    Variable weights_;  // Shape: [out_channels, in_channels, kernel_h, kernel_w]
    Variable bias_;     // Shape: [out_channels]
    int in_channels_, out_channels_;
    int kernel_size_, stride_, padding_;
    
    // Helper function to perform convolution
    Tensor convolve2d(const Tensor& input, const Tensor& kernel, int stride, int padding) {
        const auto& in_shape = input.shape();
        const auto& ker_shape = kernel.shape();
        
        // Input: [batch, in_channels, height, width]
        // Kernel: [out_channels, in_channels, ker_h, ker_w]
        int batch_size = in_shape[0];
        int in_h = in_shape[2], in_w = in_shape[3];
        int ker_h = ker_shape[2], ker_w = ker_shape[3];
        
        int out_h = (in_h + 2 * padding - ker_h) / stride + 1;
        int out_w = (in_w + 2 * padding - ker_w) / stride + 1;
        
        Tensor output({batch_size, out_channels_, out_h, out_w});
        
        // Simplified convolution (without proper padding handling)
        for (int b = 0; b < batch_size; ++b) {
            for (int oc = 0; oc < out_channels_; ++oc) {
                for (int oh = 0; oh < out_h; ++oh) {
                    for (int ow = 0; ow < out_w; ++ow) {
                        float sum = 0.0f;
                        
                        for (int ic = 0; ic < in_channels_; ++ic) {
                            for (int kh = 0; kh < ker_h; ++kh) {
                                for (int kw = 0; kw < ker_w; ++kw) {
                                    int ih = oh * stride + kh - padding;
                                    int iw = ow * stride + kw - padding;
                                    
                                    if (ih >= 0 && ih < in_h && iw >= 0 && iw < in_w) {
                                        sum += input({b, ic, ih, iw}) * 
                                               kernel({oc, ic, kh, kw});
                                    }
                                }
                            }
                        }
                        
                        output({b, oc, oh, ow}) = sum + bias_.data()({oc});
                    }
                }
            }
        }
        
        return output;
    }
    
public:
    Conv2DLayer(int in_channels, int out_channels, int kernel_size, 
                int stride = 1, int padding = 0)
        : in_channels_(in_channels), out_channels_(out_channels), 
          kernel_size_(kernel_size), stride_(stride), padding_(padding),
          weights_(Tensor({out_channels, in_channels, kernel_size, kernel_size}), true),
          bias_(Tensor({out_channels}), true) {
        
        // Kaiming initialization for conv layers
        float fan_in = in_channels * kernel_size * kernel_size;
        float std = std::sqrt(2.0f / fan_in);
        weights_.data().random_normal(0.0f, std);
        
        std::fill(bias_.data().data().begin(), bias_.data().data().end(), 0.0f);
    }
    
    Variable forward(const Variable& input) override {
        Tensor output = convolve2d(input.data(), weights_.data(), stride_, padding_);
        return Variable(output, input.requires_grad() || weights_.requires_grad());
    }
    
    void update_weights(float learning_rate) override {
        if (weights_.requires_grad()) {
            for (int i = 0; i < weights_.data().size(); ++i) {
                weights_.data().data()[i] -= learning_rate * weights_.grad().data()[i];
            }
        }
        
        if (bias_.requires_grad()) {
            for (int i = 0; i < bias_.data().size(); ++i) {
                bias_.data().data()[i] -= learning_rate * bias_.grad().data()[i];
            }
        }
    }
    
    std::vector<Variable*> parameters() override {
        return {&weights_, &bias_};
    }
};

// MaxPool2D Layer
class MaxPool2DLayer : public Layer {
private:
    int kernel_size_, stride_;
    Tensor last_input_; // Store for backward pass
    Tensor max_indices_; // Store indices of max values for backward pass
    
public:
    MaxPool2DLayer(int kernel_size, int stride = -1) 
        : kernel_size_(kernel_size), stride_(stride == -1 ? kernel_size : stride) {}
    
    Variable forward(const Variable& input) override {
        last_input_ = input.data();
        const auto& shape = input.data().shape();
        
        // Input: [batch, channels, height, width]
        int batch = shape[0], channels = shape[1];
        int in_h = shape[2], in_w = shape[3];
        
        int out_h = (in_h - kernel_size_) / stride_ + 1;
        int out_w = (in_w - kernel_size_) / stride_ + 1;
        
        Tensor output({batch, channels, out_h, out_w});
        max_indices_ = Tensor({batch, channels, out_h, out_w});
        
        for (int b = 0; b < batch; ++b) {
            for (int c = 0; c < channels; ++c) {
                for (int oh = 0; oh < out_h; ++oh) {
                    for (int ow = 0; ow < out_w; ++ow) {
                        float max_val = -std::numeric_limits<float>::infinity();
                        int max_idx = -1;
                        
                        for (int kh = 0; kh < kernel_size_; ++kh) {
                            for (int kw = 0; kw < kernel_size_; ++kw) {
                                int ih = oh * stride_ + kh;
                                int iw = ow * stride_ + kw;
                                
                                if (ih < in_h && iw < in_w) {
                                    float val = input.data()({b, c, ih, iw});
                                    if (val > max_val) {
                                        max_val = val;
                                        max_idx = ih * in_w + iw;
                                    }
                                }
                            }
                        }
                        
                        output({b, c, oh, ow}) = max_val;
                        max_indices_({b, c, oh, ow}) = max_idx;
                    }
                }
            }
        }
        
        return Variable(output, input.requires_grad());
    }
    
    void update_weights(float learning_rate) override {
        // No parameters to update
    }
    
    std::vector<Variable*> parameters() override {
        return {}; // No parameters
    }
};

// Simple RNN Cell
class RNNCell {
private:
    Variable W_ih_; // input to hidden
    Variable W_hh_; // hidden to hidden  
    Variable bias_;
    int input_size_, hidden_size_;
    
public:
    RNNCell(int input_size, int hidden_size) 
        : input_size_(input_size), hidden_size_(hidden_size),
          W_ih_(Tensor({input_size, hidden_size}), true),
          W_hh_(Tensor({hidden_size, hidden_size}), true),
          bias_(Tensor({hidden_size}), true) {
        
        // Xavier initialization
        W_ih_.data().xavier_init();
        W_hh_.data().xavier_init();
        std::fill(bias_.data().data().begin(), bias_.data().data().end(), 0.0f);
    }
    
    Variable forward(const Variable& input, const Variable& hidden) {
        // h_new = tanh(input @ W_ih + hidden @ W_hh + bias)
        
        // Matrix multiplications (simplified)
        Tensor ih_result({1, hidden_size_});
        Tensor hh_result({1, hidden_size_});
        
        // input @ W_ih
        for (int h = 0; h < hidden_size_; ++h) {
            float sum = 0.0f;
            for (int i = 0; i < input_size_; ++i) {
                sum += input.data()({0, i}) * W_ih_.data()({i, h});
            }
            ih_result({0, h}) = sum;
        }
        
        // hidden @ W_hh  
        for (int h = 0; h < hidden_size_; ++h) {
            float sum = 0.0f;
            for (int i = 0; i < hidden_size_; ++i) {
                sum += hidden.data()({0, i}) * W_hh_.data()({i, h});
            }
            hh_result({0, h}) = sum;
        }
        
        // Combine and apply tanh
        Tensor new_hidden({1, hidden_size_});
        for (int h = 0; h < hidden_size_; ++h) {
            float val = ih_result({0, h}) + hh_result({0, h}) + bias_.data()({h});
            new_hidden({0, h}) = std::tanh(val);
        }
        
        return Variable(new_hidden, true);
    }
    
    std::vector<Variable*> parameters() {
        return {&W_ih_, &W_hh_, &bias_};
    }
};

// RNN Layer
class RNNLayer : public Layer {
private:
    RNNCell cell_;
    int hidden_size_;
    Variable hidden_state_;
    
public:
    RNNLayer(int input_size, int hidden_size) 
        : cell_(input_size, hidden_size), hidden_size_(hidden_size),
          hidden_state_(Tensor({1, hidden_size}), true) {
        
        // Initialize hidden state to zeros
        std::fill(hidden_state_.data().data().begin(), 
                 hidden_state_.data().data().end(), 0.0f);
    }
    
    Variable forward(const Variable& input) override {
        // For simplicity, process single time step
        hidden_state_ = cell_.forward(input, hidden_state_);
        return hidden_state_;
    }
    
    void reset_hidden() {
        std::fill(hidden_state_.data().data().begin(), 
                 hidden_state_.data().data().end(), 0.0f);
    }
    
    void update_weights(float learning_rate) override {
        auto params = cell_.parameters();
        for (auto param : params) {
            if (param->requires_grad()) {
                for (int i = 0; i < param->data().size(); ++i) {
                    param->data().data()[i] -= learning_rate * param->grad().data()[i];
                }
            }
        }
    }
    
    std::vector<Variable*> parameters() override {
        return cell_.parameters();
    }
};

// LSTM Cell (simplified implementation)
class LSTMCell {
private:
    Variable W_ii_, W_if_, W_ig_, W_io_; // input to input/forget/gate/output
    Variable W_hi_, W_hf_, W_hg_, W_ho_; // hidden to input/forget/gate/output
    Variable b_ii_, b_if_, b_ig_, b_io_; // biases for input/forget/gate/output
    Variable b_hi_, b_hf_, b_hg_, b_ho_; // hidden biases
    
    int input_size_, hidden_size_;
    
    float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }
    
public:
    LSTMCell(int input_size, int hidden_size) 
        : input_size_(input_size), hidden_size_(hidden_size),
          W_ii_(Tensor({input_size, hidden_size}), true),
          W_if_(Tensor({input_size, hidden_size}), true),
          W_ig_(Tensor({input_size, hidden_size}), true),
          W_io_(Tensor({input_size, hidden_size}), true),
          W_hi_(Tensor({hidden_size, hidden_size}), true),
          W_hf_(Tensor({hidden_size, hidden_size}), true),
          W_hg_(Tensor({hidden_size, hidden_size}), true),
          W_ho_(Tensor({hidden_size, hidden_size}), true),
          b_ii_(Tensor({hidden_size}), true),
          b_if_(Tensor({hidden_size}), true),
          b_ig_(Tensor({hidden_size}), true),
          b_io_(Tensor({hidden_size}), true),
          b_hi_(Tensor({hidden_size}), true),
          b_hf_(Tensor({hidden_size}), true),
          b_hg_(Tensor({hidden_size}), true),
          b_ho_(Tensor({hidden_size}), true) {
        
        // Initialize all weight matrices
        std::vector<Variable*> weights = {&W_ii_, &W_if_, &W_ig_, &W_io_,
                                         &W_hi_, &W_hf_, &W_hg_, &W_ho_};
        for (auto w : weights) {
            w->data().xavier_init();
        }
        
        // Initialize biases to zero except forget gate (set to 1)
        std::vector<Variable*> biases = {&b_ii_, &b_ig_, &b_io_, &b_hi_, &b_hg_, &b_ho_};
        for (auto b : biases) {
            std::fill(b->data().data().begin(), b->data().data().end(), 0.0f);
        }
        
        // Forget gate bias initialized to 1 (helps with gradient flow)
        std::fill(b_if_.data().data().begin(), b_if_.data().data().end(), 1.0f);
        std::fill(b_hf_.data().data().begin(), b_hf_.data().data().end(), 1.0f);
    }
    
    std::pair<Variable, Variable> forward(const Variable& input, 
                                         const Variable& hidden, 
                                         const Variable& cell) {
        // LSTM forward pass
        // i_t = σ(W_ii * x_t + b_ii + W_hi * h_{t-1} + b_hi)
        // f_t = σ(W_if * x_t + b_if + W_hf * h_{t-1} + b_hf)  
        // g_t = tanh(W_ig * x_t + b_ig + W_hg * h_{t-1} + b_hg)
        // o_t = σ(W_io * x_t + b_io + W_ho * h_{t-1} + b_ho)
        // c_t = f_t * c_{t-1} + i_t * g_t
        // h_t = o_t * tanh(c_t)
        
        Tensor i_gate({1, hidden_size_}), f_gate({1, hidden_size_});
        Tensor g_gate({1, hidden_size_}), o_gate({1, hidden_size_});
        
        // Compute gates (simplified matrix multiplication)
        for (int h = 0; h < hidden_size_; ++h) {
            // Input gate
            float i_val = 0.0f;
            for (int i = 0; i < input_size_; ++i) {
                i_val += input.data()({0, i}) * W_ii_.data()({i, h});
            }
            for (int i = 0; i < hidden_size_; ++i) {
                i_val += hidden.data()({0, i}) * W_hi_.data()({i, h});
            }
            i_val += b_ii_.data()({h}) + b_hi_.data()({h});
            i_gate({0, h}) = sigmoid(i_val);
            
            // Forget gate
            float f_val = 0.0f;
            for (int i = 0; i < input_size_; ++i) {
                f_val += input.data()({0, i}) * W_if_.data()({i, h});
            }
            for (int i = 0; i < hidden_size_; ++i) {
                f_val += hidden.data()({0, i}) * W_hf_.data()({i, h});
            }
            f_val += b_if_.data()({h}) + b_hf_.data()({h});
            f_gate({0, h}) = sigmoid(f_val);
            
            // Gate gate (candidate values)
            float g_val = 0.0f;
            for (int i = 0; i < input_size_; ++i) {
                g_val += input.data()({0, i}) * W_ig_.data()({i, h});
            }
            for (int i = 0; i < hidden_size_; ++i) {
                g_val += hidden.data()({0, i}) * W_hg_.data()({i, h});
            }
            g_val += b_ig_.data()({h}) + b_hg_.data()({h});
            g_gate({0, h}) = std::tanh(g_val);
            
            // Output gate
            float o_val = 0.0f;
            for (int i = 0; i < input_size_; ++i) {
                o_val += input.data()({0, i}) * W_io_.data()({i, h});
            }
            for (int i = 0; i < hidden_size_; ++i) {
                o_val += hidden.data()({0, i}) * W_ho_.data()({i, h});
            }
            o_val += b_io_.data()({h}) + b_ho_.data()({h});
            o_gate({0, h}) = sigmoid(o_val);
        }
        
        // Update cell state and hidden state
        Tensor new_cell({1, hidden_size_}), new_hidden({1, hidden_size_});
        
        for (int h = 0; h < hidden_size_; ++h) {
            // c_t = f_t * c_{t-1} + i_t * g_t
            new_cell({0, h}) = f_gate({0, h}) * cell.data()({0, h}) + 
                              i_gate({0, h}) * g_gate({0, h});
            
            // h_t = o_t * tanh(c_t)
            new_hidden({0, h}) = o_gate({0, h}) * std::tanh(new_cell({0, h}));
        }
        
        return {Variable(new_hidden, true), Variable(new_cell, true)};
    }
    
    std::vector<Variable*> parameters() {
        return {&W_ii_, &W_if_, &W_ig_, &W_io_, &W_hi_, &W_hf_, &W_hg_, &W_ho_,
                &b_ii_, &b_if_, &b_ig_, &b_io_, &b_hi_, &b_hf_, &b_hg_, &b_ho_};
    }
};

// LSTM Layer
class LSTMLayer : public Layer {
private:
    LSTMCell cell_;
    int hidden_size_;
    Variable hidden_state_, cell_state_;
    
public:
    LSTMLayer(int input_size, int hidden_size) 
        : cell_(input_size, hidden_size), hidden_size_(hidden_size),
          hidden_state_(Tensor({1, hidden_size}), true),
          cell_state_(Tensor({1, hidden_size}), true) {
        
        // Initialize states to zeros
        std::fill(hidden_state_.data().data().begin(), 
                 hidden_state_.data().data().end(), 0.0f);
        std::fill(cell_state_.data().data().begin(), 
                 cell_state_.data().data().end(), 0.0f);
    }
    
    Variable forward(const Variable& input) override {
        auto states = cell_.forward(input, hidden_state_, cell_state_);
        hidden_state_ = states.first;
        cell_state_ = states.second;
        return hidden_state_;
    }
    
    void reset_states() {
        std::fill(hidden_state_.data().data().begin(), 
                 hidden_state_.data().data().end(), 0.0f);
        std::fill(cell_state_.data().data().begin(), 
                 cell_state_.data().data().end(), 0.0f);
    }
    
    void update_weights(float learning_rate) override {
        auto params = cell_.parameters();
        for (auto param : params) {
            if (param->requires_grad()) {
                for (int i = 0; i < param->data().size(); ++i) {
                    param->data().data()[i] -= learning_rate * param->grad().data()[i];
                }
            }
        }
    }
    
    std::vector<Variable*> parameters() override {
        return cell_.parameters();
    }
};

// Batch Normalization Layer
class BatchNormLayer : public Layer {
private:
    Variable gamma_, beta_; // learnable parameters
    Variable running_mean_, running_var_; // running statistics
    float momentum_, eps_;
    bool training_;
    
public:
    BatchNormLayer(int num_features, float momentum = 0.1f, float eps = 1e-5f)
        : momentum_(momentum), eps_(eps), training_(true),
          gamma_(Tensor({num_features}), true),
          beta_(Tensor({num_features}), true),
          running_mean_(Tensor({num_features}), false),
          running_var_(Tensor({num_features}), false) {
        
        // Initialize gamma to 1, beta to 0
        std::fill(gamma_.data().data().begin(), gamma_.data().data().end(), 1.0f);
        std::fill(beta_.data().data().begin(), beta_.data().data().end(), 0.0f);
        
        // Initialize running stats
        std::fill(running_mean_.data().data().begin(), running_mean_.data().data().end(), 0.0f);
        std::fill(running_var_.data().data().begin(), running_var_.data().data().end(), 1.0f);
    }
    
    Variable forward(const Variable& input) override {
        const auto& shape = input.data().shape();
        Tensor output(shape);
        
        if (training_) {
            // Compute batch statistics (simplified for 2D input)
            int batch_size = shape[0];
            int num_features = shape[1];
            
            // Compute mean and variance
            std::vector<float> batch_mean(num_features, 0.0f);
            std::vector<float> batch_var(num_features, 0.0f);
            
            // Mean
            for (int f = 0; f < num_features; ++f) {
                for (int b = 0; b < batch_size; ++b) {
                    batch_mean[f] += input.data()({b, f});
                }
                batch_mean[f] /= batch_size;
            }
            
            // Variance
            for (int f = 0; f < num_features; ++f) {
                for (int b = 0; b < batch_size; ++b) {
                    float diff = input.data()({b, f}) - batch_mean[f];
                    batch_var[f] += diff * diff;
                }
                batch_var[f] /= batch_size;
            }
            
            // Normalize and scale
            for (int b = 0; b < batch_size; ++b) {
                for (int f = 0; f < num_features; ++f) {
                    float normalized = (input.data()({b, f}) - batch_mean[f]) / 
                                     std::sqrt(batch_var[f] + eps_);
                    output({b, f}) = gamma_.data()({f}) * normalized + beta_.data()({f});
                }
            }
            
            // Update running statistics
            for (int f = 0; f < num_features; ++f) {
                running_mean_.data().data()[f] = (1.0f - momentum_) * running_mean_.data()({f}) + 
                                               momentum_ * batch_mean[f];
                running_var_.data().data()[f] = (1.0f - momentum_) * running_var_.data()({f}) + 
                                              momentum_ * batch_var[f];
            }
        } else {
            // Use running statistics for inference
            int batch_size = shape[0];
            int num_features = shape[1];
            
            for (int b = 0; b < batch_size; ++b) {
                for (int f = 0; f < num_features; ++f) {
                    float normalized = (input.data()({b, f}) - running_mean_.data()({f})) / 
                                     std::sqrt(running_var_.data()({f}) + eps_);
                    output({b, f}) = gamma_.data()({f}) * normalized + beta_.data()({f});
                }
            }
        }
        
        return Variable(output, input.requires_grad());
    }
    
    void set_training(bool training) { training_ = training; }
    
    void update_weights(float learning_rate) override {
        if (gamma_.requires_grad()) {
            for (int i = 0; i < gamma_.data().size(); ++i) {
                gamma_.data().data()[i] -= learning_rate * gamma_.grad().data()[i];
            }
        }
        
        if (beta_.requires_grad()) {
            for (int i = 0; i < beta_.data().size(); ++i) {
                beta_.data().data()[i] -= learning_rate * beta_.grad().data()[i];
            }
        }
    }
    
    std::vector<Variable*> parameters() override {
        return {&gamma_, &beta_};
    }
};

// Dropout Layer
class DropoutLayer : public Layer {
private:
    float p_;
    bool training_;
    Tensor mask_;
    
public:
    DropoutLayer(float p = 0.5f) : p_(p), training_(true) {}
    
    Variable forward(const Variable& input) override {
        if (!training_ || p_ == 0.0f) {
            return input;
        }
        
        Tensor output = input.data();
        mask_ = Tensor(input.data().shape());
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution dist(1.0f - p_);
        
        // Apply dropout mask
        for (int i = 0; i < output.size(); ++i) {
            bool keep = dist(gen);
            mask_.data()[i] = keep ? 1.0f / (1.0f - p_) : 0.0f; // Scale by 1/(1-p)
            output.data()[i] *= mask_.data()[i];
        }
        
        return Variable(output, input.requires_grad());
    }
    
    void set_training(bool training) { training_ = training; }
    
    void update_weights(float learning_rate) override {
        // No parameters to update
    }
    
    std::vector<Variable*> parameters() override {
        return {}; // No parameters
    }