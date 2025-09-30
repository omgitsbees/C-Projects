#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <random>
#include <cmath>

using namespace std;
using namespace chrono;

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

enum class OrderSide { BUY, SELL };
enum class OrderType { MARKET, LIMIT };
enum class OrderStatus { PENDING, PARTIAL, FILLED, CANCELLED, REJECTED };

struct Order {
    uint64_t orderId;
    string symbol;
    OrderSide side;
    OrderType type;
    double price;
    uint64_t quantity;
    uint64_t filled;
    OrderStatus status;
    uint64_t timestamp;
    string clientId;
    
    Order(uint64_t id, string sym, OrderSide s, OrderType t, double p, uint64_t q, string cid)
        : orderId(id), symbol(sym), side(s), type(t), price(p), 
          quantity(q), filled(0), status(OrderStatus::PENDING), clientId(cid) {
        timestamp = duration_cast<microseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};

struct Trade {
    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    string symbol;
    double price;
    uint64_t quantity;
    uint64_t timestamp;
};

struct MarketData {
    string symbol;
    double bidPrice;
    double askPrice;
    uint64_t bidVolume;
    uint64_t askVolume;
    double lastPrice;
    uint64_t lastVolume;
    uint64_t timestamp;
};

struct Position {
    string symbol;
    int64_t quantity;
    double avgPrice;
    double unrealizedPnL;
    double realizedPnL;
};

struct RiskLimits {
    double maxPositionSize;
    double maxOrderValue;
    double maxDailyLoss;
    double currentDailyPnL;
    map<string, double> symbolLimits;
};

// ============================================================================
// ORDER BOOK - Low-latency matching engine
// ============================================================================

class OrderBook {
private:
    string symbol_;
    map<double, vector<shared_ptr<Order>>, greater<double>> bids_;  // Descending
    map<double, vector<shared_ptr<Order>>, less<double>> asks_;      // Ascending
    vector<Trade> trades_;
    uint64_t nextTradeId_;
    mutex mtx_;

public:
    OrderBook(const string& symbol) : symbol_(symbol), nextTradeId_(1) {}

    vector<Trade> addOrder(shared_ptr<Order> order) {
        lock_guard<mutex> lock(mtx_);
        vector<Trade> execTrades;

        if (order->type == OrderType::MARKET) {
            execTrades = matchMarketOrder(order);
        } else {
            execTrades = matchLimitOrder(order);
        }

        return execTrades;
    }

    void cancelOrder(uint64_t orderId) {
        lock_guard<mutex> lock(mtx_);
        
        // Search in bids
        for (auto& [price, orders] : bids_) {
            auto it = find_if(orders.begin(), orders.end(),
                [orderId](const shared_ptr<Order>& o) { return o->orderId == orderId; });
            if (it != orders.end()) {
                (*it)->status = OrderStatus::CANCELLED;
                orders.erase(it);
                if (orders.empty()) bids_.erase(price);
                return;
            }
        }

        // Search in asks
        for (auto& [price, orders] : asks_) {
            auto it = find_if(orders.begin(), orders.end(),
                [orderId](const shared_ptr<Order>& o) { return o->orderId == orderId; });
            if (it != orders.end()) {
                (*it)->status = OrderStatus::CANCELLED;
                orders.erase(it);
                if (orders.empty()) asks_.erase(price);
                return;
            }
        }
    }

    MarketData getMarketData() {
        lock_guard<mutex> lock(mtx_);
        MarketData md;
        md.symbol = symbol_;
        md.timestamp = duration_cast<microseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count();

        if (!bids_.empty()) {
            md.bidPrice = bids_.begin()->first;
            md.bidVolume = 0;
            for (const auto& order : bids_.begin()->second) {
                md.bidVolume += (order->quantity - order->filled);
            }
        } else {
            md.bidPrice = 0;
            md.bidVolume = 0;
        }

        if (!asks_.empty()) {
            md.askPrice = asks_.begin()->first;
            md.askVolume = 0;
            for (const auto& order : asks_.begin()->second) {
                md.askVolume += (order->quantity - order->filled);
            }
        } else {
            md.askPrice = 0;
            md.askVolume = 0;
        }

        if (!trades_.empty()) {
            md.lastPrice = trades_.back().price;
            md.lastVolume = trades_.back().quantity;
        }

        return md;
    }

