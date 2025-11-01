#pragma once

#ifdef USE_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cudnn.h>
#endif
  
#include <memory>
#include <stdexcept>

// CUDA utilities and device management
class CudaDevice {
private:
    static std::unique_ptr<CudaDevice> instance_;
    int device_id_;
    
#ifdef USE_CUDA
    cublasHandle_t cublas_handle_;
    cudnnHandle_t cudnn_handle_;
#endif

public:
    static CudaDevice& getInstance() {
        if (!instance_) {
            instance_ = std::make_unique<CudaDevice>();
        }
        return *instance_;
    }
    
    CudaDevice(int device_id = 0) : device_id_(device_id) {
#ifdef USE_CUDA
        cudaSetDevice(device_id_);
        
        // Initialize cuBLAS
        cublasStatus_t stat = cublasCreate(&cublas_handle_);
        if (stat != CUBLAS_STATUS_SUCCESS) {
            throw std::runtime_error("cuBLAS initialization failed");
        }
        
        // Initialize cuDNN
        cudnnStatus_t cudnn_stat = cudnnCreate(&cudnn_handle_);
        if (cudnn_stat != CUDNN_STATUS_SUCCESS) {
            throw std::runtime_error("cuDNN initialization failed");
        }
#endif
    }
    
    ~CudaDevice() {
#ifdef USE_CUDA
        if (cublas_handle_) cublasDestroy(cublas_handle_);
        if (cudnn_handle_) cudnnDestroy(cudnn_handle_);
#endif
    }
    
#ifdef USE_CUDA
    cublasHandle_t getCublasHandle() { return cublas_handle_; }
    cudnnHandle_t getCudnnHandle() { return cudnn_handle_; }
#endif
    
    bool isAvailable() {
#ifdef USE_CUDA
        int device_count;
        cudaError_t error = cudaGetDeviceCount(&device_count);
        return error == cudaSuccess && device_count > 0;
#else
        return false;
#endif
    }
};

// GPU Memory management
class GpuTensor {
private:
    float* device_ptr_;
    std::vector<int> shape_;
    int size_;
    bool owns_data_;
    
public:
    GpuTensor() : device_ptr_(nullptr), size_(0), owns_data_(false) {}
    
    GpuTensor(const std::vector<int>& shape) : shape_(shape), owns_data_(true) {
        size_ = 1;
        for (int dim : shape_) size_ *= dim;
        
#ifdef USE_CUDA
        cudaError_t err = cudaMalloc(&device_ptr_, size_ * sizeof(float));
        if (err != cudaSuccess) {
            throw std::runtime_error("CUDA memory allocation failed: " + 
                                   std::string(cudaGetErrorString(err)));
        }
#else
        throw std::runtime_error("CUDA not available");
#endif
    }
    
    ~GpuTensor() {
        if (owns_data_ && device_ptr_) {
#ifdef USE_CUDA
            cudaFree(device_ptr_);
#endif
        }
    }
    
    // Move constructor and assignment
    GpuTensor(GpuTensor&& other) noexcept 
        : device_ptr_(other.device_ptr_), shape_(std::move(other.shape_)), 
          size_(other.size_), owns_data_(other.owns_data_) {
        other.device_ptr_ = nullptr;
        other.owns_data_ = false;
    }
    
    GpuTensor& operator=(GpuTensor&& other) noexcept {
        if (this != &other) {
            if (owns_data_ && device_ptr_) {
#ifdef USE_CUDA
                cudaFree(device_ptr_);
#endif
            }
            device_ptr_ = other.device_ptr_;
            shape_ = std::move(other.shape_);
            size_ = other.size_;
            owns_data_ = other.owns_data_;
            
            other.device_ptr_ = nullptr;
            other.owns_data_ = false;
        }
        return *this;
    }
    
