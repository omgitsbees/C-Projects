#include <QtWidgets>
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QSplitter>
#include <QChart>
#include <QChartView>
#include <QPieSeries>
#include <QBarSet>
#include <QBarSeries>
#include <QCategoryAxis>
#include <QValueAxis>
#include <QSplineSeries>
#include <QDateTimeAxis>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDate>
#include <QDateTime>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
  
using namespace QtCharts;

// Expense structure to hold expense data
struct Expense {
    int id;
    QString description;
    double amount;
    QString category;
    QDate date;
    QString notes;
    
    Expense() : id(0), amount(0.0) {}
    
    Expense(int _id, const QString& _desc, double _amount, 
            const QString& _cat, const QDate& _date, const QString& _notes = "")
        : id(_id), description(_desc), amount(_amount), category(_cat), date(_date), notes(_notes) {}
};

// Main application class
class ExpenseTracker : public QMainWindow {
    Q_OBJECT

private:
    // UI Components
    QWidget* centralWidget;
    QTabWidget* tabWidget;
    
    // Input tab components
    QWidget* inputTab;
    QLineEdit* descriptionEdit;
    QLineEdit* amountEdit;
    QComboBox* categoryCombo;
    QDateEdit* dateEdit;
    QTextEdit* notesEdit;
    QPushButton* addButton;
    QPushButton* updateButton;
    QPushButton* deleteButton;
    QTableWidget* expensesTable;
    
    // Reports tab components
    QWidget* reportsTab;
    QComboBox* reportCategoryFilter;
    QDateEdit* reportStartDate;
    QDateEdit* reportEndDate;
    QPushButton* generateReportButton;
    QTextEdit* reportDisplay;
    
    // Visualization tab components
    QWidget* chartTab;
    QChartView* pieChartView;
    QChartView* barChartView;
    QChartView* lineChartView;
    QComboBox* chartTypeCombo;
    QPushButton* updateChartButton;
    
    // Data storage
    std::vector<Expense> expenses;
    int nextId;
    QString dataFilePath;
    
    // Categories
    QStringList categories = {
        "Food & Dining", "Transportation", "Entertainment", "Bills & Utilities",
        "Shopping", "Health & Medical", "Travel", "Education", "Business",
        "Personal Care", "Gifts & Donations", "Other"
    };

public:
    ExpenseTracker(QWidget* parent = nullptr) : QMainWindow(parent), nextId(1) {
        setWindowTitle("Personal Expense Tracker");
        setMinimumSize(1000, 700);
        
        // Set up data file path
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appDataPath);
        dataFilePath = appDataPath + "/expenses.json";
        
        setupUI();
        loadData();
        updateExpensesTable();
        updateCharts();
        
        // Connect signals
        connect(addButton, &QPushButton::clicked, this, &ExpenseTracker::addExpense);
        connect(updateButton, &QPushButton::clicked, this, &ExpenseTracker::updateExpense);
        connect(deleteButton, &QPushButton::clicked, this, &ExpenseTracker::deleteExpense);
        connect(generateReportButton, &QPushButton::clicked, this, &ExpenseTracker::generateReport);
        connect(updateChartButton, &QPushButton::clicked, this, &ExpenseTracker::updateCharts);
        connect(expensesTable, &QTableWidget::itemSelectionChanged, this, &ExpenseTracker::onExpenseSelected);
    }