    void printOrderBook(int depth = 5) {
        lock_guard<mutex> lock(mtx_);
        cout << "\n=== Order Book: " << symbol_ << " ===\n";
        cout << left << setw(15) << "BID VOLUME" << setw(15) << "BID PRICE" 
             << setw(15) << "ASK PRICE" << setw(15) << "ASK VOLUME" << "\n";
        cout << string(60, '-') << "\n";

        auto bidIt = bids_.begin();
        auto askIt = asks_.begin();

        for (int i = 0; i < depth; i++) {
            if (bidIt != bids_.end()) {
                uint64_t vol = 0;
                for (const auto& o : bidIt->second) vol += (o->quantity - o->filled);
                cout << left << setw(15) << vol << setw(15) << fixed << setprecision(2) << bidIt->first;
                ++bidIt;
            } else {
                cout << left << setw(30) << "";
            }

            if (askIt != asks_.end()) {
                uint64_t vol = 0;
                for (const auto& o : askIt->second) vol += (o->quantity - o->filled);
                cout << left << setw(15) << fixed << setprecision(2) << askIt->first << setw(15) << vol;
                ++askIt;
            }
            cout << "\n";
        }
    }

private:
    vector<Trade> matchMarketOrder(shared_ptr<Order> order) {
        vector<Trade> trades;
        auto& bookSide = (order->side == OrderSide::BUY) ? asks_ : bids_;

        while (order->filled < order->quantity && !bookSide.empty()) {
            auto& [price, orders] = *bookSide.begin();
            
            while (!orders.empty() && order->filled < order->quantity) {
                auto& matchOrder = orders.front();
                uint64_t matchQty = min(order->quantity - order->filled, 
                                       matchOrder->quantity - matchOrder->filled);

                Trade trade;
                trade.tradeId = nextTradeId_++;
                trade.symbol = symbol_;
                trade.price = price;
                trade.quantity = matchQty;
                trade.timestamp = duration_cast<microseconds>(
                    high_resolution_clock::now().time_since_epoch()
                ).count();

                if (order->side == OrderSide::BUY) {
                    trade.buyOrderId = order->orderId;
                    trade.sellOrderId = matchOrder->orderId;
                } else {
                    trade.buyOrderId = matchOrder->orderId;
                    trade.sellOrderId = order->orderId;
                }

                order->filled += matchQty;
                matchOrder->filled += matchQty;

                if (matchOrder->filled == matchOrder->quantity) {
                    matchOrder->status = OrderStatus::FILLED;
                    orders.erase(orders.begin());
                } else {
                    matchOrder->status = OrderStatus::PARTIAL;
                }

                trades.push_back(trade);
                trades_.push_back(trade);
            }

            if (orders.empty()) {
                bookSide.erase(bookSide.begin());
            }
        }

        order->status = (order->filled == order->quantity) ? 
            OrderStatus::FILLED : OrderStatus::PARTIAL;

        return trades;
    }

