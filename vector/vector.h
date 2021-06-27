#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;

	RawMemory& operator=(const RawMemory& rhs) = delete;

	RawMemory(RawMemory&& other) noexcept {
		Swap(other);
	}

	RawMemory& operator=(RawMemory&& rhs) noexcept {
		Swap(rhs);
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
	// Выделяет сырую память под n элементов и возвращает указатель на неё
	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	// Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
	Vector() = default;

	explicit Vector(size_t size)
		: data_(size)
		, size_(size) {
		std::uninitialized_value_construct_n(data_.GetAddress(), size);
	}

	Vector(const Vector& other)
		: data_(other.size_)
		, size_(other.size_) {
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
	}

	Vector(Vector&& other) noexcept
		: data_(std::move(other.data_))
		, size_(std::exchange(other.size_, 0)) {
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector rhs_copy(rhs);
				Swap(rhs_copy);
			}
			else {
				if (rhs.Size() < size_) {
					std::copy(rhs.data_.GetAddress(), rhs.data_ + rhs.Size(), data_.GetAddress());
					std::destroy_n(data_ + rhs.Size(), size_ - rhs.Size());
				}
				else {
					std::copy(rhs.data_.GetAddress(), rhs.data_ + size_, data_.GetAddress());
					std::uninitialized_copy_n(rhs.data_ + size_, rhs.Size() - size_, data_.GetAddress() + size_);
				}
				size_ = rhs.Size();
			}
		}
		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		Swap(rhs);
		return *this;
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

	void Reserve(size_t new_capacity) {
		if (new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(new_capacity);
		CopyOrMoveAndSwap(new_data);
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < size_);
		return data_[index];
	}

	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

	void Resize(size_t new_size) {
		if (new_size < size_) {
			std::destroy_n(data_ + new_size, size_ - new_size);
		}
		else if (new_size > size_) {
			Reserve(new_size);
			std::uninitialized_default_construct_n(data_ + size_, new_size - size_);
		}
		size_ = new_size;
	}

	void PushBack(const T& value) {
		EmplaceBack(value);
	}

	void PushBack(T&& value) {
		EmplaceBack(std::move(value));
	}

	void PopBack() {
		std::destroy_at(data_ + size_ - 1);
		--size_;
	}

	template <typename... Args>
	T& EmplaceBack(Args&&... args) {
		if (size_ == Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			new (new_data + size_) T(std::forward<Args>(args)...);
			CopyOrMoveAndSwap(new_data);
		}
		else {
			new (data_ + size_) T(std::forward<Args>(args)...);
		}
		++size_;
		return data_[size_ - 1];
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;

	void CopyOrMoveAndSwap(RawMemory<T>& new_data) {
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
		}
		else {
			std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
		}
		std::destroy_n(data_.GetAddress(), size_);
		data_.Swap(new_data);
	}
};