    // Copy data from CPU tensor
    void copyFromCpu(const Tensor& cpu_tensor) {
#ifdef USE_CUDA
        if (cpu_tensor.size() != size_) {
            throw std::runtime_error("Size mismatch in copyFromCpu");
        }
        
        cudaError_t err = cudaMemcpy(device_ptr_, cpu_tensor.data().data(), 
                                   size_ * sizeof(float), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            throw std::runtime_error("CUDA memcpy H2D failed: " + 
                                   std::string(cudaGetErrorString(err)));
        }
#endif
    }
    
    // Copy data to CPU tensor
    void copyToCpu(Tensor& cpu_tensor) const {
#ifdef USE_CUDA
        if (cpu_tensor.size() != size_) {
            throw std::runtime_error("Size mismatch in copyToCpu");
        }
        
        cudaError_t err = cudaMemcpy(cpu_tensor.data().data(), device_ptr_, 
                                   size_ * sizeof(float), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            throw std::runtime_error("CUDA memcpy D2H failed: " + 
                                   std::string(cudaGetErrorString(err)));
        }
#endif
    }
    
    float* data() { return device_ptr_; }
    const float* data() const { return device_ptr_; }
    const std::vector<int>& shape() const { return shape_; }
    int size() const { return size_; }
};

#ifdef USE_CUDA

// CUDA kernels for basic operations
__global__ void relu_forward_kernel(const float* input, float* output, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        output[idx] = fmaxf(0.0f, input[idx]);
    }
}

__global__ void relu_backward_kernel(const float* input, const float* grad_output, 
                                   float* grad_input, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        grad_input[idx] = input[idx] > 0.0f ? grad_output[idx] : 0.0f;
    }
}

__global__ void sigmoid_forward_kernel(const float* input, float* output, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        output[idx] = 1.0f / (1.0f + expf(-input[idx]));
    }
}

__global__ void tanh_forward_kernel(const float* input, float* output, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        output[idx] = tanhf(input[idx]);
    }
}

__global__ void add_bias_kernel(float* output, const float* bias, 
                              int batch_size, int features) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total_size = batch_size * features;
    
    if (idx < total_size) {
        int feature_idx = idx % features;
        output[idx] += bias[feature_idx];
    }
}

__global__ void mse_loss_kernel(const float* predictions, const float* targets,
                              float* loss, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        float diff = predictions[idx] - targets[idx];
        atomicAdd(loss, diff * diff / size);
    }
}

// Convolution kernel (simplified 2D convolution)
__global__ void conv2d_forward_kernel(const float* input, const float* weight, 
                                     const float* bias, float* output,
                                     int batch_size, int in_channels, int out_channels,
                                     int in_height, int in_width,
                                     int out_height, int out_width,
                                     int kernel_size, int stride, int padding) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total_outputs = batch_size * out_channels * out_height * out_width;
    
    if (idx < total_outputs) {
        int w = idx % out_width;
        int h = (idx / out_width) % out_height;
        int oc = (idx / (out_width * out_height)) % out_channels;
        int b = idx / (out_channels * out_height * out_width);
        
        float sum = 0.0f;
        
        for (int ic = 0; ic < in_channels; ++ic) {
            for (int kh = 0; kh < kernel_size; ++kh) {
                for (int kw = 0; kw < kernel_size; ++kw) {
                    int ih = h * stride + kh - padding;
                    int iw = w * stride + kw - padding;
                    
                    if (ih >= 0 && ih < in_height && iw >= 0 && iw < in_width) {
                        int input_idx = b * (in_channels * in_height * in_width) +
                                       ic * (in_height * in_width) + 
                                       ih * in_width + iw;
                        int weight_idx = oc * (in_channels * kernel_size * kernel_size) +
                                        ic * (kernel_size * kernel_size) +
                                        kh * kernel_size + kw;
                        
                        sum += input[input_idx] * weight[weight_idx];
                    }
                }
            }
        }
        
        output[idx] = sum + bias[oc];
    }
}

#endif

// GPU-accelerated operations wrapper
class GpuOps {
public:
    static void relu_forward(const GpuTensor& input, GpuTensor& output) {
#ifdef USE_CUDA
        int size = input.size();
        int threads = 256;
        int blocks = (size + threads - 1) / threads;
        
        relu_forward_kernel<<<blocks, threads>>>(input.data(), output.data(), size);
        cudaDeviceSynchronize();
#endif
    }
    