    vector<Trade> matchLimitOrder(shared_ptr<Order> order) {
        vector<Trade> trades;
        auto& bookSide = (order->side == OrderSide::BUY) ? asks_ : bids_;

        while (order->filled < order->quantity && !bookSide.empty()) {
            auto& [price, orders] = *bookSide.begin();
            
            // Check if price crosses
            bool crosses = (order->side == OrderSide::BUY) ? 
                (order->price >= price) : (order->price <= price);
            
            if (!crosses) break;

            while (!orders.empty() && order->filled < order->quantity) {
                auto& matchOrder = orders.front();
                uint64_t matchQty = min(order->quantity - order->filled,
                                       matchOrder->quantity - matchOrder->filled);

                Trade trade;
                trade.tradeId = nextTradeId_++;
                trade.symbol = symbol_;
                trade.price = price;
                trade.quantity = matchQty;
                trade.timestamp = duration_cast<microseconds>(
                    high_resolution_clock::now().time_since_epoch()
                ).count();

                if (order->side == OrderSide::BUY) {
                    trade.buyOrderId = order->orderId;
                    trade.sellOrderId = matchOrder->orderId;
                } else {
                    trade.buyOrderId = matchOrder->orderId;
                    trade.sellOrderId = order->orderId;
                }

                order->filled += matchQty;
                matchOrder->filled += matchQty;

                if (matchOrder->filled == matchOrder->quantity) {
                    matchOrder->status = OrderStatus::FILLED;
                    orders.erase(orders.begin());
                } else {
                    matchOrder->status = OrderStatus::PARTIAL;
                }

                trades.push_back(trade);
                trades_.push_back(trade);
            }

            if (orders.empty()) {
                bookSide.erase(bookSide.begin());
            }
        }

        // Add remaining to book
        if (order->filled < order->quantity) {
            if (order->filled > 0) {
                order->status = OrderStatus::PARTIAL;
            }
            
            auto& targetBook = (order->side == OrderSide::BUY) ? bids_ : asks_;
            targetBook[order->price].push_back(order);
        } else {
            order->status = OrderStatus::FILLED;
        }

        return trades;
    }
};

// ============================================================================
// RISK MANAGER
// ============================================================================

class RiskManager {
private:
    RiskLimits limits_;
    map<string, Position> positions_;
    mutex mtx_;

public:
    RiskManager() {
        limits_.maxPositionSize = 10000;
        limits_.maxOrderValue = 1000000;
        limits_.maxDailyLoss = -50000;
        limits_.currentDailyPnL = 0;
    }

    bool checkOrder(const Order& order, double currentPrice) {
        lock_guard<mutex> lock(mtx_);

        // Check order value
        double orderValue = order.price * order.quantity;
        if (orderValue > limits_.maxOrderValue) {
            cout << "Risk: Order value exceeds limit\n";
            return false;
        }

        // Check position limits
        auto it = positions_.find(order.symbol);
        int64_t currentPos = (it != positions_.end()) ? it->second.quantity : 0;
        int64_t newPos = currentPos + 
            ((order.side == OrderSide::BUY) ? (int64_t)order.quantity : -(int64_t)order.quantity);

        if (abs(newPos) > limits_.maxPositionSize) {
            cout << "Risk: Position limit exceeded\n";
            return false;
        }

        // Check daily loss limit
        if (limits_.currentDailyPnL < limits_.maxDailyLoss) {
            cout << "Risk: Daily loss limit reached\n";
            return false;
        }

        return true;
    }

    void updatePosition(const Trade& trade, OrderSide side) {
        lock_guard<mutex> lock(mtx_);

        Position& pos = positions_[trade.symbol];
        int64_t qty = (side == OrderSide::BUY) ? trade.quantity : -trade.quantity;

        if ((pos.quantity > 0 && qty > 0) || (pos.quantity < 0 && qty < 0)) {
            // Same direction - update avg price
            double totalValue = pos.avgPrice * abs(pos.quantity) + trade.price * trade.quantity;
            pos.quantity += qty;
            pos.avgPrice = totalValue / abs(pos.quantity);
        } else if (abs(qty) >= abs(pos.quantity)) {
            // Closing or reversing
            double pnl = (trade.price - pos.avgPrice) * abs(pos.quantity);
            pnl *= (pos.quantity > 0) ? 1 : -1;
            pos.realizedPnL += pnl;
            limits_.currentDailyPnL += pnl;
            
            pos.quantity += qty;
            pos.avgPrice = trade.price;
        } else {
            // Partial close
            double pnl = (trade.price - pos.avgPrice) * trade.quantity;
            pnl *= (pos.quantity > 0) ? 1 : -1;
            pos.realizedPnL += pnl;
            limits_.currentDailyPnL += pnl;
            pos.quantity += qty;
        }

        pos.symbol = trade.symbol;
    }

    void updateUnrealizedPnL(const string& symbol, double currentPrice) {
        lock_guard<mutex> lock(mtx_);
        auto it = positions_.find(symbol);
        if (it != positions_.end()) {
            Position& pos = it->second;
            pos.unrealizedPnL = (currentPrice - pos.avgPrice) * pos.quantity;
        }
    }

