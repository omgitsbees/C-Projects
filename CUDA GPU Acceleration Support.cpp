#pragma once

#ifdef USE_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cudnn.h>
#endif
  
#include <memory>
#include <stdexcept>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations for CPU types
class Tensor;
class Variable;

// Macro for CUDA error checking
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            throw std::runtime_error(std::string("CUDA error at ") + __FILE__ + ":" + \
                                   std::to_string(__LINE__) + " - " + \
                                   cudaGetErrorString(err)); \
        } \
    } while(0)

#define CUBLAS_CHECK(call) \
    do { \
        cublasStatus_t stat = call; \
        if (stat != CUBLAS_STATUS_SUCCESS) { \
            throw std::runtime_error(std::string("cuBLAS error at ") + __FILE__ + ":" + \
                                   std::to_string(__LINE__)); \
        } \
    } while(0)

#define CUDNN_CHECK(call) \
    do { \
        cudnnStatus_t stat = call; \
        if (stat != CUDNN_STATUS_SUCCESS) { \
            throw std::runtime_error(std::string("cuDNN error at ") + __FILE__ + ":" + \
                                   std::to_string(__LINE__) + " - " + \
                                   cudnnGetErrorString(stat)); \
        } \
    } while(0)

// Configuration constants
namespace CudaConfig {
    constexpr int DEFAULT_BLOCK_SIZE = 256;
    constexpr int MAX_BLOCK_SIZE = 1024;
    constexpr size_t MEMORY_ALIGNMENT = 256;
}

// CUDA utilities and device management
class CudaDevice {
private:
    static std::unique_ptr<CudaDevice> instance_;
    static std::once_flag init_flag_;
    
    int device_id_;
    cudaDeviceProp device_prop_;
    
#ifdef USE_CUDA
    cublasHandle_t cublas_handle_;
    cudnnHandle_t cudnn_handle_;
    cudaStream_t default_stream_;
#endif

    CudaDevice(int device_id = 0) : device_id_(device_id) {
#ifdef USE_CUDA
        cublas_handle_ = nullptr;
        cudnn_handle_ = nullptr;
        default_stream_ = nullptr;
        
        CUDA_CHECK(cudaSetDevice(device_id_));
        CUDA_CHECK(cudaGetDeviceProperties(&device_prop_, device_id_));
        
        // Initialize cuBLAS
        CUBLAS_CHECK(cublasCreate(&cublas_handle_));
        
        // Initialize cuDNN
        CUDNN_CHECK(cudnnCreate(&cudnn_handle_));
        
        // Create default stream
        CUDA_CHECK(cudaStreamCreate(&default_stream_));
        
        // Set cuBLAS stream
        CUBLAS_CHECK(cublasSetStream(cublas_handle_, default_stream_));
#endif
    }

public:
    static CudaDevice& getInstance() {
        std::call_once(init_flag_, []() {
            instance_ = std::unique_ptr<CudaDevice>(new CudaDevice());
        });
        return *instance_;
    }
    
    ~CudaDevice() {
#ifdef USE_CUDA
        if (default_stream_) cudaStreamDestroy(default_stream_);
        if (cublas_handle_) cublasDestroy(cublas_handle_);
        if (cudnn_handle_) cudnnDestroy(cudnn_handle_);
#endif
    }
    
    // Delete copy and move
    CudaDevice(const CudaDevice&) = delete;
    CudaDevice& operator=(const CudaDevice&) = delete;
    CudaDevice(CudaDevice&&) = delete;
    CudaDevice& operator=(CudaDevice&&) = delete;
    
#ifdef USE_CUDA
    cublasHandle_t getCublasHandle() { return cublas_handle_; }
    cudnnHandle_t getCudnnHandle() { return cudnn_handle_; }
    cudaStream_t getDefaultStream() { return default_stream_; }
    const cudaDeviceProp& getDeviceProperties() const { return device_prop_; }
#endif
    
    bool isAvailable() const {
#ifdef USE_CUDA
        int device_count;
        cudaError_t error = cudaGetDeviceCount(&device_count);
        return error == cudaSuccess && device_count > 0;
#else
        return false;
#endif
    }
    
    int getOptimalBlockSize(int size) const {
#ifdef USE_CUDA
        if (size < 128) return 128;
        if (size < 256) return 256;
        if (size < 512) return 512;
        return CudaConfig::DEFAULT_BLOCK_SIZE;
#else
        return CudaConfig::DEFAULT_BLOCK_SIZE;
#endif
    }
};

// Memory pool for efficient GPU memory management
class GpuMemoryPool {
private:
    struct Block {
        void* ptr;
        size_t size;
        bool in_use;
        
