#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <syncstream>
#include <chrono>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace sys = boost::system;
namespace net = boost::asio;

using namespace std::chrono;
using namespace std::literals;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        std::osyncstream os{std::cout};
        os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
           << "s] "sv << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, HotDogHandler handler, std::shared_ptr<GasCooker> gas_cooker)
        : io_{io}, id_{id}, handler_{std::move(handler)}, gas_cooker_(std::move(gas_cooker)) { }

    // Запускает асинхронное выполнение заказа
    void Execute() {
        MakeBread();
        MakeSausage();
    }

private:
    void MakeBread() {
        logger_.LogMessage("Start baking bread");
        bread_->StartBake(*gas_cooker_, []() {});
        bread_timer_.async_wait([self = shared_from_this()](sys::error_code) {
            self->bread_->StopBaking();
            net::defer(self->strand_, [self = std::move(self)]() {
                self->OnBreadMade();
            });
        });
    }

    void OnBreadMade() {
        logger_.LogMessage("Breed has been baked."sv);
        CheckReadiness();
    }

    void MakeSausage() {
        logger_.LogMessage("Start frying sausage");
        sausage_->StartFry(*gas_cooker_, []() {});
        sausage_timer_.async_wait([self = shared_from_this()](sys::error_code) {
            self->sausage_->StopFry();
            net::defer(self->strand_, [self = std::move(self)]() {
                self->OnSausageMade();
            });
        });
    }

    void OnSausageMade() {
        logger_.LogMessage("Sausage has been fried."sv);
        CheckReadiness();
    }

    void CheckReadiness() {
        if (sausage_->IsCooked() && bread_->IsCooked()) {
            handler_(Result{HotDog{id_, sausage_, bread_}});
        }
    }

    int id_;
    net::io_context& io_;
    std::shared_ptr<GasCooker> gas_cooker_;
    HotDogHandler handler_;
    Logger logger_{std::to_string(id_)};
    std::shared_ptr<Sausage> sausage_ = std::make_shared<Sausage>(id_); 
    std::shared_ptr<Bread> bread_ = std::make_shared<Bread>(id_);
    net::steady_timer bread_timer_{io_, 1s};
    net::steady_timer sausage_timer_{io_, 1500ms};
    net::strand<net::io_context::executor_type> strand_ = net::make_strand(io_);
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        // 1) Выпекаем булку в течение 1 секунды, жарим сосиску в течение 1.5 секунд
        // 2) Собираем из них хот-дог
        const int order_id = ++next_order_id_;
        std::make_shared<Order>(io_, order_id, std::move(handler), gas_cooker_)->Execute();
    }

private:
    net::io_context& io_;
    int next_order_id_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