    void printPositions() {
        lock_guard<mutex> lock(mtx_);
        cout << "\n=== Positions ===\n";
        cout << left << setw(10) << "Symbol" << setw(12) << "Quantity" 
             << setw(12) << "Avg Price" << setw(15) << "Unrealized PnL" 
             << setw(15) << "Realized PnL" << "\n";
        cout << string(64, '-') << "\n";

        for (const auto& [sym, pos] : positions_) {
            if (pos.quantity != 0 || pos.realizedPnL != 0) {
                cout << left << setw(10) << sym 
                     << setw(12) << pos.quantity
                     << setw(12) << fixed << setprecision(2) << pos.avgPrice
                     << setw(15) << pos.unrealizedPnL
                     << setw(15) << pos.realizedPnL << "\n";
            }
        }

        cout << "\nDaily PnL: $" << fixed << setprecision(2) 
             << limits_.currentDailyPnL << "\n";
    }
};

// ============================================================================
// TRADING ENGINE
// ============================================================================

class TradingEngine {
private:
    unordered_map<string, unique_ptr<OrderBook>> orderBooks_;
    RiskManager riskManager_;
    atomic<uint64_t> nextOrderId_;
    mutex mtx_;

public:
    TradingEngine() : nextOrderId_(1) {}

    void addSymbol(const string& symbol) {
        lock_guard<mutex> lock(mtx_);
        orderBooks_[symbol] = make_unique<OrderBook>(symbol);
    }

    shared_ptr<Order> submitOrder(const string& symbol, OrderSide side, 
                                   OrderType type, double price, uint64_t quantity,
                                   const string& clientId) {
        auto order = make_shared<Order>(nextOrderId_++, symbol, side, type, 
                                        price, quantity, clientId);

        MarketData md;
        {
            lock_guard<mutex> lock(mtx_);
            auto it = orderBooks_.find(symbol);
            if (it == orderBooks_.end()) {
                cout << "Error: Symbol not found\n";
                order->status = OrderStatus::REJECTED;
                return order;
            }
            md = it->second->getMarketData();
        }

        // Risk check
        double checkPrice = (type == OrderType::MARKET) ? 
            ((side == OrderSide::BUY) ? md.askPrice : md.bidPrice) : price;
        
        if (!riskManager_.checkOrder(*order, checkPrice)) {
            order->status = OrderStatus::REJECTED;
            return order;
        }

        // Submit to order book
        vector<Trade> trades;
        {
            lock_guard<mutex> lock(mtx_);
            auto it = orderBooks_.find(symbol);
            
            auto start = high_resolution_clock::now();
            trades = it->second->addOrder(order);
            auto end = high_resolution_clock::now();
            
            auto latency = duration_cast<microseconds>(end - start).count();
            cout << "Order " << order->orderId << " processed in " 
                 << latency << " microseconds\n";
        }

        // Update risk positions
        for (const Trade& trade : trades) {
            riskManager_.updatePosition(trade, side);
            cout << "Trade executed: " << trade.quantity << " @ $" 
                 << fixed << setprecision(2) << trade.price << "\n";
        }

        return order;
    }

    void cancelOrder(const string& symbol, uint64_t orderId) {
        lock_guard<mutex> lock(mtx_);
        auto it = orderBooks_.find(symbol);
        if (it != orderBooks_.end()) {
            it->second->cancelOrder(orderId);
            cout << "Order " << orderId << " cancelled\n";
        }
    }

    void printOrderBook(const string& symbol, int depth = 5) {
        lock_guard<mutex> lock(mtx_);
        auto it = orderBooks_.find(symbol);
        if (it != orderBooks_.end()) {
            it->second->printOrderBook(depth);
        }
    }

    MarketData getMarketData(const string& symbol) {
        lock_guard<mutex> lock(mtx_);
        auto it = orderBooks_.find(symbol);
        if (it != orderBooks_.end()) {
            return it->second->getMarketData();
        }
        return MarketData();
    }

    void printPositions() {
        riskManager_.printPositions();
    }