        Block(void* p, size_t s) : ptr(p), size(s), in_use(false) {}
    };
    
    std::vector<Block> blocks_;
    size_t total_allocated_;
    mutable std::mutex mutex_;
    
    static std::unique_ptr<GpuMemoryPool> instance_;
    static std::once_flag init_flag_;
    
    GpuMemoryPool() : total_allocated_(0) {}
    
    static size_t alignSize(size_t size) {
        return ((size + CudaConfig::MEMORY_ALIGNMENT - 1) / CudaConfig::MEMORY_ALIGNMENT) 
               * CudaConfig::MEMORY_ALIGNMENT;
    }

public:
    static GpuMemoryPool& getInstance() {
        std::call_once(init_flag_, []() {
            instance_ = std::unique_ptr<GpuMemoryPool>(new GpuMemoryPool());
        });
        return *instance_;
    }
    
    ~GpuMemoryPool() {
        cleanup();
    }
    
    void* allocate(size_t size) {
#ifdef USE_CUDA
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t aligned_size = alignSize(size);
        
        // Look for suitable free block
        for (auto& block : blocks_) {
            if (!block.in_use && block.size >= aligned_size) {
                block.in_use = true;
                
                // Split block if significantly larger
                if (block.size > aligned_size * 2) {
                    void* remaining_ptr = static_cast<char*>(block.ptr) + aligned_size;
                    size_t remaining_size = block.size - aligned_size;
                    block.size = aligned_size;
                    blocks_.emplace_back(remaining_ptr, remaining_size);
                }
                
                return block.ptr;
            }
        }
        
        // No suitable block found, allocate new memory
        void* ptr;
        CUDA_CHECK(cudaMalloc(&ptr, aligned_size));
        
        blocks_.emplace_back(ptr, aligned_size);
        blocks_.back().in_use = true;
        total_allocated_ += aligned_size;
        
        return ptr;
#else
        throw std::runtime_error("CUDA not available");
#endif
    }
    
    void deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& block : blocks_) {
            if (block.ptr == ptr) {
                block.in_use = false;
                return;
            }
        }
    }
    
    void cleanup() {
#ifdef USE_CUDA
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& block : blocks_) {
            cudaFree(block.ptr);
        }
        blocks_.clear();
        total_allocated_ = 0;
#endif
    }
    
    size_t totalMemory() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return total_allocated_; 
    }
};

// GPU Memory management with smart pointer support
class GpuTensor {
private:
    float* device_ptr_;
    std::vector<int> shape_;
    int size_;
    bool owns_data_;
    bool use_memory_pool_;
    
public:
    GpuTensor() : device_ptr_(nullptr), size_(0), owns_data_(false), use_memory_pool_(false) {}
    
    GpuTensor(const std::vector<int>& shape, bool use_pool = true) 
        : shape_(shape), owns_data_(true), use_memory_pool_(use_pool) {
        size_ = 1;
        for (int dim : shape_) size_ *= dim;
        
#ifdef USE_CUDA
        if (use_memory_pool_) {
            device_ptr_ = static_cast<float*>(
                GpuMemoryPool::getInstance().allocate(size_ * sizeof(float))
            );
        } else {
            CUDA_CHECK(cudaMalloc(&device_ptr_, size_ * sizeof(float)));
        }
        
        // Initialize to zero
        CUDA_CHECK(cudaMemset(device_ptr_, 0, size_ * sizeof(float)));
#else
        throw std::runtime_error("CUDA not available");
#endif
    }
    
    ~GpuTensor() {
        if (owns_data_ && device_ptr_) {
#ifdef USE_CUDA
            if (use_memory_pool_) {
                GpuMemoryPool::getInstance().deallocate(device_ptr_);
            } else {
                cudaFree(device_ptr_);
            }
#endif
        }
    }
    
    // Delete copy operations
    GpuTensor(const GpuTensor&) = delete;
    GpuTensor& operator=(const GpuTensor&) = delete;
    
    // Move constructor and assignment
    GpuTensor(GpuTensor&& other) noexcept 
        : device_ptr_(other.device_ptr_), shape_(std::move(other.shape_)), 
          size_(other.size_), owns_data_(other.owns_data_),
          use_memory_pool_(other.use_memory_pool_) {
        other.device_ptr_ = nullptr;
        other.owns_data_ = false;
    }
    
    GpuTensor& operator=(GpuTensor&& other) noexcept {
        if (this != &other) {
            if (owns_data_ && device_ptr_) {
#ifdef USE_CUDA
                if (use_memory_pool_) {
                    GpuMemoryPool::getInstance().deallocate(device_ptr_);
                } else {
                    cudaFree(device_ptr_);
                }
#endif
            }
            device_ptr_ = other.device_ptr_;
            shape_ = std::move(other.shape_);
            size_ = other.size_;
            owns_data_ = other.owns_data_;
            use_memory_pool_ = other.use_memory_pool_;
            
            other.device_ptr_ = nullptr;
            other.owns_data_ = false;
        }
        return *this;
    }
    