private slots:
    void addExpense() {
        QString description = descriptionEdit->text().trimmed();
        QString amountText = amountEdit->text().trimmed();
        QString category = categoryCombo->currentText();
        QDate date = dateEdit->date();
        QString notes = notesEdit->toPlainText().trimmed();
        
        if (description.isEmpty() || amountText.isEmpty()) {
            QMessageBox::warning(this, "Input Error", "Please fill in description and amount.");
            return;
        }
        
        bool ok;
        double amount = amountText.toDouble(&ok);
        if (!ok || amount <= 0) {
            QMessageBox::warning(this, "Input Error", "Please enter a valid positive amount.");
            return;
        }
        
        Expense expense(nextId++, description, amount, category, date, notes);
        expenses.push_back(expense);
        
        updateExpensesTable();
        updateCharts();
        saveData();
        clearInputFields();
        
        QMessageBox::information(this, "Success", "Expense added successfully!");
    }
    
    void updateExpense() {
        int row = expensesTable->currentRow();
        if (row < 0 || row >= expenses.size()) {
            QMessageBox::warning(this, "Selection Error", "Please select an expense to update.");
            return;
        }
        
        QString description = descriptionEdit->text().trimmed();
        QString amountText = amountEdit->text().trimmed();
        QString category = categoryCombo->currentText();
        QDate date = dateEdit->date();
        QString notes = notesEdit->toPlainText().trimmed();
        
        if (description.isEmpty() || amountText.isEmpty()) {
            QMessageBox::warning(this, "Input Error", "Please fill in description and amount.");
            return;
        }
        
        bool ok;
        double amount = amountText.toDouble(&ok);
        if (!ok || amount <= 0) {
            QMessageBox::warning(this, "Input Error", "Please enter a valid positive amount.");
            return;
        }
        
        expenses[row].description = description;
        expenses[row].amount = amount;
        expenses[row].category = category;
        expenses[row].date = date;
        expenses[row].notes = notes;
        
        updateExpensesTable();
        updateCharts();
        saveData();
        clearInputFields();
        
        QMessageBox::information(this, "Success", "Expense updated successfully!");
    }
    
    void deleteExpense() {
        int row = expensesTable->currentRow();
        if (row < 0 || row >= expenses.size()) {
            QMessageBox::warning(this, "Selection Error", "Please select an expense to delete.");
            return;
        }
        
        int reply = QMessageBox::question(this, "Confirm Delete", 
            "Are you sure you want to delete this expense?",
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::Yes) {
            expenses.erase(expenses.begin() + row);
            updateExpensesTable();
            updateCharts();
            saveData();
            clearInputFields();
            QMessageBox::information(this, "Success", "Expense deleted successfully!");
        }
    }
    
    void onExpenseSelected() {
        int row = expensesTable->currentRow();
        if (row >= 0 && row < expenses.size()) {
            const Expense& expense = expenses[row];
            descriptionEdit->setText(expense.description);
            amountEdit->setText(QString::number(expense.amount, 'f', 2));
            categoryCombo->setCurrentText(expense.category);
            dateEdit->setDate(expense.date);
            notesEdit->setPlainText(expense.notes);
        }
    }
    
    void generateReport() {
        QString categoryFilter = reportCategoryFilter->currentText();
        QDate startDate = reportStartDate->date();
        QDate endDate = reportEndDate->date();
        
        if (startDate > endDate) {
            QMessageBox::warning(this, "Date Error", "Start date must be before or equal to end date.");
            return;
        }
        
        // Filter expenses
        std::vector<Expense> filteredExpenses;
        for (const auto& expense : expenses) {
            bool categoryMatch = (categoryFilter == "All Categories" || expense.category == categoryFilter);
            bool dateMatch = (expense.date >= startDate && expense.date <= endDate);
            
            if (categoryMatch && dateMatch) {
                filteredExpenses.push_back(expense);
            }
        }
        
        // Generate report
        QString report = generateDetailedReport(filteredExpenses, startDate, endDate, categoryFilter);
        reportDisplay->setPlainText(report);
        
        // Enable export
        QPushButton* exportButton = reportsTab->findChild<QPushButton*>("exportButton");
        if (exportButton) {
            exportButton->setEnabled(true);
        }
    }