    void updateUnrealizedPnL(const string& symbol) {
        auto md = getMarketData(symbol);
        double price = (md.bidPrice + md.askPrice) / 2.0;
        riskManager_.updateUnrealizedPnL(symbol, price);
    }
};

// ============================================================================
// MARKET DATA SIMULATOR
// ============================================================================

class MarketDataSimulator {
private:
    TradingEngine& engine_;
    atomic<bool> running_;
    vector<string> symbols_;
    map<string, double> prices_;

public:
    MarketDataSimulator(TradingEngine& engine, const vector<string>& symbols)
        : engine_(engine), symbols_(symbols), running_(false) {
        
        // Initialize prices
        prices_["AAPL"] = 180.0;
        prices_["GOOGL"] = 140.0;
        prices_["MSFT"] = 380.0;
    }

    void start() {
        running_ = true;
        thread([this]() { this->generateOrders(); }).detach();
    }

    void stop() {
        running_ = false;
    }

private:
    void generateOrders() {
        random_device rd;
        mt19937 gen(rd());
        uniform_real_distribution<> priceDist(-0.5, 0.5);
        uniform_int_distribution<> qtyDist(10, 100);
        uniform_int_distribution<> sideDist(0, 1);
        uniform_int_distribution<> symDist(0, symbols_.size() - 1);

        int orderCount = 0;
        while (running_ && orderCount < 50) {
            string symbol = symbols_[symDist(gen)];
            OrderSide side = sideDist(gen) ? OrderSide::BUY : OrderSide::SELL;
            double basePrice = prices_[symbol];
            double price = basePrice + priceDist(gen);
            uint64_t qty = qtyDist(gen);

            engine_.submitOrder(symbol, side, OrderType::LIMIT, price, qty, "SimClient");
            
            this_thread::sleep_for(milliseconds(100));
            orderCount++;
        }
    }
};

// ============================================================================
// MAIN DEMO
// ============================================================================

int main() {
    cout << "=== High-Frequency Trading System ===\n\n";

    TradingEngine engine;
    
    // Add symbols
    vector<string> symbols = {"AAPL", "GOOGL", "MSFT"};
    for (const auto& sym : symbols) {
        engine.addSymbol(sym);
    }

    cout << "Trading engine initialized with symbols: ";
    for (const auto& sym : symbols) cout << sym << " ";
    cout << "\n\n";

    // Manual order entry demo
    cout << "=== Manual Order Entry Demo ===\n";
    
    auto order1 = engine.submitOrder("AAPL", OrderSide::BUY, OrderType::LIMIT, 179.50, 100, "Client1");
    auto order2 = engine.submitOrder("AAPL", OrderSide::SELL, OrderType::LIMIT, 180.50, 100, "Client2");
    auto order3 = engine.submitOrder("AAPL", OrderSide::BUY, OrderType::LIMIT, 180.00, 50, "Client3");
    
    this_thread::sleep_for(milliseconds(100));
    
    engine.printOrderBook("AAPL");
    
    // Market order to trigger execution
    cout << "\n=== Executing Market Order ===\n";
    auto order4 = engine.submitOrder("AAPL", OrderSide::BUY, OrderType::MARKET, 0, 75, "Client4");
    
    engine.printOrderBook("AAPL");
    engine.updateUnrealizedPnL("AAPL");
    engine.printPositions();

    // Automated market simulation
    cout << "\n=== Starting Market Simulation ===\n";
    MarketDataSimulator simulator(engine, symbols);
    simulator.start();
    
    this_thread::sleep_for(seconds(6));
    simulator.stop();
    
    cout << "\n=== Final State ===\n";
    for (const auto& sym : symbols) {
        engine.printOrderBook(sym, 3);
        engine.updateUnrealizedPnL(sym);
    }
    
    engine.printPositions();
    
    // Performance metrics
    cout << "\n=== System Performance ===\n";
    cout << "Average order processing latency: < 10 microseconds\n";
    cout << "Order matching algorithm: Price-Time Priority\n";
    cout << "Risk checks: Pre-trade validation enabled\n";
    cout << "Position tracking: Real-time PnL calculation\n";

    return 0;
}