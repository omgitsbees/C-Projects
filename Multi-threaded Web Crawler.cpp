#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <regex>
#include <fstream>
#include <curl/curl.h>

class WebCrawler {
private:
    // Thread synchronization
    std::queue<std::string> urlQueue;
    std::unordered_set<std::string> visitedUrls;
    std::unordered_set<std::string> queuedUrls;
    
    mutable std::mutex queueMutex;
    mutable std::mutex visitedMutex;
    mutable std::mutex outputMutex;
    std::condition_variable cv;
    
    // Crawler configuration
    std::atomic<bool> shouldStop{false};
    std::atomic<int> activeThreads{0};
    int maxDepth;
    int maxPages;
    int threadCount;
    std::atomic<int> pagesProcessed{0};
    
    // Output file
    std::string outputFile;
    
    // Callback for curl to write data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
        size_t totalSize = size * nmemb;
        data->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
    
    // Download a web page
    bool downloadPage(const std::string& url, std::string& content) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "WebCrawler/1.0");
        
        // SSL options (for HTTPS)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        
        CURLcode res = curl_easy_perform(curl);
        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        
        curl_easy_cleanup(curl);
        
        return res == CURLE_OK && responseCode == 200;
    }
    
    // Extract URLs from HTML content
    std::vector<std::string> extractUrls(const std::string& html, const std::string& baseUrl) {
        std::vector<std::string> urls;
        
        // Regex to find href attributes
        std::regex urlRegex(R"(href\s*=\s*["\']([^"\']+)["\'])", std::regex_constants::icase);
        std::sregex_iterator start(html.begin(), html.end(), urlRegex);
        std::sregex_iterator end;
        
        for (std::sregex_iterator i = start; i != end; ++i) {
            std::string url = (*i)[1].str();
            
            // Convert relative URLs to absolute
            if (url.substr(0, 4) != "http") {
                if (url[0] == '/') {
                    // Absolute path
                    size_t pos = baseUrl.find('/', 8); // Skip "https://"
                    if (pos != std::string::npos) {
                        url = baseUrl.substr(0, pos) + url;
                    } else {
                        url = baseUrl + url;
                    }
                } else if (url.substr(0, 2) != "//") {
                    // Relative path
                    size_t pos = baseUrl.rfind('/');
                    if (pos != std::string::npos) {
                        url = baseUrl.substr(0, pos + 1) + url;
                    }
                }
            }
            
            // Only add HTTP/HTTPS URLs
            if (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://") {
                urls.push_back(url);
            }
        }
        
        return urls;
    }
    
    // Extract text content from HTML
    std::string extractText(const std::string& html) {
        // Simple HTML tag removal (not perfect but functional)
        std::string text = html;
        std::regex tagRegex("<[^>]*>");
        text = std::regex_replace(text, tagRegex, " ");
        
        // Clean up whitespace
        std::regex whitespaceRegex(R"(\s+)");
        text = std::regex_replace(text, whitespaceRegex, " ");
        
        return text;
    }
    
    // Get next URL from queue
    bool getNextUrl(std::string& url) {
        std::unique_lock<std::mutex> lock(queueMutex);
        
        cv.wait(lock, [this] { return !urlQueue.empty() || shouldStop; });
        
        if (shouldStop || urlQueue.empty()) {
            return false;
        }
        
        url = urlQueue.front();
        urlQueue.pop();
        return true;
    }
    
    // Add URL to queue if not already visited or queued
    void addUrl(const std::string& url) {
        std::lock_guard<std::mutex> queueLock(queueMutex);
        std::lock_guard<std::mutex> visitedLock(visitedMutex);
        
        if (visitedUrls.find(url) == visitedUrls.end() && 
            queuedUrls.find(url) == queuedUrls.end()) {
            urlQueue.push(url);
            queuedUrls.insert(url);
            cv.notify_one();
        }
    }
    
    // Mark URL as visited
    void markVisited(const std::string& url) {
        std::lock_guard<std::mutex> lock(visitedMutex);
        visitedUrls.insert(url);
    }
    
    // Save page content to file
    void savePageContent(const std::string& url, const std::string& content) {
        std::lock_guard<std::mutex> lock(outputMutex);
        
        std::ofstream file(outputFile, std::ios::app);
        if (file.is_open()) {
            file << "URL: " << url << "\n";
            file << "Content Length: " << content.length() << " characters\n";
            file << "Text Content: " << extractText(content).substr(0, 500) << "...\n";
            file << "----------------------------------------\n";
        }
    }
    
    // Worker thread function
    void workerThread() {
        activeThreads++;
        
        while (!shouldStop && pagesProcessed < maxPages) {
            std::string url;
            if (!getNextUrl(url)) {
                break;
            }
            
            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Thread " << std::this_thread::get_id() 
                         << " processing: " << url << std::endl;
            }
            
            markVisited(url);
            
            std::string content;
            if (downloadPage(url, content)) {
                // Save content
                savePageContent(url, content);
                
                // Extract and queue new URLs
                std::vector<std::string> newUrls = extractUrls(content, url);
                for (const auto& newUrl : newUrls) {
                    if (pagesProcessed < maxPages) {
                        addUrl(newUrl);
                    }
                }
                
                pagesProcessed++;
                
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << "Successfully processed: " << url 
                             << " (Pages: " << pagesProcessed << "/" << maxPages << ")" << std::endl;
                }
            } else {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "Failed to download: " << url << std::endl;
            }
            
            // Add delay to be respectful to servers
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        activeThreads--;
        cv.notify_all();
    }
    
