#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <functional>
#include <atomic>
#include <random>
#include <algorithm>
#include <memory>
#include <future>
#include <sstream>
   
// Forward declarations
class Node;
class JobScheduler;
class DataManager;
class LoadBalancer;

// Data structures for MapReduce operations
struct KeyValuePair {
    std::string key;
    std::string value;
    
    KeyValuePair(const std::string& k, const std::string& v) : key(k), value(v) {}
};

struct TaskResult {
    std::string taskId;
    std::vector<KeyValuePair> results;
    bool success;
    std::string errorMessage;
    
    TaskResult(const std::string& id) : taskId(id), success(true) {}
};

// Abstract base classes for user-defined functions
class MapFunction {
public:
    virtual ~MapFunction() = default;
    virtual std::vector<KeyValuePair> map(const std::string& input) = 0;
};

class ReduceFunction {
public:
    virtual ~ReduceFunction() = default;
    virtual std::string reduce(const std::string& key, const std::vector<std::string>& values) = 0;
};

// Node status enumeration
enum class NodeStatus {
    ACTIVE,
    INACTIVE,
    FAILED
};

// Job status enumeration
enum class JobStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED
};

// Represents a compute node in the distributed system
class Node {
private:
    std::string nodeId;
    std::string ipAddress;
    int port;
    NodeStatus status;
    std::atomic<int> currentLoad;
    std::atomic<int> maxCapacity;
    std::mutex nodeMutex;
    std::thread workerThread;
    std::queue<std::function<void()>> taskQueue;
    std::mutex queueMutex;
    std::condition_variable taskCondition;
    std::atomic<bool> running;

public:
    Node(const std::string& id, const std::string& ip, int p, int capacity = 10)
        : nodeId(id), ipAddress(ip), port(p), status(NodeStatus::ACTIVE),
          currentLoad(0), maxCapacity(capacity), running(true) {
        workerThread = std::thread(&Node::processTasksLoop, this);
    }

    ~Node() {
        shutdown();
    }

    void shutdown() {
        running = false;
        taskCondition.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    std::string getId() const { return nodeId; }
    std::string getAddress() const { return ipAddress + ":" + std::to_string(port); }
    NodeStatus getStatus() const { return status; }
    int getCurrentLoad() const { return currentLoad.load(); }
    int getCapacity() const { return maxCapacity.load(); }
    
    void setStatus(NodeStatus newStatus) {
        std::lock_guard<std::mutex> lock(nodeMutex);
        status = newStatus;
    }

    bool canAcceptTask() const {
        return status == NodeStatus::ACTIVE && currentLoad < maxCapacity;
    }

    void addTask(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            taskQueue.push(task);
            currentLoad++;
        }
        taskCondition.notify_one();
    }

    TaskResult executeMapTask(const std::string& taskId, const std::string& input, 
                             std::shared_ptr<MapFunction> mapFunc) {
        TaskResult result(taskId);
        try {
            result.results = mapFunc->map(input);
            std::cout << "Node " << nodeId << " completed map task " << taskId << std::endl;
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
        }
        return result;
    }

    TaskResult executeReduceTask(const std::string& taskId, const std::string& key,
                                const std::vector<std::string>& values,
                                std::shared_ptr<ReduceFunction> reduceFunc) {
        TaskResult result(taskId);
        try {
            std::string reducedValue = reduceFunc->reduce(key, values);
            result.results.emplace_back(key, reducedValue);
            std::cout << "Node " << nodeId << " completed reduce task " << taskId << std::endl;
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
        }
        return result;
    }

private:
    void processTasksLoop() {
        while (running) {
            std::unique_lock<std::mutex> lock(queueMutex);
            taskCondition.wait(lock, [this] { return !taskQueue.empty() || !running; });

            if (!running) break;

            if (!taskQueue.empty()) {
                auto task = taskQueue.front();
                taskQueue.pop();
                lock.unlock();

                // Execute task
                task();
                currentLoad--;
            }
        }
    }
};

// Manages data distribution and replication across nodes
class DataManager {
private:
    std::map<std::string, std::vector<std::string>> dataBlocks; // dataId -> node addresses
    std::map<std::string, std::string> actualData; // dataId -> data content
    std::mutex dataMutex;
    int replicationFactor;

public:
    DataManager(int replicas = 3) : replicationFactor(replicas) {}