    static void relu_backward(const GpuTensor& input, const GpuTensor& grad_output,
                            GpuTensor& grad_input) {
#ifdef USE_CUDA
        int size = input.size();
        int threads = 256;
        int blocks = (size + threads - 1) / threads;
        
        relu_backward_kernel<<<blocks, threads>>>(input.data(), grad_output.data(),
                                                 grad_input.data(), size);
        cudaDeviceSynchronize();
#endif
    }
    
    static void gemm(const GpuTensor& A, const GpuTensor& B, GpuTensor& C,
                    bool transpose_A = false, bool transpose_B = false,
                    float alpha = 1.0f, float beta = 0.0f) {
#ifdef USE_CUDA
        auto& device = CudaDevice::getInstance();
        cublasHandle_t handle = device.getCublasHandle();
        
        // Assuming A is [M, K], B is [K, N], C is [M, N]
        const auto& shape_A = A.shape();
        const auto& shape_B = B.shape();
        const auto& shape_C = C.shape();
        
        int M = transpose_A ? shape_A[1] : shape_A[0];
        int N = transpose_B ? shape_B[0] : shape_B[1];
        int K = transpose_A ? shape_A[0] : shape_A[1];
        
        cublasOperation_t op_A = transpose_A ? CUBLAS_OP_T : CUBLAS_OP_N;
        cublasOperation_t op_B = transpose_B ? CUBLAS_OP_T : CUBLAS_OP_N;
        
        cublasStatus_t stat = cublasSgemm(handle, op_B, op_A, N, M, K,
                                        &alpha, B.data(), shape_B[0],
                                        A.data(), shape_A[0],
                                        &beta, C.data(), shape_C[0]);
        
        if (stat != CUBLAS_STATUS_SUCCESS) {
            throw std::runtime_error("cuBLAS GEMM operation failed");
        }
#endif
    }
    
    static void conv2d_forward(const GpuTensor& input, const GpuTensor& weight,
                             const GpuTensor& bias, GpuTensor& output,
                             int stride = 1, int padding = 0) {
#ifdef USE_CUDA
        const auto& in_shape = input.shape();  // [batch, in_channels, height, width]
        const auto& w_shape = weight.shape();  // [out_channels, in_channels, ker_h, ker_w]
        const auto& out_shape = output.shape(); // [batch, out_channels, out_height, out_width]
        
        int batch_size = in_shape[0];
        int in_channels = in_shape[1];
        int in_height = in_shape[2];
        int in_width = in_shape[3];
        int out_channels = w_shape[0];
        int kernel_size = w_shape[2]; // Assuming square kernels
        int out_height = out_shape[2];
        int out_width = out_shape[3];
        
        int total_outputs = batch_size * out_channels * out_height * out_width;
        int threads = 256;
        int blocks = (total_outputs + threads - 1) / threads;
        
        conv2d_forward_kernel<<<blocks, threads>>>(
            input.data(), weight.data(), bias.data(), output.data(),
            batch_size, in_channels, out_channels,
            in_height, in_width, out_height, out_width,
            kernel_size, stride, padding
        );
        
        cudaDeviceSynchronize();
#endif
    }
};

// GPU-accelerated Variable class
class GpuVariable {
private:
    std::shared_ptr<GpuTensor> data_;
    std::shared_ptr<GpuTensor> grad_;
    bool requires_grad_;
    bool on_gpu_;

public:
    GpuVariable(const std::vector<int>& shape, bool requires_grad = false)
        : requires_grad_(requires_grad), on_gpu_(true) {
        data_ = std::make_shared<GpuTensor>(shape);
        if (requires_grad_) {
            grad_ = std::make_shared<GpuTensor>(shape);
        }
    }
    