    // Copy data from CPU tensor
    void copyFromCpu(const Tensor& cpu_tensor) {
#ifdef USE_CUDA
        if (cpu_tensor.size() != size_) {
            throw std::runtime_error("Size mismatch in copyFromCpu: expected " + 
                                   std::to_string(size_) + ", got " + 
                                   std::to_string(cpu_tensor.size()));
        }
        
        CUDA_CHECK(cudaMemcpy(device_ptr_, cpu_tensor.data().data(), 
                             size_ * sizeof(float), cudaMemcpyHostToDevice));
#endif
    }
    
    // Copy data to CPU tensor
    void copyToCpu(Tensor& cpu_tensor) const {
#ifdef USE_CUDA
        if (cpu_tensor.size() != size_) {
            throw std::runtime_error("Size mismatch in copyToCpu: expected " + 
                                   std::to_string(size_) + ", got " + 
                                   std::to_string(cpu_tensor.size()));
        }
        
        CUDA_CHECK(cudaMemcpy(cpu_tensor.data().data(), device_ptr_, 
                             size_ * sizeof(float), cudaMemcpyDeviceToHost));
#endif
    }
    
    void zero() {
#ifdef USE_CUDA
        CUDA_CHECK(cudaMemset(device_ptr_, 0, size_ * sizeof(float)));
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

__global__ void sigmoid_backward_kernel(const float* output, const float* grad_output,
                                       float* grad_input, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        float sig = output[idx];
        grad_input[idx] = grad_output[idx] * sig * (1.0f - sig);
    }
}

__global__ void tanh_forward_kernel(const float* input, float* output, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        output[idx] = tanhf(input[idx]);
    }
}

__global__ void tanh_backward_kernel(const float* output, const float* grad_output,
                                    float* grad_input, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        float t = output[idx];
        grad_input[idx] = grad_output[idx] * (1.0f - t * t);
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
        atomicAdd(loss, diff * diff);
    }
}

__global__ void scale_kernel(float* data, float scale, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        data[idx] *= scale;
    }
}

#endif

// GPU-accelerated operations wrapper
class GpuOps {
public:
    static void relu_forward(const GpuTensor& input, GpuTensor& output) {
#ifdef USE_CUDA
        int size = input.size();
        auto& device = CudaDevice::getInstance();
        int threads = device.getOptimalBlockSize(size);
        int blocks = (size + threads - 1) / threads;
        
        relu_forward_kernel<<<blocks, threads, 0, device.getDefaultStream()>>>(
            input.data(), output.data(), size);
        CUDA_CHECK(cudaStreamSynchronize(device.getDefaultStream()));
#endif
    }
    
    static void relu_backward(const GpuTensor& input, const GpuTensor& grad_output,
                             GpuTensor& grad_input) {
#ifdef USE_CUDA
        int size = input.size();
        auto& device = CudaDevice::getInstance();
        int threads = device.getOptimalBlockSize(size);
        int blocks = (size + threads - 1) / threads;
        
        relu_backward_kernel<<<blocks, threads, 0, device.getDefaultStream()>>>(
            input.data(), grad_output.data(), grad_input.data(), size);
        CUDA_CHECK(cudaStreamSynchronize(device.getDefaultStream()));
#endif
    }
    
    static void sigmoid_forward(const GpuTensor& input, GpuTensor& output) {
#ifdef USE_CUDA
        int size = input.size();
        auto& device = CudaDevice::getInstance();
        int threads = device.getOptimalBlockSize(size);
        int blocks = (size + threads - 1) / threads;
        
        sigmoid_forward_kernel<<<blocks, threads, 0, device.getDefaultStream()>>>(
            input.data(), output.data(), size);
        CUDA_CHECK(cudaStreamSynchronize(device.getDefaultStream()));
#endif
    }
    
    static void add_bias(GpuTensor& output, const GpuTensor& bias, 
                        int batch_size, int features) {
#ifdef USE_CUDA
        int size = batch_size * features;
        auto& device = CudaDevice::getInstance();
        int threads = device.getOptimalBlockSize(size);
        int blocks = (size + threads - 1) / threads;
        
        add_bias_kernel<<<blocks, threads, 0, device.getDefaultStream()>>>(
            output.data(), bias.data(), batch_size, features);
        CUDA_CHECK(cudaStreamSynchronize(device.getDefaultStream()));
#endif
    }
    