    void storeData(const std::string& dataId, const std::string& data,
                   const std::vector<std::shared_ptr<Node>>& availableNodes) {
        std::lock_guard<std::mutex> lock(dataMutex);
        
        actualData[dataId] = data;
        
        // Select nodes for replication
        std::vector<std::string> selectedNodes;
        int nodesToSelect = std::min(replicationFactor, (int)availableNodes.size());
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::vector<int> indices(availableNodes.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), gen);
        
        for (int i = 0; i < nodesToSelect; i++) {
            if (availableNodes[indices[i]]->getStatus() == NodeStatus::ACTIVE) {
                selectedNodes.push_back(availableNodes[indices[i]]->getAddress());
            }
        }
        
        dataBlocks[dataId] = selectedNodes;
        std::cout << "Data " << dataId << " replicated to " << selectedNodes.size() << " nodes" << std::endl;
    }

    std::string retrieveData(const std::string& dataId) {
        std::lock_guard<std::mutex> lock(dataMutex);
        auto it = actualData.find(dataId);
        if (it != actualData.end()) {
            return it->second;
        }
        return "";
    }

    std::vector<std::string> getDataLocations(const std::string& dataId) {
        std::lock_guard<std::mutex> lock(dataMutex);
        auto it = dataBlocks.find(dataId);
        if (it != dataBlocks.end()) {
            return it->second;
        }
        return {};
    }

    void handleNodeFailure(const std::string& failedNodeAddress,
                          const std::vector<std::shared_ptr<Node>>& availableNodes) {
        std::lock_guard<std::mutex> lock(dataMutex);
        
        for (auto& [dataId, locations] : dataBlocks) {
            auto it = std::find(locations.begin(), locations.end(), failedNodeAddress);
            if (it != locations.end()) {
                locations.erase(it);
                
                // Re-replicate if below threshold
                if (locations.size() < replicationFactor) {
                    for (const auto& node : availableNodes) {
                        if (node->getStatus() == NodeStatus::ACTIVE &&
                            std::find(locations.begin(), locations.end(), node->getAddress()) == locations.end()) {
                            locations.push_back(node->getAddress());
                            std::cout << "Re-replicated data " << dataId << " to node " << node->getAddress() << std::endl;
                            break;
                        }
                    }
                }
            }
        }
    }
};

// Handles load balancing across nodes
class LoadBalancer {
private:
    std::vector<std::shared_ptr<Node>> nodes;
    std::mutex balancerMutex;

public:
    void addNode(std::shared_ptr<Node> node) {
        std::lock_guard<std::mutex> lock(balancerMutex);
        nodes.push_back(node);
    }

    void removeNode(const std::string& nodeId) {
        std::lock_guard<std::mutex> lock(balancerMutex);
        nodes.erase(
            std::remove_if(nodes.begin(), nodes.end(),
                [nodeId](const std::shared_ptr<Node>& node) {
                    return node->getId() == nodeId;
                }),
            nodes.end()
        );
    }

    std::shared_ptr<Node> selectBestNode() {
        std::lock_guard<std::mutex> lock(balancerMutex);
        
        std::shared_ptr<Node> bestNode = nullptr;
        double bestScore = -1.0;
        
        for (const auto& node : nodes) {
            if (node->canAcceptTask()) {
                // Score based on inverse load ratio
                double loadRatio = (double)node->getCurrentLoad() / node->getCapacity();
                double score = 1.0 - loadRatio;
                
                if (score > bestScore) {
                    bestScore = score;
                    bestNode = node;
                }
            }
        }
        
        return bestNode;
    }

    std::vector<std::shared_ptr<Node>> getActiveNodes() {
        std::lock_guard<std::mutex> lock(balancerMutex);
        std::vector<std::shared_ptr<Node>> activeNodes;
        
        for (const auto& node : nodes) {
            if (node->getStatus() == NodeStatus::ACTIVE) {
                activeNodes.push_back(node);
            }
        }
        
        return activeNodes;
    }

    void checkNodeHealth() {
        std::lock_guard<std::mutex> lock(balancerMutex);
        
        for (auto& node : nodes) {
            // Simple health check - in real implementation, this would be network-based
            if (node->getStatus() == NodeStatus::ACTIVE) {
                // Simulate occasional node failures for demonstration
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(1, 1000);
                
                if (dis(gen) <= 5) { // 0.5% chance of failure
                    node->setStatus(NodeStatus::FAILED);
                    std::cout << "Node " << node->getId() << " has failed!" << std::endl;
                }
            }
        }
    }
};

// Job representation
struct Job {
    std::string jobId;
    std::string inputData;
    std::shared_ptr<MapFunction> mapFunc;
    std::shared_ptr<ReduceFunction> reduceFunc;
    JobStatus status;
    std::vector<TaskResult> mapResults;
    std::vector<TaskResult> reduceResults;
    std::chrono::steady_clock::time_point startTime;
    
