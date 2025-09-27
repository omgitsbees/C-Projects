#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <random>
#include <cmath>

// Forward declarations
class Tensor;
class Variable;
class Function;

// Tensor class for n-dimensional arrays
class Tensor {
private:
    std::vector<float> data_;
    std::vector<int> shape_;
    int size_;
    
    void calculate_size() {
        size_ = 1;
        for (int dim : shape_) size_ *= dim;
    }

public:
    Tensor() : size_(0) {}
    
    Tensor(const std::vector<int>& shape) : shape_(shape) {
        calculate_size();
        data_.resize(size_, 0.0f);
    }
    
    Tensor(const std::vector<int>& shape, const std::vector<float>& data) 
        : shape_(shape), data_(data) {
        calculate_size();
        if (data_.size() != size_) {
            throw std::runtime_error("Data size doesn't match tensor shape");
        }
    }
    
    // Basic operations
    float& operator()(const std::vector<int>& indices) {
        int flat_idx = 0, multiplier = 1;
        for (int i = shape_.size() - 1; i >= 0; --i) {
            flat_idx += indices[i] * multiplier;
            multiplier *= shape_[i];
        }
        return data_[flat_idx];
    }
    
    const float& operator()(const std::vector<int>& indices) const {
        int flat_idx = 0, multiplier = 1;
        for (int i = shape_.size() - 1; i >= 0; --i) {
            flat_idx += indices[i] * multiplier;
            multiplier *= shape_[i];
        }
        return data_[flat_idx];
    }
    
    // Getters
    const std::vector<int>& shape() const { return shape_; }
    const std::vector<float>& data() const { return data_; }
    std::vector<float>& data() { return data_; }
    int size() const { return size_; }
    
    // Reshape
    Tensor reshape(const std::vector<int>& new_shape) const {
        int new_size = 1;
        for (int dim : new_shape) new_size *= dim;
        if (new_size != size_) {
            throw std::runtime_error("Cannot reshape: size mismatch");
        }
        return Tensor(new_shape, data_);
    }
    
    // Initialize with random values
    void random_normal(float mean = 0.0f, float std = 1.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(mean, std);
        
        for (auto& val : data_) {
            val = dist(gen);
        }
    }
    
    void xavier_init() {
        if (shape_.size() < 2) return;
        float fan_in = shape_[0];
        float fan_out = shape_[1];
        float std = std::sqrt(2.0f / (fan_in + fan_out));
        random_normal(0.0f, std);
    }
};

// Variable class for automatic differentiation
class Variable {
private:
    std::shared_ptr<Tensor> data_;
    std::shared_ptr<Tensor> grad_;
    std::shared_ptr<Function> grad_fn_;
    bool requires_grad_;

public:
    Variable(const Tensor& data, bool requires_grad = false) 
        : data_(std::make_shared<Tensor>(data)), requires_grad_(requires_grad) {
        if (requires_grad_) {
            grad_ = std::make_shared<Tensor>(data.shape());
        }
    }
    
    // Getters
    Tensor& data() { return *data_; }
    const Tensor& data() const { return *data_; }
    Tensor& grad() { 
        if (!grad_) throw std::runtime_error("Gradient not available");
        return *grad_; 
    }
    bool requires_grad() const { return requires_grad_; }
    
    // Set gradient function for backprop
    void set_grad_fn(std::shared_ptr<Function> fn) { grad_fn_ = fn; }
    
    // Backward pass
    void backward();
    
    // Zero gradients
    void zero_grad() {
        if (grad_) {
            std::fill(grad_->data().begin(), grad_->data().end(), 0.0f);
        }
    }
};

// Abstract base class for all operations
class Function {
public:
    virtual ~Function() = default;
    virtual std::vector<Variable> forward(const std::vector<Variable>& inputs) = 0;
    virtual std::vector<Variable> backward(const Variable& grad_output) = 0;
};

// Matrix multiplication function
class MatMulFunction : public Function {
private:
    Variable input1_, input2_;
    
public:
    std::vector<Variable> forward(const std::vector<Variable>& inputs) override {
        if (inputs.size() != 2) {
            throw std::runtime_error("MatMul requires exactly 2 inputs");
        }
        
        input1_ = inputs[0];
        input2_ = inputs[1];
        
        const auto& shape1 = input1_.data().shape();
        const auto& shape2 = input2_.data().shape();
        
        if (shape1.size() != 2 || shape2.size() != 2 || shape1[1] != shape2[0]) {
            throw std::runtime_error("Invalid shapes for matrix multiplication");
        }
        
        std::vector<int> out_shape = {shape1[0], shape2[1]};
        Tensor result(out_shape);
        
        // Perform matrix multiplication
        for (int i = 0; i < shape1[0]; ++i) {
            for (int j = 0; j < shape2[1]; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < shape1[1]; ++k) {
                    sum += input1_.data()({i, k}) * input2_.data()({k, j});
                }
                result({i, j}) = sum;
            }
        }
        
