#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
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
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
	}

	Vector(Vector&& other) noexcept
		: data_(std::move(other.data_))
		, size_(std::exchange(other.size_, 0)) {
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
				const size_t copy_count = rhs.Size() < size_ ? rhs.Size() : size_;
				std::copy(rhs.data_.GetAddress(), rhs.data_ + copy_count, data_.GetAddress());
				if (rhs.Size() < size_) {
					std::destroy_n(data_ + rhs.Size(), size_ - rhs.Size());
				}
				else if (rhs.Size() > size_) {
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
		return data_.GetAddress();
	}

	const_iterator cend() const noexcept {
		return data_ + size_;
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

	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::move(value));
	}

	iterator Erase(const_iterator pos) {
		iterator res_pos = const_cast<iterator>(pos);
		std::move(res_pos + 1, end(), res_pos);
		PopBack();
		return res_pos;
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

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		iterator res_pos = nullptr;
		if (pos == cend()) {
			res_pos = &EmplaceBack(std::forward<Args>(args)...);
		}
		else if (size_ == Capacity()) {
			RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
			const size_t dist_pos = pos - begin();
			new (new_data + dist_pos) T(std::forward<Args>(args)...);
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(begin(), dist_pos, new_data.GetAddress());
			}
			else {
				try {
					std::uninitialized_copy_n(begin(), dist_pos, new_data.GetAddress());
				}
				catch (...) {
					std::destroy_at(new_data + dist_pos);
				}
			}
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(data_ + dist_pos, size_ - dist_pos, new_data + dist_pos + 1);
			}
			else {
				try {
					std::uninitialized_copy_n(data_ + dist_pos, size_ - dist_pos, new_data + dist_pos + 1);
				}
				catch (...) {
					std::destroy_n(new_data.GetAddress(), dist_pos + 1);
				}
			}
			std::destroy_n(begin(), size_);
			data_.Swap(new_data);
			res_pos = begin() + dist_pos;
			++size_;
		}
		else {
			T new_elem(std::forward<Args>(args)...);
			new (data_ + size_) T(std::move(data_[size_ - 1u]));
			res_pos = const_cast<iterator>(pos);
			std::move_backward(res_pos, std::prev(end()), end());
			*res_pos = std::move(new_elem);
			++size_;
		}
		return res_pos;
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