    Job(const std::string& id, const std::string& input,
        std::shared_ptr<MapFunction> mf, std::shared_ptr<ReduceFunction> rf)
        : jobId(id), inputData(input), mapFunc(mf), reduceFunc(rf),
          status(JobStatus::PENDING) {}
};

// Main job scheduler and coordinator
class JobScheduler {
private:
    std::queue<std::shared_ptr<Job>> jobQueue;
    std::vector<std::shared_ptr<Job>> runningJobs;
    std::vector<std::shared_ptr<Job>> completedJobs;
    std::unique_ptr<LoadBalancer> loadBalancer;
    std::unique_ptr<DataManager> dataManager;
    std::mutex schedulerMutex;
    std::thread schedulerThread;
    std::thread healthCheckThread;
    std::atomic<bool> running;

public:
    JobScheduler() : running(true) {
        loadBalancer = std::make_unique<LoadBalancer>();
        dataManager = std::make_unique<DataManager>();
        
        schedulerThread = std::thread(&JobScheduler::scheduleLoop, this);
        healthCheckThread = std::thread(&JobScheduler::healthCheckLoop, this);
    }

    ~JobScheduler() {
        shutdown();
    }

    void shutdown() {
        running = false;
        if (schedulerThread.joinable()) {
            schedulerThread.join();
        }
        if (healthCheckThread.joinable()) {
            healthCheckThread.join();
        }
    }

    void addNode(std::shared_ptr<Node> node) {
        loadBalancer->addNode(node);
        std::cout << "Added node " << node->getId() << " to the cluster" << std::endl;
    }

    void submitJob(std::shared_ptr<Job> job) {
        std::lock_guard<std::mutex> lock(schedulerMutex);
        jobQueue.push(job);
        std::cout << "Job " << job->jobId << " submitted to scheduler" << std::endl;
    }

    std::vector<std::shared_ptr<Job>> getCompletedJobs() {
        std::lock_guard<std::mutex> lock(schedulerMutex);
        return completedJobs;
    }

private:
    void scheduleLoop() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            {
                std::lock_guard<std::mutex> lock(schedulerMutex);
                
                // Start pending jobs
                while (!jobQueue.empty()) {
                    auto job = jobQueue.front();
                    jobQueue.pop();
                    
                    job->status = JobStatus::RUNNING;
                    job->startTime = std::chrono::steady_clock::now();
                    runningJobs.push_back(job);
                    
                    // Execute job asynchronously
                    std::thread(&JobScheduler::executeJob, this, job).detach();
                }
                
                // Check completed jobs
                auto it = runningJobs.begin();
                while (it != runningJobs.end()) {
                    if ((*it)->status == JobStatus::COMPLETED || (*it)->status == JobStatus::FAILED) {
                        completedJobs.push_back(*it);
                        it = runningJobs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }

    void executeJob(std::shared_ptr<Job> job) {
        try {
            std::cout << "Starting execution of job " << job->jobId << std::endl;
            
            // Store input data
            dataManager->storeData(job->jobId + "_input", job->inputData, loadBalancer->getActiveNodes());
            
            // Phase 1: Map phase
            executeMapPhase(job);
            
            // Phase 2: Shuffle and sort (simplified)
            std::map<std::string, std::vector<std::string>> shuffledData;
            for (const auto& result : job->mapResults) {
                for (const auto& kv : result.results) {
                    shuffledData[kv.key].push_back(kv.value);
                }
            }
            
            // Phase 3: Reduce phase
            executeReducePhase(job, shuffledData);
            
            job->status = JobStatus::COMPLETED;
            std::cout << "Job " << job->jobId << " completed successfully" << std::endl;
            
        } catch (const std::exception& e) {
            job->status = JobStatus::FAILED;
            std::cout << "Job " << job->jobId << " failed: " << e.what() << std::endl;
        }
    }

    void executeMapPhase(std::shared_ptr<Job> job) {
        // Split input data into chunks (simplified - split by lines)
        std::vector<std::string> chunks;
        std::istringstream iss(job->inputData);
        std::string line;
        while (std::getline(iss, line)) {
            chunks.push_back(line);
        }
        
        // Execute map tasks
        std::vector<std::future<TaskResult>> futures;
        
        for (size_t i = 0; i < chunks.size(); i++) {
            auto node = loadBalancer->selectBestNode();
            if (!node) {
                throw std::runtime_error("No available nodes for map task");
            }
            
            std::string taskId = job->jobId + "_map_" + std::to_string(i);
            
            auto future = std::async(std::launch::async, [node, taskId, chunks, i, job]() {
                return node->executeMapTask(taskId, chunks[i], job->mapFunc);
            });
            
            futures.push_back(std::move(future));
        }
        
        // Collect results
        for (auto& future : futures) {
            job->mapResults.push_back(future.get());
        }
    }

    void executeReducePhase(std::shared_ptr<Job> job, 
                           const std::map<std::string, std::vector<std::string>>& shuffledData) {
        std::vector<std::future<TaskResult>> futures;
        
        for (const auto& [key, values] : shuffledData) {
            auto node = loadBalancer->selectBestNode();
            if (!node) {
                throw std::runtime_error("No available nodes for reduce task");
            }
            
            std::string taskId = job->jobId + "_reduce_" + key;
            
            auto future = std::async(std::launch::async, [node, taskId, key, values, job]() {
                return node->executeReduceTask(taskId, key, values, job->reduceFunc);
            });
            
            futures.push_back(std::move(future));
        }
        
        // Collect results
        for (auto& future : futures) {
            job->reduceResults.push_back(future.get());
        }
    }

    void healthCheckLoop() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            loadBalancer->checkNodeHealth();
            
            // Handle failed nodes
            for (const auto& node : loadBalancer->getActiveNodes()) {
                if (node->getStatus() == NodeStatus::FAILED) {
                    dataManager->handleNodeFailure(node->getAddress(), loadBalancer->getActiveNodes());
                }
            }
        }
    }
};