        Variable output(result, input1_.requires_grad() || input2_.requires_grad());
        return {output};
    }
    
    std::vector<Variable> backward(const Variable& grad_output) override {
        // Gradients: dL/dA = dL/dC * B^T, dL/dB = A^T * dL/dC
        std::vector<Variable> grads;
        
        if (input1_.requires_grad()) {
            // Compute gradient w.r.t. first input
            // This is a simplified version - full implementation would handle batch dims
            const auto& shape1 = input1_.data().shape();
            Tensor grad1(shape1);
            // grad1 = grad_output @ input2_.T (simplified)
            grads.push_back(Variable(grad1));
        }
        
        if (input2_.requires_grad()) {
            // Compute gradient w.r.t. second input
            const auto& shape2 = input2_.data().shape();
            Tensor grad2(shape2);
            // grad2 = input1_.T @ grad_output (simplified)
            grads.push_back(Variable(grad2));
        }
        
        return grads;
    }
};

// Activation functions
class ReLUFunction : public Function {
private:
    Variable input_;
    
public:
    std::vector<Variable> forward(const std::vector<Variable>& inputs) override {
        input_ = inputs[0];
        Tensor result = input_.data();
        
        for (auto& val : result.data()) {
            val = std::max(0.0f, val);
        }
        
        Variable output(result, input_.requires_grad());
        return {output};
    }
    
    std::vector<Variable> backward(const Variable& grad_output) override {
        Tensor grad_input = grad_output.data();
        
        for (int i = 0; i < grad_input.size(); ++i) {
            if (input_.data().data()[i] <= 0.0f) {
                grad_input.data()[i] = 0.0f;
            }
        }
        
        return {Variable(grad_input)};
    }
};

// Layer base class
class Layer {
public:
    virtual ~Layer() = default;
    virtual Variable forward(const Variable& input) = 0;
    virtual void update_weights(float learning_rate) = 0;
    virtual std::vector<Variable*> parameters() = 0;
};

// Linear/Dense layer
class LinearLayer : public Layer {
private:
    Variable weights_;
    Variable bias_;
    int in_features_;
    int out_features_;
    
public:
    LinearLayer(int in_features, int out_features) 
        : in_features_(in_features), out_features_(out_features),
          weights_(Tensor({in_features, out_features}), true),
          bias_(Tensor({out_features}), true) {
        
        // Xavier initialization
        weights_.data().xavier_init();
        
        // Zero bias initialization
        std::fill(bias_.data().data().begin(), bias_.data().data().end(), 0.0f);
    }
    
    Variable forward(const Variable& input) override {
        // y = xW + b
        auto matmul_fn = std::make_shared<MatMulFunction>();
        auto result = matmul_fn->forward({input, weights_})[0];
        
        // Add bias (simplified - would need proper broadcasting)
        for (int i = 0; i < result.data().shape()[0]; ++i) {
            for (int j = 0; j < result.data().shape()[1]; ++j) {
                result.data()({i, j}) += bias_.data()({j});
            }
        }
        
        return result;
    }
    
    void update_weights(float learning_rate) override {
        if (weights_.requires_grad() && weights_.grad().size() > 0) {
            for (int i = 0; i < weights_.data().size(); ++i) {
                weights_.data().data()[i] -= learning_rate * weights_.grad().data()[i];
            }
        }
        
        if (bias_.requires_grad() && bias_.grad().size() > 0) {
            for (int i = 0; i < bias_.data().size(); ++i) {
                bias_.data().data()[i] -= learning_rate * bias_.grad().data()[i];
            }
        }
    }
    
    std::vector<Variable*> parameters() override {
        return {&weights_, &bias_};
    }
};

// Simple Sequential model
class Sequential {
private:
    std::vector<std::unique_ptr<Layer>> layers_;
    
public:
    template<typename LayerType, typename... Args>
    void add(Args&&... args) {
        layers_.push_back(std::make_unique<LayerType>(std::forward<Args>(args)...));
    }
    
    Variable forward(const Variable& input) {
        Variable current = input;
        for (auto& layer : layers_) {
            current = layer->forward(current);
        }
        return current;
    }
    
    void update_weights(float learning_rate) {
        for (auto& layer : layers_) {
            layer->update_weights(learning_rate);
        }
    }
    
    std::vector<Variable*> parameters() {
        std::vector<Variable*> params;
        for (auto& layer : layers_) {
            auto layer_params = layer->parameters();
            params.insert(params.end(), layer_params.begin(), layer_params.end());
        }
        return params;
    }
};

// Loss functions
class MSELoss {
public:
    static Variable forward(const Variable& predictions, const Variable& targets) {
        const auto& pred_data = predictions.data().data();
        const auto& target_data = targets.data().data();
        
        float loss = 0.0f;
        for (int i = 0; i < pred_data.size(); ++i) {
            float diff = pred_data[i] - target_data[i];
            loss += diff * diff;
        }
        loss /= pred_data.size();
        
        return Variable(Tensor({1}, {loss}), true);
    }
};

// Simple optimizer
class SGD {
private:
    float learning_rate_;
    
public:
    SGD(float learning_rate) : learning_rate_(learning_rate) {}
    
    void step(Sequential& model) {
        model.update_weights(learning_rate_);
    }
    
    void zero_grad(Sequential& model) {
        auto params = model.parameters();
        for (auto param : params) {
            param->zero_grad();
        }
    }
};