    // Matrix multiplication: C = alpha * A * B + beta * C
    // A: [M, K], B: [K, N], C: [M, N]
    static void gemm(const GpuTensor& A, const GpuTensor& B, GpuTensor& C,
                     bool transpose_A = false, bool transpose_B = false,
                     float alpha = 1.0f, float beta = 0.0f) {
#ifdef USE_CUDA
        auto& device = CudaDevice::getInstance();
        cublasHandle_t handle = device.getCublasHandle();
        
        const auto& shape_A = A.shape();
        const auto& shape_B = B.shape();
        
        // Matrix dimensions (considering potential transpose)
        int M = transpose_A ? shape_A[1] : shape_A[0];
        int K_A = transpose_A ? shape_A[0] : shape_A[1];
        int K_B = transpose_B ? shape_B[1] : shape_B[0];
        int N = transpose_B ? shape_B[0] : shape_B[1];
        
        if (K_A != K_B) {
            throw std::runtime_error("Matrix dimension mismatch in GEMM: K_A=" + 
                                   std::to_string(K_A) + ", K_B=" + std::to_string(K_B));
        }
        int K = K_A;
        
        cublasOperation_t op_A = transpose_A ? CUBLAS_OP_T : CUBLAS_OP_N;
        cublasOperation_t op_B = transpose_B ? CUBLAS_OP_T : CUBLAS_OP_N;
        
        // cuBLAS uses column-major, so we compute B^T * A^T = (A * B)^T
        int lda = transpose_A ? M : K;
        int ldb = transpose_B ? K : N;
        int ldc = M;
        
        CUBLAS_CHECK(cublasSgemm(handle, op_A, op_B, M, N, K,
                                &alpha, A.data(), lda,
                                B.data(), ldb,
                                &beta, C.data(), ldc));
#endif
    }
};

// GPU-accelerated Variable class
class GpuVariable {
private:
    std::shared_ptr<GpuTensor> data_;
    std::shared_ptr<GpuTensor> grad_;
    bool requires_grad_;

public:
    GpuVariable() : requires_grad_(false) {}
    
    GpuVariable(const std::vector<int>& shape, bool requires_grad = false)
        : requires_grad_(requires_grad) {
        data_ = std::make_shared<GpuTensor>(shape);
        if (requires_grad_) {
            grad_ = std::make_shared<GpuTensor>(shape);
        }
    }
    
    // Create from CPU tensor
    static GpuVariable fromCpu(const Variable& cpu_var);
    
    // Convert to CPU
    Variable toCpu() const;
    
    void zero_grad() {
        if (grad_) {
            grad_->zero();
        }
    }
    
    GpuTensor& data() { return *data_; }
    const GpuTensor& data() const { return *data_; }
    GpuTensor& grad() { 
        if (!grad_) throw std::runtime_error("Gradient not available");
        return *grad_; 
    }
    const GpuTensor& grad() const {
        if (!grad_) throw std::runtime_error("Gradient not available");
        return *grad_;
    }
    bool requires_grad() const { return requires_grad_; }
    bool has_data() const { return data_ != nullptr; }
};

// GPU-accelerated layers
class GpuLinearLayer {
private:
    GpuVariable weights_;
    GpuVariable bias_;
    int in_features_, out_features_;

public:
    GpuLinearLayer(int in_features, int out_features);
    
    GpuVariable forward(const GpuVariable& input) {
        if (!input.has_data()) {
            throw std::runtime_error("Input has no data");
        }
        
        const auto& in_shape = input.data().shape();
        int batch_size = in_shape[0];
        
        // Create output tensor [batch_size, out_features]
        GpuVariable output(std::vector<int>{batch_size, out_features_}, true);
        
        // Perform matrix multiplication: output = input * weights
        GpuOps::gemm(input.data(), weights_.data(), output.data());
        
        // Add bias
        GpuOps::add_bias(output.data(), bias_.data(), batch_size, out_features_);
        
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
    MixedTrainer(bool use_gpu = true) 
        : use_gpu_(use_gpu && CudaDevice::getInstance().isAvailable()) {
        if (use_gpu && !use_gpu_) {
            throw std::runtime_error("GPU requested but not available");
        }
    }
    
    bool using_gpu() const { return use_gpu_; }
    
    void synchronize() {
#ifdef USE_CUDA
        if (use_gpu_) {
            CUDA_CHECK(cudaDeviceSynchronize());
        }
#endif
    }
};

// Initialize static members
std::unique_ptr<CudaDevice> CudaDevice::instance_ = nullptr;
std::once_flag CudaDevice::init_flag_;

std::unique_ptr<GpuMemoryPool> GpuMemoryPool::instance_ = nullptr;
std::once_flag GpuMemoryPool::init_flag_;