    // Create from CPU tensor
    static GpuVariable fromCpu(const Variable& cpu_var) {
        GpuVariable gpu_var(cpu_var.data().shape(), cpu_var.requires_grad());
        gpu_var.data_->copyFromCpu(cpu_var.data());
        return gpu_var;
    }
    
    // Convert to CPU
    Variable toCpu() const {
        Tensor cpu_tensor(data_->shape());
        data_->copyToCpu(cpu_tensor);
        return Variable(cpu_tensor, requires_grad_);
    }
    
    GpuTensor& data() { return *data_; }
    const GpuTensor& data() const { return *data_; }
    GpuTensor& grad() { 
        if (!grad_) throw std::runtime_error("Gradient not available");
        return *grad_; 
    }
    bool requires_grad() const { return requires_grad_; }
    bool is_on_gpu() const { return on_gpu_; }
};

// GPU-accelerated layers
class GpuLinearLayer {
private:
    GpuVariable weights_;
    GpuVariable bias_;
    int in_features_, out_features_;

public:
    GpuLinearLayer(int in_features, int out_features) 
        : in_features_(in_features), out_features_(out_features),
          weights_(std::vector<int>{in_features, out_features}, true),
          bias_(std::vector<int>{out_features}, true) {
        
        // Initialize weights (copy from CPU initialized tensor)
        Tensor cpu_weights({in_features, out_features});
        cpu_weights.xavier_init();
        weights_.data().copyFromCpu(cpu_weights);
        
        // Initialize bias to zero
        Tensor cpu_bias({out_features});
        std::fill(cpu_bias.data().begin(), cpu_bias.data().end(), 0.0f);
        bias_.data().copyFromCpu(cpu_bias);
    }
    
    GpuVariable forward(const GpuVariable& input) {
        // Create output tensor
        const auto& in_shape = input.data().shape();
        GpuVariable output(std::vector<int>{in_shape[0], out_features_}, true);
        
        // Perform matrix multiplication using cuBLAS
        GpuOps::gemm(input.data(), weights_.data(), output.data());
        
        // Add bias (would need a proper bias addition kernel)
        // For now, simplified - in practice you'd use a CUDA kernel
        
        return output;
    }
    
    std::vector<GpuVariable*> parameters() {
        return {&weights_, &bias_};
    }
};

// GPU-accelerated Conv2D layer
class GpuConv2DLayer {
private:
    GpuVariable weights_;
    GpuVariable bias_;
    int in_channels_, out_channels_, kernel_size_, stride_, padding_;

public:
    GpuConv2DLayer(int in_channels, int out_channels, int kernel_size,
                   int stride = 1, int padding = 0)
        : in_channels_(in_channels), out_channels_(out_channels),
          kernel_size_(kernel_size), stride_(stride), padding_(padding),
          weights_(std::vector<int>{out_channels, in_channels, kernel_size, kernel_size}, true),
          bias_(std::vector<int>{out_channels}, true) {
        
        // Initialize weights
        Tensor cpu_weights({out_channels, in_channels, kernel_size, kernel_size});
        float fan_in = in_channels * kernel_size * kernel_size;
        float std = std::sqrt(2.0f / fan_in);
        cpu_weights.random_normal(0.0f, std);
        weights_.data().copyFromCpu(cpu_weights);
        
        // Initialize bias
        Tensor cpu_bias({out_channels});
        std::fill(cpu_bias.data().begin(), cpu_bias.data().end(), 0.0f);
        bias_.data().copyFromCpu(cpu_bias);
    }
    
    GpuVariable forward(const GpuVariable& input) {
        const auto& in_shape = input.data().shape();
        int batch = in_shape[0];
        int in_h = in_shape[2], in_w = in_shape[3];
        
        int out_h = (in_h + 2 * padding_ - kernel_size_) / stride_ + 1;
        int out_w = (in_w + 2 * padding_ - kernel_size_) / stride_ + 1;
        
        GpuVariable output(std::vector<int>{batch, out_channels_, out_h, out_w}, true);
        
        // Perform convolution using custom CUDA kernel
        GpuOps::conv2d_forward(input.data(), weights_.data(), bias_.data(), 
                              output.data(), stride_, padding_);
        
        return output;
    }
    