private:
    void setupUI() {
        centralWidget = new QWidget;
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Create tab widget
        tabWidget = new QTabWidget;
        mainLayout->addWidget(tabWidget);
        
        setupInputTab();
        setupReportsTab();
        setupChartTab();
        
        // Menu bar
        setupMenuBar();
    }
    
    void setupInputTab() {
        inputTab = new QWidget;
        tabWidget->addTab(inputTab, "Add/Edit Expenses");
        
        QHBoxLayout* mainLayout = new QHBoxLayout(inputTab);
        
        // Left side - Input form
        QGroupBox* inputGroup = new QGroupBox("Expense Details");
        QGridLayout* inputLayout = new QGridLayout(inputGroup);
        
        inputLayout->addWidget(new QLabel("Description:"), 0, 0);
        descriptionEdit = new QLineEdit;
        inputLayout->addWidget(descriptionEdit, 0, 1);
        
        inputLayout->addWidget(new QLabel("Amount ($):"), 1, 0);
        amountEdit = new QLineEdit;
        amountEdit->setPlaceholderText("0.00");
        inputLayout->addWidget(amountEdit, 1, 1);
        
        inputLayout->addWidget(new QLabel("Category:"), 2, 0);
        categoryCombo = new QComboBox;
        categoryCombo->addItems(categories);
        inputLayout->addWidget(categoryCombo, 2, 1);
        
        inputLayout->addWidget(new QLabel("Date:"), 3, 0);
        dateEdit = new QDateEdit(QDate::currentDate());
        dateEdit->setCalendarPopup(true);
        inputLayout->addWidget(dateEdit, 3, 1);
        
        inputLayout->addWidget(new QLabel("Notes:"), 4, 0);
        notesEdit = new QTextEdit;
        notesEdit->setMaximumHeight(80);
        inputLayout->addWidget(notesEdit, 4, 1);
        
        // Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        addButton = new QPushButton("Add Expense");
        addButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
        updateButton = new QPushButton("Update Selected");
        updateButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px; }");
        deleteButton = new QPushButton("Delete Selected");
        deleteButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; }");
        
        buttonLayout->addWidget(addButton);
        buttonLayout->addWidget(updateButton);
        buttonLayout->addWidget(deleteButton);
        inputLayout->addLayout(buttonLayout, 5, 0, 1, 2);
        
        mainLayout->addWidget(inputGroup);
        
        // Right side - Expenses table
        QGroupBox* tableGroup = new QGroupBox("Expenses List");
        QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);
        
        expensesTable = new QTableWidget;
        expensesTable->setColumnCount(5);
        QStringList headers = {"Description", "Amount", "Category", "Date", "Notes"};
        expensesTable->setHorizontalHeaderLabels(headers);
        expensesTable->horizontalHeader()->setStretchLastSection(true);
        expensesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        expensesTable->setAlternatingRowColors(true);
        tableLayout->addWidget(expensesTable);
        
        mainLayout->addWidget(tableGroup);
        
        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 2);
    }
    
    void setupReportsTab() {
        reportsTab = new QWidget;
        tabWidget->addTab(reportsTab, "Reports");
        
        QVBoxLayout* mainLayout = new QVBoxLayout(reportsTab);
        
        // Filter controls
        QGroupBox* filterGroup = new QGroupBox("Report Filters");
        QGridLayout* filterLayout = new QGridLayout(filterGroup);
        
        filterLayout->addWidget(new QLabel("Category:"), 0, 0);
        reportCategoryFilter = new QComboBox;
        QStringList filterCategories = {"All Categories"};
        filterCategories.append(categories);
        reportCategoryFilter->addItems(filterCategories);
        filterLayout->addWidget(reportCategoryFilter, 0, 1);
        
        filterLayout->addWidget(new QLabel("Start Date:"), 0, 2);
        reportStartDate = new QDateEdit(QDate::currentDate().addDays(-30));
        reportStartDate->setCalendarPopup(true);
        filterLayout->addWidget(reportStartDate, 0, 3);
        
        filterLayout->addWidget(new QLabel("End Date:"), 0, 4);
        reportEndDate = new QDateEdit(QDate::currentDate());
        reportEndDate->setCalendarPopup(true);
        filterLayout->addWidget(reportEndDate, 0, 5);
        
        generateReportButton = new QPushButton("Generate Report");
        generateReportButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
        filterLayout->addWidget(generateReportButton, 1, 0, 1, 2);
        
        QPushButton* exportButton = new QPushButton("Export to CSV");
        exportButton->setObjectName("exportButton");
        exportButton->setEnabled(false);
        exportButton->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 8px; }");
        connect(exportButton, &QPushButton::clicked, this, &ExpenseTracker::exportToCSV);
        filterLayout->addWidget(exportButton, 1, 2, 1, 2);
        
        mainLayout->addWidget(filterGroup);
        
        // Report display
        reportDisplay = new QTextEdit;
        reportDisplay->setFont(QFont("Courier", 10));
        reportDisplay->setReadOnly(true);
        mainLayout->addWidget(reportDisplay);
    }
    
    void setupChartTab() {
        chartTab = new QWidget;
        tabWidget->addTab(chartTab, "Data Visualization");
        
        QVBoxLayout* mainLayout = new QVBoxLayout(chartTab);
        
        // Chart controls
        QHBoxLayout* controlLayout = new QHBoxLayout;
        controlLayout->addWidget(new QLabel("Chart Type:"));
        chartTypeCombo = new QComboBox;
        chartTypeCombo->addItems({"Category Breakdown (Pie)", "Monthly Spending (Bar)", "Spending Trend (Line)"});
        controlLayout->addWidget(chartTypeCombo);
        
        updateChartButton = new QPushButton("Update Charts");
        updateChartButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
        controlLayout->addWidget(updateChartButton);
        controlLayout->addStretch();
        
        mainLayout->addLayout(controlLayout);
        
        // Charts container
        QSplitter* chartSplitter = new QSplitter(Qt::Horizontal);
        
        // Pie chart
        pieChartView = new QChartView;
        pieChartView->setRenderHint(QPainter::Antialiasing);
        chartSplitter->addWidget(pieChartView);
        
        // Bar chart
        barChartView = new QChartView;
        barChartView->setRenderHint(QPainter::Antialiasing);
        chartSplitter->addWidget(barChartView);
        
        mainLayout->addWidget(chartSplitter);
        
        // Line chart
        lineChartView = new QChartView;
        lineChartView->setRenderHint(QPainter::Antialiasing);
        mainLayout->addWidget(lineChartView);
    }
    
    void setupMenuBar() {
        QMenuBar* menuBar = this->menuBar();
        
        QMenu* fileMenu = menuBar->addMenu("File");
        fileMenu->addAction("Export All Data to CSV", this, &ExpenseTracker::exportAllToCSV);
        fileMenu->addSeparator();
        fileMenu->addAction("Exit", this, &QWidget::close);
        
        QMenu* helpMenu = menuBar->addMenu("Help");
        helpMenu->addAction("About", [this]() {
            QMessageBox::about(this, "About Expense Tracker",
                "Personal Expense Tracker v1.0\n\n"
                "A comprehensive expense management application\n"
                "with data visualization and reporting capabilities.");
        });
    }
    
    void updateExpensesTable() {
        expensesTable->setRowCount(expenses.size());
        
        for (size_t i = 0; i < expenses.size(); ++i) {
            const Expense& expense = expenses[i];
            
            expensesTable->setItem(i, 0, new QTableWidgetItem(expense.description));
            expensesTable->setItem(i, 1, new QTableWidgetItem(QString("$%1").arg(expense.amount, 0, 'f', 2)));
            expensesTable->setItem(i, 2, new QTableWidgetItem(expense.category));
            expensesTable->setItem(i, 3, new QTableWidgetItem(expense.date.toString("yyyy-MM-dd")));
            expensesTable->setItem(i, 4, new QTableWidgetItem(expense.notes));
        }
        
        expensesTable->resizeColumnsToContents();
    }
    
    void updateCharts() {
        updatePieChart();
        updateBarChart();
        updateLineChart();
    }
    
    void updatePieChart() {
        // Calculate category totals
        std::map<QString, double> categoryTotals;
        for (const auto& expense : expenses) {
            categoryTotals[expense.category] += expense.amount;
        }
        
        QPieSeries* series = new QPieSeries;
        for (const auto& pair : categoryTotals) {
            QPieSlice* slice = series->append(pair.first, pair.second);
            slice->setLabelVisible(true);
            slice->setLabel(QString("%1: $%2").arg(pair.first).arg(pair.second, 0, 'f', 2));
        }
        
        QChart* chart = new QChart;
        chart->addSeries(series);
        chart->setTitle("Expenses by Category");
        chart->legend()->hide();
        
        pieChartView->setChart(chart);
    }
    
    void updateBarChart() {
        // Calculate monthly totals
        std::map<QString, double> monthlyTotals;
        for (const auto& expense : expenses) {
            QString monthKey = expense.date.toString("yyyy-MM");
            monthlyTotals[monthKey] += expense.amount;
        }
        
        QBarSet* set = new QBarSet("Monthly Spending");
        QStringList months;
        
        for (const auto& pair : monthlyTotals) {
            months << pair.first;
            *set << pair.second;
        }
        
        QBarSeries* series = new QBarSeries;
        series->append(set);
        
        QChart* chart = new QChart;
        chart->addSeries(series);
        chart->setTitle("Monthly Spending Trends");
        
        QCategoryAxis* axisX = new QCategoryAxis;
        for (int i = 0; i < months.size(); ++i) {
            axisX->append(months[i], i);
        }
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        
        QValueAxis* axisY = new QValueAxis;
        axisY->setTitleText("Amount ($)");
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        
        barChartView->setChart(chart);
    }
    
    void updateLineChart() {
        // Calculate cumulative spending over time
        std::vector<Expense> sortedExpenses = expenses;
        std::sort(sortedExpenses.begin(), sortedExpenses.end(), 
                  [](const Expense& a, const Expense& b) {
                      return a.date < b.date;
                  });
        
        QSplineSeries* series = new QSplineSeries;
        double cumulativeAmount = 0;
        
        for (const auto& expense : sortedExpenses) {
            cumulativeAmount += expense.amount;
            series->append(QDateTime(expense.date).toMSecsSinceEpoch(), cumulativeAmount);
        }
        
        QChart* chart = new QChart;
        chart->addSeries(series);
        chart->setTitle("Cumulative Spending Over Time");
        
        QDateTimeAxis* axisX = new QDateTimeAxis;
        axisX->setFormat("MMM yyyy");
        axisX->setTitleText("Date");
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        
        QValueAxis* axisY = new QValueAxis;
        axisY->setTitleText("Cumulative Amount ($)");
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        
        lineChartView->setChart(chart);
    }
    
    QString generateDetailedReport(const std::vector<Expense>& filteredExpenses, 
                                   const QDate& startDate, const QDate& endDate, 
                                   const QString& categoryFilter) {
        QString report;
        QTextStream stream(&report);
        
        stream << "EXPENSE REPORT\n";
        stream << "==============\n\n";
        stream << "Period: " << startDate.toString("yyyy-MM-dd") << " to " << endDate.toString("yyyy-MM-dd") << "\n";
        stream << "Category Filter: " << categoryFilter << "\n\n";
        
        if (filteredExpenses.empty()) {
            stream << "No expenses found for the selected criteria.\n";
            return report;
        }
        
        // Summary statistics
        double totalAmount = std::accumulate(filteredExpenses.begin(), filteredExpenses.end(), 0.0,
                                           [](double sum, const Expense& exp) { return sum + exp.amount; });
        double avgAmount = totalAmount / filteredExpenses.size();
        
        auto minExpense = std::min_element(filteredExpenses.begin(), filteredExpenses.end(),
                                         [](const Expense& a, const Expense& b) { return a.amount < b.amount; });
        auto maxExpense = std::max_element(filteredExpenses.begin(), filteredExpenses.end(),
                                         [](const Expense& a, const Expense& b) { return a.amount < b.amount; });
        
        stream << "SUMMARY\n";
        stream << "-------\n";
        stream << "Total Expenses: " << filteredExpenses.size() << "\n";
        stream << "Total Amount: $" << QString::number(totalAmount, 'f', 2) << "\n";
        stream << "Average Amount: $" << QString::number(avgAmount, 'f', 2) << "\n";
        stream << "Minimum Expense: $" << QString::number(minExpense->amount, 'f', 2) << " (" << minExpense->description << ")\n";
        stream << "Maximum Expense: $" << QString::number(maxExpense->amount, 'f', 2) << " (" << maxExpense->description << ")\n\n";
        
        // Category breakdown
        std::map<QString, double> categoryTotals;
        std::map<QString, int> categoryCounts;
        for (const auto& expense : filteredExpenses) {
            categoryTotals[expense.category] += expense.amount;
            categoryCounts[expense.category]++;
        }
        
        stream << "CATEGORY BREAKDOWN\n";
        stream << "------------------\n";
        for (const auto& pair : categoryTotals) {
            double percentage = (pair.second / totalAmount) * 100;
            stream << QString("%1: $%2 (%3 expenses, %4%)\n")
                      .arg(pair.first)
                      .arg(pair.second, 0, 'f', 2)
                      .arg(categoryCounts[pair.first])
                      .arg(percentage, 0, 'f', 1);
        }
        
        stream << "\nDETAILED EXPENSES\n";
        stream << "----------------\n";
        for (const auto& expense : filteredExpenses) {
            stream << expense.date.toString("yyyy-MM-dd") << " | "
                   << QString("$%1").arg(expense.amount, 8, 'f', 2) << " | "
                   << QString("%1").arg(expense.category, -15) << " | "
                   << expense.description << "\n";
            if (!expense.notes.isEmpty()) {
                stream << "    Notes: " << expense.notes << "\n";
            }
        }
        
        return report;
    }
    
    void clearInputFields() {
        descriptionEdit->clear();
        amountEdit->clear();
        categoryCombo->setCurrentIndex(0);
        dateEdit->setDate(QDate::currentDate());
        notesEdit->clear();
    }
    
    void saveData() {
        QJsonDocument doc;
        QJsonObject root;
        QJsonArray expensesArray;
        
        for (const auto& expense : expenses) {
            QJsonObject expenseObj;
            expenseObj["id"] = expense.id;
            expenseObj["description"] = expense.description;
            expenseObj["amount"] = expense.amount;
            expenseObj["category"] = expense.category;
            expenseObj["date"] = expense.date.toString(Qt::ISODate);
            expenseObj["notes"] = expense.notes;
            expensesArray.append(expenseObj);
        }
        
        root["expenses"] = expensesArray;
        root["nextId"] = nextId;
        doc.setObject(root);
        
        QFile file(dataFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }
    
    void loadData() {
        QFile file(dataFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return; // File doesn't exist yet
        }
        
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        
        nextId = root["nextId"].toInt(1);
        QJsonArray expensesArray = root["expenses"].toArray();
        
        expenses.clear();
        for (const auto& value : expensesArray) {
            QJsonObject obj = value.toObject();
            Expense expense;
            expense.id = obj["id"].toInt();
            expense.description = obj["description"].toString();
            expense.amount = obj["amount"].toDouble();
            expense.category = obj["category"].toString();
            expense.date = QDate::fromString(obj["date"].toString(), Qt::ISODate);
            expense.notes = obj["notes"].toString();
            expenses.push_back(expense);
        }
    }
    
private slots:
    void exportToCSV() {
        QString fileName = QFileDialog::getSaveFileName(this, "Export Report to CSV", 
                                                       "expense_report.csv", "CSV Files (*.csv)");
        if (fileName.isEmpty()) return;
        
        // Get filtered expenses based on current report settings
        QString categoryFilter = reportCategoryFilter->currentText();
        QDate startDate = reportStartDate->date();
        QDate endDate = reportEndDate->date();
        
        std::vector<Expense> filteredExpenses;
        for (const auto& expense : expenses) {
            bool categoryMatch = (categoryFilter == "All Categories" || expense.category == categoryFilter);
            bool dateMatch = (expense.date >= startDate && expense.date <= endDate);
            
            if (categoryMatch && dateMatch) {
                filteredExpenses.push_back(expense);
            }
        }
        
        exportExpensesToCSV(fileName, filteredExpenses);
    }
    
    void exportAllToCSV() {
        QString fileName = QFileDialog::getSaveFileName(this, "Export All Expenses to CSV", 
                                                       "all_expenses.csv", "CSV Files (*.csv)");
        if (fileName.isEmpty()) return;
        
        exportExpensesToCSV(fileName, expenses);
    }
    
    void exportExpensesToCSV(const QString& fileName, const std::vector<Expense>& expensesToExport) {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Export Error", "Could not open file for writing.");
            return;
        }
        
        QTextStream stream(&file);
        
        // Write header
        stream << "Date,Description,Amount,Category,Notes\n";
        
        // Write expenses
        for (const auto& expense : expensesToExport) {
            stream << expense.date.toString("yyyy-MM-dd") << ","
                   << "\"" << expense.description << "\","
                   << expense.amount << ","
                   << "\"" << expense.category << "\","
                   << "\"" << expense.notes << "\"\n";
        }
        
        QMessageBox::information(this, "Export Complete", 
                               QString("Successfully exported %1 expenses to CSV file.").arg(expensesToExport.size()));
    }
};

// Application entry point
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("Expense Tracker");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Personal Finance Tools");
    
    // Set application style
    app.setStyle("Fusion");
    
    // Dark theme palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);
    
    ExpenseTracker tracker;
    tracker.show();
    
    return app.exec();
}


#include "main.moc"
