#pragma once
#include <cassert>
#include <cstdlib>
#include <string>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <memory>
#include <utility>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (*this != &rhs) {
            RawMemory<T> copy(rhs);
            Swap(copy);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:

    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size) 
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) 
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), 
                                  size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (size_ > rhs.size_) {
                    for (size_t index = 0; index != rhs.size_; ++index) {
                        data_[index] = rhs.data_[index];
                    }
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else {
                    for (size_t index = 0; index != size_; ++index) {
                        data_[index] = rhs.data_[index];
                    }
                    std::uninitialized_copy_n(rhs.data_ + size_,
                                              rhs.size_ - size_,
                                              data_ + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    template <typename Type>
    void PushBack(Type&& value) {
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            new (new_data + size_) T(std::forward<Type>(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::forward<Type>(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        return data_[size_++];
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        auto new_index = std::distance(begin(), iterator(pos));
        if (size_ == Capacity()) {
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            new (new_data + new_index) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), new_index, new_data.GetAddress());
                std::uninitialized_move_n(data_ + new_index, size_ - new_index, new_data + new_index + 1);
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), new_index, new_data.GetAddress());
                std::uninitialized_copy_n(data_ + new_index, size_ - new_index, new_data + new_index + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            T tmp(std::forward<Args>(args)...);
            new (end()) T(std::move(data_[size_ - 1]));
            std::move_backward(data_ + new_index, end() - 1, end());
            data_[new_index] = std::move(tmp);
        }
        ++size_;
        return iterator{ begin() + new_index };
    }

    void PopBack() {
        if (size_ != 0) {
            std::destroy_at(data_ + (--size_));
        }
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos <= end());
        assert(size_ != 0);
        auto new_index = std::distance(begin(), iterator(pos));
        std::move(begin() + (new_index + 1), end(), begin() + new_index);
        std::destroy_at(data_ + size_ - 1);
        --size_;
        return iterator{ begin() + new_index };
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    T& At(size_t index) {
        using namespace std::string_literals;
        if (index >= size_) throw std::out_of_range("Invalid vector index"s);
        return data_[index];
    }

    const T& At(size_t index) const {
        using namespace std::string_literals;
        if (index >= size_) throw std::out_of_range("Invalid vector index"s);
        return data_[index];
    }

    void Clear() noexcept {
        std::destroy_n(data_.GetAddress(), size_);
        size_ = 0;
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= size_) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

private:
    
    RawMemory<T> data_ = {};
    size_t size_ = 0;
  
};