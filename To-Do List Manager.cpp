#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

class Task {
private:
    static int nextId;
    int id;
    std::string description;
    bool completed;
    std::string dateCreated;

public:
    Task(const std::string& desc) : id(nextId++), description(desc), completed(false) {
        // Get current date/time
        time_t now = time(0);
        char* dt = ctime(&now);
        dateCreated = std::string(dt);
        dateCreated.pop_back(); // Remove newline character
    }

    // Constructor for loading from file
    Task(int taskId, const std::string& desc, bool comp, const std::string& date) 
        : id(taskId), description(desc), completed(comp), dateCreated(date) {
        if (taskId >= nextId) {
            nextId = taskId + 1;
        }
    }

    // Getters
    int getId() const { return id; }
    std::string getDescription() const { return description; }
    bool isCompleted() const { return completed; }
    std::string getDateCreated() const { return dateCreated; }

    // Setters
    void setDescription(const std::string& desc) { description = desc; }
    void setCompleted(bool comp) { completed = comp; }

    void toggleCompleted() { completed = !completed; }

    std::string toString() const {
        return std::to_string(id) + "|" + description + "|" + 
               (completed ? "1" : "0") + "|" + dateCreated;
    }
};

// Initialize static member
int Task::nextId = 1;

class TodoManager {
private:
    std::vector<Task> tasks;
    std::string filename;

public:
    TodoManager(const std::string& file = "tasks.txt") : filename(file) {
        loadTasks();
    }

    ~TodoManager() {
        saveTasks();
    }

    void addTask() {
        std::cout << "\nEnter task description: ";
        std::cin.ignore(); // Clear input buffer
        std::string description;
        std::getline(std::cin, description);
        
        if (!description.empty()) {
            tasks.emplace_back(description);
            std::cout << "Task added successfully!\n";
        } else {
            std::cout << "Task description cannot be empty!\n";
        }
    }

    void displayTasks() const {
        if (tasks.empty()) {
            std::cout << "\nNo tasks available.\n";
            return;
        }

        std::cout << "\n" << std::setfill('=') << std::setw(80) << "" << std::setfill(' ') << "\n";
        std::cout << "                           YOUR TO-DO LIST\n";
        std::cout << std::setfill('=') << std::setw(80) << "" << std::setfill(' ') << "\n";
        std::cout << std::left << std::setw(4) << "ID" 
                  << std::setw(8) << "Status" 
                  << std::setw(40) << "Description" 
                  << "Date Created\n";
        std::cout << std::setfill('-') << std::setw(80) << "" << std::setfill(' ') << "\n";

        for (const auto& task : tasks) {
            std::cout << std::left << std::setw(4) << task.getId()
                      << std::setw(8) << (task.isCompleted() ? "[DONE]" : "[TODO]")
                      << std::setw(40) << task.getDescription()
                      << task.getDateCreated() << "\n";
        }
        std::cout << std::setfill('=') << std::setw(80) << "" << std::setfill(' ') << "\n";
    }

    void removeTask() {
        if (tasks.empty()) {
            std::cout << "\nNo tasks to remove.\n";
            return;
        }

        displayTasks();
        std::cout << "\nEnter task ID to remove: ";
        int id;
        std::cin >> id;

        auto it = std::find_if(tasks.begin(), tasks.end(),
                              [id](const Task& task) { return task.getId() == id; });

        if (it != tasks.end()) {
            std::cout << "Removing task: \"" << it->getDescription() << "\"\n";
            tasks.erase(it);
            std::cout << "Task removed successfully!\n";
        } else {
            std::cout << "Task with ID " << id << " not found!\n";
        }
    }

    void toggleTaskCompletion() {
        if (tasks.empty()) {
            std::cout << "\nNo tasks available.\n";
            return;
        }

        displayTasks();
        std::cout << "\nEnter task ID to toggle completion: ";
        int id;
        std::cin >> id;

        auto it = std::find_if(tasks.begin(), tasks.end(),
                              [id](const Task& task) { return task.getId() == id; });

        if (it != tasks.end()) {
            it->toggleCompleted();
            std::cout << "Task \"" << it->getDescription() << "\" marked as " 
                      << (it->isCompleted() ? "completed" : "incomplete") << "!\n";
        } else {
            std::cout << "Task with ID " << id << " not found!\n";
        }
    }