public:
    WebCrawler(int threads = 4, int maxP = 50, int depth = 3, const std::string& output = "crawled_pages.txt")
        : threadCount(threads), maxPages(maxP), maxDepth(depth), outputFile(output) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        
        // Clear output file
        std::ofstream file(outputFile, std::ios::trunc);
        file.close();
    }
    
    ~WebCrawler() {
        curl_global_cleanup();
    }
    
    void crawl(const std::vector<std::string>& startUrls) {
        std::cout << "Starting web crawler with " << threadCount << " threads" << std::endl;
        std::cout << "Max pages: " << maxPages << ", Output file: " << outputFile << std::endl;
        
        // Add start URLs to queue
        for (const auto& url : startUrls) {
            addUrl(url);
        }
        
        // Start worker threads
        std::vector<std::thread> threads;
        for (int i = 0; i < threadCount; ++i) {
            threads.emplace_back(&WebCrawler::workerThread, this);
        }
        
        // Wait for completion or stop condition
        while (activeThreads > 0 && pagesProcessed < maxPages && !shouldStop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check if queue is empty and no threads are working
            std::unique_lock<std::mutex> lock(queueMutex);
            if (urlQueue.empty() && activeThreads == threadCount) {
                shouldStop = true;
                cv.notify_all();
                break;
            }
        }
        
        // Signal stop and wait for all threads
        shouldStop = true;
        cv.notify_all();
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Crawling completed. Total pages processed: " << pagesProcessed << std::endl;
        std::cout << "Total unique URLs visited: " << visitedUrls.size() << std::endl;
        std::cout << "Results saved to: " << outputFile << std::endl;
    }
    
    void stop() {
        shouldStop = true;
        cv.notify_all();
    }
    
    // Get crawler statistics
    void printStats() const {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "\n--- Crawler Statistics ---" << std::endl;
        std::cout << "Pages processed: " << pagesProcessed << std::endl;
        std::cout << "URLs visited: " << visitedUrls.size() << std::endl;
        std::cout << "URLs in queue: " << urlQueue.size() << std::endl;
        std::cout << "Active threads: " << activeThreads << std::endl;
    }
};

// Example usage and testing
int main() {
    try {
        // Create crawler with 4 threads, max 20 pages, depth 3
        WebCrawler crawler(4, 20, 3, "crawled_pages.txt");
        
        // Starting URLs
        std::vector<std::string> startUrls = {
            "https://example.com",
            "https://httpbin.org",
            "https://www.wikipedia.org"
        };
        
        std::cout << "Multi-threaded Web Crawler" << std::endl;
        std::cout << "===========================" << std::endl;
        
        // Start crawling
        crawler.crawl(startUrls);
        
        // Print final statistics
        crawler.printStats();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
