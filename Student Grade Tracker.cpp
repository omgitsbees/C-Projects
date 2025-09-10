#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <numeric>

class Student {
private:
    std::string name;
    std::vector<double> grades;

public:
    // Constructor
    Student(const std::string& studentName) : name(studentName) {}

    // Getters
    std::string getName() const { return name; }
    std::vector<double> getGrades() const { return grades; }

    // Add a grade
    void addGrade(double grade) {
        if (grade >= 0 && grade <= 100) {
            grades.push_back(grade);
        } else {
            std::cout << "Invalid grade! Please enter a grade between 0-100.\n";
        }
    }

    // Calculate average
    double calculateAverage() const {
        if (grades.empty()) return 0.0;
        return std::accumulate(grades.begin(), grades.end(), 0.0) / grades.size();
    }

    // Determine letter grade based on average
    char getLetterGrade() const {
        double avg = calculateAverage();
        if (avg >= 90) return 'A';
        else if (avg >= 80) return 'B';
        else if (avg >= 70) return 'C';
        else if (avg >= 60) return 'D';
        else return 'F';
    }

    // Display student info
    void displayInfo() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Student: " << name << "\n";
        std::cout << "Grades: ";
        for (size_t i = 0; i < grades.size(); ++i) {
            std::cout << grades[i];
            if (i < grades.size() - 1) std::cout << ", ";
        }
        std::cout << "\nAverage: " << calculateAverage() << "\n";
        std::cout << "Letter Grade: " << getLetterGrade() << "\n";
        std::cout << "------------------------\n";
    }
};

class GradeTracker {
private:
    std::vector<Student> students;

public:
    // Add a new student
    void addStudent(const std::string& name) {
        // Check if student already exists
        for (const auto& student : students) {
            if (student.getName() == name) {
                std::cout << "Student " << name << " already exists!\n";
                return;
            }
        }
        students.emplace_back(name);
        std::cout << "Student " << name << " added successfully!\n";
    }

    // Add grade to a specific student
    void addGradeToStudent(const std::string& name, double grade) {
        auto it = std::find_if(students.begin(), students.end(),
            [&name](const Student& s) { return s.getName() == name; });
        
        if (it != students.end()) {
            it->addGrade(grade);
            std::cout << "Grade " << grade << " added to " << name << "\n";
        } else {
            std::cout << "Student " << name << " not found!\n";
        }
    }

    // Display all students
    void displayAllStudents() const {
        if (students.empty()) {
            std::cout << "No students in the system.\n";
            return;
        }

        std::cout << "\n=== ALL STUDENTS ===\n";
        for (const auto& student : students) {
            student.displayInfo();
        }
    }

    // Display a specific student
    void displayStudent(const std::string& name) const {
        auto it = std::find_if(students.begin(), students.end(),
            [&name](const Student& s) { return s.getName() == name; });
        
        if (it != students.end()) {
            std::cout << "\n";
            it->displayInfo();
        } else {
            std::cout << "Student " << name << " not found!\n";
        }
    }

    // Calculate class average
    double calculateClassAverage() const {
        if (students.empty()) return 0.0;
        
        double totalAverage = 0.0;
        for (const auto& student : students) {
            totalAverage += student.calculateAverage();
        }
        return totalAverage / students.size();
    }

    // Display class statistics
    void displayClassStats() const {
        if (students.empty()) {
            std::cout << "No students to display statistics.\n";
            return;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n=== CLASS STATISTICS ===\n";
        std::cout << "Total Students: " << students.size() << "\n";
        std::cout << "Class Average: " << calculateClassAverage() << "\n";

        // Count letter grade distribution
        int gradeCount[5] = {0}; // A, B, C, D, F
        for (const auto& student : students) {
            char grade = student.getLetterGrade();
            switch (grade) {
                case 'A': gradeCount[0]++; break;
                case 'B': gradeCount[1]++; break;
                case 'C': gradeCount[2]++; break;
                case 'D': gradeCount[3]++; break;
                case 'F': gradeCount[4]++; break;
            }
        }

        std::cout << "\nGrade Distribution:\n";
        std::cout << "A: " << gradeCount[0] << " students\n";
        std::cout << "B: " << gradeCount[1] << " students\n";
        std::cout << "C: " << gradeCount[2] << " students\n";
        std::cout << "D: " << gradeCount[3] << " students\n";
        std::cout << "F: " << gradeCount[4] << " students\n";
        std::cout << "------------------------\n";
    }

    // Get number of students
    size_t getStudentCount() const { return students.size(); }
};

void displayMenu() {
    std::cout << "\n=== STUDENT GRADE TRACKER ===\n";
    std::cout << "1. Add Student\n";
    std::cout << "2. Add Grade to Student\n";
    std::cout << "3. Display All Students\n";
    std::cout << "4. Display Specific Student\n";
    std::cout << "5. Display Class Statistics\n";
    std::cout << "6. Exit\n";
    std::cout << "Choose an option: ";
}

int main() {
    GradeTracker tracker;
    int choice;
    std::string studentName;
    double grade;

    std::cout << "Welcome to the Student Grade Tracker!\n";

    do {
        displayMenu();
        std::cin >> choice;

        switch (choice) {
            case 1:
                std::cout << "Enter student name: ";
                std::cin.ignore();
                std::getline(std::cin, studentName);
                tracker.addStudent(studentName);
                break;

            case 2:
                if (tracker.getStudentCount() == 0) {
                    std::cout << "No students in the system. Add a student first.\n";
                    break;
                }
                std::cout << "Enter student name: ";
                std::cin.ignore();
                std::getline(std::cin, studentName);
                std::cout << "Enter grade (0-100): ";
                std::cin >> grade;
                tracker.addGradeToStudent(studentName, grade);
                break;

            case 3:
                tracker.displayAllStudents();
                break;

            case 4:
                if (tracker.getStudentCount() == 0) {
                    std::cout << "No students in the system.\n";
                    break;
                }
                std::cout << "Enter student name: ";
                std::cin.ignore();
                std::getline(std::cin, studentName);
                tracker.displayStudent(studentName);
                break;

            case 5:
                tracker.displayClassStats();
                break;

            case 6:
                std::cout << "Thank you for using the Student Grade Tracker!\n";
                break;

            default:
                std::cout << "Invalid option. Please try again.\n";
        }
    } while (choice != 6);

    return 0;
}