    std::vector<GpuVariable*> parameters() {
        return {&weights_, &bias_};
    }
};

// Training utilities for mixed CPU/GPU training
class MixedTrainer {
private:
    bool use_gpu_;
    
public:
    MixedTrainer(bool use_gpu = true) : use_gpu_(use_gpu && CudaDevice::getInstance().isAvailable()) {}
    
    template<typename ModelType>
    void train_step(ModelType& model, const Variable& input, const Variable& target,
                   float learning_rate) {
        if (use_gpu_) {
            // Move to GPU
            auto gpu_input = GpuVariable::fromCpu(input);
            auto gpu_target = GpuVariable::fromCpu(target);
            
            // Forward pass on GPU
            auto gpu_output = model.forward(gpu_input);
            
            // Compute loss (simplified)
            auto cpu_output = gpu_output.toCpu();
            auto loss = MSELoss::forward(cpu_output, target);
            
            // Backward pass (would need GPU implementation)
            loss.backward();
            
            // Update weights on GPU
            // model.update_weights_gpu(learning_rate);
        } else {
            // CPU training
            auto output = model.forward(input);
            auto loss = MSELoss::forward(output, target);
            loss.backward();
            model.update_weights(learning_rate);
        }
    }
    
    bool using_gpu() const { return use_gpu_; }
};

// Memory pool for efficient GPU memory management
class GpuMemoryPool {
private:
    std::vector<std::pair<void*, size_t>> free_blocks_;
    std::vector<std::pair<void*, size_t>> used_blocks_;
    size_t total_allocated_;
    
    static std::unique_ptr<GpuMemoryPool> instance_;
    
public:
    static GpuMemoryPool& getInstance() {
        if (!instance_) {
            instance_ = std::make_unique<GpuMemoryPool>();
        }
        return *instance_;
    }
    
    void* allocate(size_t size) {
#ifdef USE_CUDA
        // Round up to nearest 256 bytes for alignment
        size_t aligned_size = ((size + 255) / 256) * 256;
        
        // Look for suitable free block
        for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
            if (it->second >= aligned_size) {
                void* ptr = it->first;
                size_t block_size = it->second;
                
                free_blocks_.erase(it);
                used_blocks_.emplace_back(ptr, aligned_size);
                
                // Split block if larger than needed
                if (block_size > aligned_size) {
                    void* remaining = static_cast<char*>(ptr) + aligned_size;
                    free_blocks_.emplace_back(remaining, block_size - aligned_size);
                }
                
                return ptr;
            }
        }
        
        // No suitable block found, allocate new memory
        void* ptr;
        cudaError_t err = cudaMalloc(&ptr, aligned_size);
        if (err != cudaSuccess) {
            throw std::runtime_error("GPU memory allocation failed");
        }
        
        used_blocks_.emplace_back(ptr, aligned_size);
        total_allocated_ += aligned_size;
        
        return ptr;
#else
        throw std::runtime_error("CUDA not available");
#endif
    }
    
    void deallocate(void* ptr) {
        // Find and move from used to free blocks
        for (auto it = used_blocks_.begin(); it != used_blocks_.end(); ++it) {
            if (it->first == ptr) {
                free_blocks_.emplace_back(it->first, it->second);
                used_blocks_.erase(it);
                return;
            }
        }
    }
    
    void cleanup() {
#ifdef USE_CUDA
        for (const auto& block : free_blocks_) {
            cudaFree(block.first);
        }
        for (const auto& block : used_blocks_) {
            cudaFree(block.first);
        }
        free_blocks_.clear();
        used_blocks_.clear();
        total_allocated_ = 0;
#endif
    }
    
    size_t total_memory() const { return total_allocated_; }
};

// Initialize static members
std::unique_ptr<CudaDevice> CudaDevice::instance_ = nullptr;

std::unique_ptr<GpuMemoryPool> GpuMemoryPool::instance_ = nullptr;