    void editTask() {
        if (tasks.empty()) {
            std::cout << "\nNo tasks to edit.\n";
            return;
        }

        displayTasks();
        std::cout << "\nEnter task ID to edit: ";
        int id;
        std::cin >> id;

        auto it = std::find_if(tasks.begin(), tasks.end(),
                              [id](const Task& task) { return task.getId() == id; });

        if (it != tasks.end()) {
            std::cout << "Current description: \"" << it->getDescription() << "\"\n";
            std::cout << "Enter new description: ";
            std::cin.ignore(); // Clear input buffer
            std::string newDescription;
            std::getline(std::cin, newDescription);
            
            if (!newDescription.empty()) {
                it->setDescription(newDescription);
                std::cout << "Task updated successfully!\n";
            } else {
                std::cout << "Task description cannot be empty! Edit cancelled.\n";
            }
        } else {
            std::cout << "Task with ID " << id << " not found!\n";
        }
    }

    void displayStatistics() const {
        if (tasks.empty()) {
            std::cout << "\nNo tasks available for statistics.\n";
            return;
        }

        int totalTasks = tasks.size();
        int completedTasks = std::count_if(tasks.begin(), tasks.end(),
                                         [](const Task& task) { return task.isCompleted(); });
        int pendingTasks = totalTasks - completedTasks;
        double completionRate = (totalTasks > 0) ? (completedTasks * 100.0 / totalTasks) : 0.0;

        std::cout << "\n" << std::setfill('=') << std::setw(50) << "" << std::setfill(' ') << "\n";
        std::cout << "                  TASK STATISTICS\n";
        std::cout << std::setfill('=') << std::setw(50) << "" << std::setfill(' ') << "\n";
        std::cout << "Total Tasks:     " << totalTasks << "\n";
        std::cout << "Completed:       " << completedTasks << "\n";
        std::cout << "Pending:         " << pendingTasks << "\n";
        std::cout << "Completion Rate: " << std::fixed << std::setprecision(1) 
                  << completionRate << "%\n";
        std::cout << std::setfill('=') << std::setw(50) << "" << std::setfill(' ') << "\n";
    }

    void saveTasks() const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cout << "Error: Could not open file for saving!\n";
            return;
        }

        for (const auto& task : tasks) {
            file << task.toString() << "\n";
        }

        file.close();
        std::cout << "Tasks saved to " << filename << "\n";
    }

    void loadTasks() {
        std::ifstream file(filename);
        if (!file.is_open()) {
            // File doesn't exist yet, which is fine for first run
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            // Parse the line: id|description|completed|dateCreated
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            size_t pos3 = line.find('|', pos2 + 1);

            if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos) {
                int id = std::stoi(line.substr(0, pos1));
                std::string description = line.substr(pos1 + 1, pos2 - pos1 - 1);
                bool completed = (line.substr(pos2 + 1, pos3 - pos2 - 1) == "1");
                std::string dateCreated = line.substr(pos3 + 1);

                tasks.emplace_back(id, description, completed, dateCreated);
            }
        }

        file.close();
        if (!tasks.empty()) {
            std::cout << "Loaded " << tasks.size() << " tasks from " << filename << "\n";
        }
    }
};

void displayMenu() {
    std::cout << "\n" << std::setfill('*') << std::setw(50) << "" << std::setfill(' ') << "\n";
    std::cout << "              TO-DO LIST MANAGER\n";
    std::cout << std::setfill('*') << std::setw(50) << "" << std::setfill(' ') << "\n";
    std::cout << "1. Add Task\n";
    std::cout << "2. Display Tasks\n";
    std::cout << "3. Remove Task\n";
    std::cout << "4. Toggle Task Completion\n";
    std::cout << "5. Edit Task\n";
    std::cout << "6. View Statistics\n";
    std::cout << "7. Save Tasks\n";
    std::cout << "8. Exit\n";
    std::cout << std::setfill('*') << std::setw(50) << "" << std::setfill(' ') << "\n";
    std::cout << "Enter your choice (1-8): ";
}

int main() {
    TodoManager manager;
    int choice;

    std::cout << "Welcome to the To-Do List Manager!\n";

    while (true) {
        displayMenu();
        std::cin >> choice;

        switch (choice) {
            case 1:
                manager.addTask();
                break;
            case 2:
                manager.displayTasks();
                break;
            case 3:
                manager.removeTask();
                break;
            case 4:
                manager.toggleTaskCompletion();
                break;
            case 5:
                manager.editTask();
                break;
            case 6:
                manager.displayStatistics();
                break;
            case 7:
                manager.saveTasks();
                break;
            case 8:
                std::cout << "\nThank you for using To-Do List Manager!\n";
                std::cout << "Your tasks have been automatically saved.\n";
                return 0;
            default:
                std::cout << "\nInvalid choice! Please enter a number between 1 and 8.\n";
                break;
        }

        // Pause before showing menu again
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore();
        std::cin.get();
    }

    return 0;
}