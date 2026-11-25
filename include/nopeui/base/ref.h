#ifndef NOPEUI_BASE_REF_H_
#define NOPEUI_BASE_REF_H_

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace nopeui {

class RefCounted {
public:
    RefCounted() noexcept = default;
    RefCounted(const RefCounted&) = delete;
    RefCounted& operator=(const RefCounted&) = delete;

    virtual ~RefCounted() = default;

    void AddRef() const noexcept {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void Release() const noexcept {
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete const_cast<RefCounted*>(this);
        }
    }

private:
    mutable std::atomic<std::uint32_t> ref_count_{1};
};

template <typename T>
class Ref {
    static_assert(std::is_base_of_v<RefCounted, T>, "Ref<T> requires T to derive from RefCounted");

public:
    Ref() noexcept = default;

    // Adopts the existing reference; does not call AddRef().
    explicit Ref(T* ptr) noexcept : ptr_(ptr) {}

    Ref(const Ref& other) noexcept : ptr_(other.ptr_) {
        if (ptr_ != nullptr) {
            ptr_->AddRef();
        }
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Ref(const Ref<U>& other) noexcept : ptr_(other.Get()) {
        if (ptr_ != nullptr) {
            ptr_->AddRef();
        }
    }

    Ref(Ref&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ~Ref() {
        Reset();
    }

    Ref& operator=(const Ref& other) noexcept {
        if (this != &other) {
            T* next = other.ptr_;
            if (next != nullptr) {
                next->AddRef();
            }
            Reset();
            ptr_ = next;
        }
        return *this;
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Ref& operator=(const Ref<U>& other) noexcept {
        T* next = other.Get();
        if (next != nullptr) {
            next->AddRef();
        }
        Reset();
        ptr_ = next;
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void Reset() noexcept {
        if (ptr_ != nullptr) {
            T* old = ptr_;
            ptr_ = nullptr;
            old->Release();
        }
    }

    T* Get() const noexcept {
        return ptr_;
    }

    T* operator->() const noexcept {
        return ptr_;
    }

    T& operator*() const noexcept {
        return *ptr_;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

private:
    T* ptr_ = nullptr;
};

}  // namespace nopeui

#endif  // NOPEUI_BASE_REF_H_