// Example MapReduce implementations
class WordCountMapper : public MapFunction {
public:
    std::vector<KeyValuePair> map(const std::string& input) override {
        std::vector<KeyValuePair> results;
        std::istringstream iss(input);
        std::string word;
        
        while (iss >> word) {
            // Remove punctuation and convert to lowercase
            word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            
            if (!word.empty()) {
                results.emplace_back(word, "1");
            }
        }
        
        return results;
    }
};

class WordCountReducer : public ReduceFunction {
public:
    std::string reduce(const std::string& key, const std::vector<std::string>& values) override {
        int count = 0;
        for (const auto& value : values) {
            count += std::stoi(value);
        }
        return std::to_string(count);
    }
};

// Main demonstration function
int main() {
    std::cout << "=== Distributed Computing Framework Demo ===" << std::endl;
    
    // Create the job scheduler
    JobScheduler scheduler;
    
    // Add nodes to the cluster
    auto node1 = std::make_shared<Node>("node1", "192.168.1.1", 8001, 5);
    auto node2 = std::make_shared<Node>("node2", "192.168.1.2", 8002, 5);
    auto node3 = std::make_shared<Node>("node3", "192.168.1.3", 8003, 5);
    
    scheduler.addNode(node1);
    scheduler.addNode(node2);
    scheduler.addNode(node3);
    
    // Create a word count job
    std::string inputText = "hello world\nhello distributed computing\nworld of parallel processing\nhello world again";
    
    auto mapFunction = std::make_shared<WordCountMapper>();
    auto reduceFunction = std::make_shared<WordCountReducer>();
    
    auto job = std::make_shared<Job>("job1", inputText, mapFunction, reduceFunction);
    
    // Submit the job
    scheduler.submitJob(job);
    
    // Wait for job completion
    std::cout << "\nWaiting for job completion..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Display results
    auto completedJobs = scheduler.getCompletedJobs();
    
    for (const auto& completedJob : completedJobs) {
        std::cout << "\n=== Job " << completedJob->jobId << " Results ===" << std::endl;
        std::cout << "Status: " << (completedJob->status == JobStatus::COMPLETED ? "COMPLETED" : "FAILED") << std::endl;
        
        if (completedJob->status == JobStatus::COMPLETED) {
            std::cout << "\nWord Count Results:" << std::endl;
            for (const auto& result : completedJob->reduceResults) {
                for (const auto& kv : result.results) {
                    std::cout << kv.key << ": " << kv.value << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n=== Framework Features Demonstrated ===" << std::endl;
    std::cout << "✓ MapReduce-style distributed processing" << std::endl;
    std::cout << "✓ Job scheduling and execution" << std::endl;
    std::cout << "✓ Load balancing across nodes" << std::endl;
    std::cout << "✓ Data replication and fault tolerance" << std::endl;
    std::cout << "✓ Automatic health monitoring" << std::endl;
    
    // Clean shutdown
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    return 0;

}
