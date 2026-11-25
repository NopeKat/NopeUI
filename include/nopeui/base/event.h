#ifndef NOPEUI_BASE_EVENT_H_
#define NOPEUI_BASE_EVENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace nopeui {

template <typename... Args>
class Event;

class Subscription {
public:
    Subscription() noexcept = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : state_(std::move(other.state_)),
          id_(other.id_),
          disconnect_(other.disconnect_) {
        other.id_ = 0;
        other.disconnect_ = nullptr;
    }

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            Reset();
            state_ = std::move(other.state_);
            id_ = other.id_;
            disconnect_ = other.disconnect_;
            other.id_ = 0;
            other.disconnect_ = nullptr;
        }
        return *this;
    }

    ~Subscription() {
        Reset();
    }

    void Reset() noexcept {
        if (state_ != nullptr && disconnect_ != nullptr) {
            disconnect_(state_.get(), id_);
        }
        state_.reset();
        id_ = 0;
        disconnect_ = nullptr;
    }

    explicit operator bool() const noexcept {
        return state_ != nullptr && disconnect_ != nullptr;
    }

private:
    using DisconnectFn = void (*)(void*, std::uint64_t);

    Subscription(std::shared_ptr<void> state, std::uint64_t id, DisconnectFn disconnect) noexcept
        : state_(std::move(state)), id_(id), disconnect_(disconnect) {}

    template <typename... Args>
    friend class Event;

    std::shared_ptr<void> state_;
    std::uint64_t id_ = 0;
    DisconnectFn disconnect_ = nullptr;
};

template <typename... Args>
class Event {
public:
    using Handler = std::function<void(Args...)>;

    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) noexcept = default;
    Event& operator=(Event&&) noexcept = default;

    Subscription Subscribe(Handler handler) {
        const std::uint64_t id = next_id_++;
        state_->handlers.emplace(id, std::move(handler));
        return Subscription(state_, id, &Event::Disconnect);
    }

    void Raise(Args... args) const {
        std::vector<std::pair<std::uint64_t, Handler>> snapshot;
        snapshot.reserve(state_->handlers.size());
        for (const auto& entry : state_->handlers) {
            snapshot.emplace_back(entry.first, entry.second);
        }

        for (auto& entry : snapshot) {
            if (entry.second) {
                entry.second(args...);
            }
        }
    }

    std::size_t HandlerCount() const noexcept {
        return state_->handlers.size();
    }

private:
    struct State {
        std::map<std::uint64_t, Handler> handlers;
    };

    static void Disconnect(void* state, std::uint64_t id) noexcept {
        static_cast<State*>(state)->handlers.erase(id);
    }

    std::shared_ptr<State> state_ = std::make_shared<State>();
    std::uint64_t next_id_ = 1;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_EVENT_H_
