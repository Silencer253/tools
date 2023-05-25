#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iostream>

template <typename T>
class RawMemory {
public:
	RawMemory(const RawMemory&) = delete;

	RawMemory& operator=(const RawMemory& rhs) = delete;

	RawMemory(RawMemory&& other) noexcept :
		buffer_(std::move(other.GetAddress())),
		capacity_(other.capacity_)
	{
		other.buffer_ = nullptr;
		other.capacity_ = 0;
	}

	RawMemory& operator=(RawMemory&& rhs) noexcept
	{
		buffer_ = std::move(rhs.buffer_);
		capacity_ = std::move(rhs.capacity_);
		return *this;
	}

	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
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

	void Nullify()
	{
		buffer_ = nullptr;
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
private:
	// Вызывает деструкторы n объектов массива по адресу buf
	static void DestroyN(T* buf, size_t n) noexcept {
		for (size_t i = 0; i != n; ++i) {
			Destroy(buf + i);
		}
	}

	// Создаёт копию объекта elem в сырой памяти по адресу buf
	static void CopyConstruct(T* buf, const T& elem) {
		new (buf) T(elem);
	}

	// Вызывает деструктор объекта по адресу buf
	static void Destroy(T* buf) noexcept {
		buf->~T();
	}

public:
	Vector(Vector&& other) noexcept :
		data_(std::move(other.data_)), size_(other.size_)
	{
		other.size_ = 0;
	}

	Vector& operator=(const Vector& rhs)
	{
		if (this == &rhs) {
			return *this;
		}

		if (rhs.size_ > data_.Capacity()) {
			Vector<T> tmp(rhs);
			Swap(tmp);
		} else {
			if (size_ > rhs.size_) {
				DestroyN(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
				for (size_t i = 0; i < rhs.size_; ++i) {
					*(data_.GetAddress() + i) = *(rhs.data_.GetAddress() + i);
				}
			} else {
				for (size_t i = 0; i < size_; ++i) {
					*(data_.GetAddress() + i) = *(rhs.data_.GetAddress() + i);
				}
				for (size_t i = size_; i < rhs.size_; ++i) {
					CopyConstruct((data_.GetAddress() + i),
								  *(rhs.data_.GetAddress() + i));
				}
			}
			size_ = rhs.size_;
		}

		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept
	{
		if (this == &rhs) {
			return *this;
		}

		if (rhs.size_ > data_.Capacity()) {
			Vector<T> tmp(std::move(rhs));
			Swap(tmp);
		}

		return *this;
	}

	void Swap(Vector& other) noexcept
	{
		data_.Swap(other.data_);
		std::swap(this->size_, other.size_);
	}

	Vector() = default;

	explicit Vector(size_t size) : data_(size), size_(size)
	{
		std::uninitialized_value_construct_n(data_.GetAddress(), size);
	}

	Vector(const Vector& other) : data_(other.size_), size_(other.size_)
	{
		std::uninitialized_copy_n(other.data_.GetAddress(), other.size_,
								  data_.GetAddress());
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

	size_t Size() const noexcept {
		return size_;
	}

	void Reserve(const size_t t_new_capacity)
	{
		if (t_new_capacity <= data_.Capacity()) {
			return;
		}
		RawMemory<T> new_data(t_new_capacity);

		// constexpr оператор if будет вычислен во время компиляции
		if constexpr (std::is_nothrow_move_constructible_v<T> ||
				!std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(data_.GetAddress(), size_,
									  new_data.GetAddress());
		} else {
			std::uninitialized_copy_n(data_.GetAddress(), size_,
									  new_data.GetAddress());
		}
		std::destroy_n(data_.GetAddress(), size_);
		data_.Swap(new_data);
	}

	size_t Capacity() const noexcept
	{
		return data_.Capacity();
	}

	const T& operator[](size_t index) const noexcept
	{
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept
	{
		assert(index < size_);
		return data_[index];
	}

	void Resize(size_t new_size)
	{
		if (new_size < size_) {
			DestroyN(&data_[new_size], size_ - new_size);
		}
		else {
			size_t size_buf = size_;
			Reserve(new_size);
			std::uninitialized_value_construct_n(&data_[size_buf], new_size - size_buf);
		}
		size_ = new_size;
	}

	void PushBack(const T& value)
	{
		if (size_ == Capacity()) {
		   size_t new_size = size_ * 2;

		   if (size_ == 0) {
			   new_size = 1;
		   }

		   RawMemory<T> new_data(new_size);
		   CopyConstruct(&new_data[size_], value);

		   if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			   std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
		   } else {
			   std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
		   }

		   std::destroy_n(data_.GetAddress(), size_);

		   data_.Swap(new_data);
	   } else {
		   CopyConstruct(&data_[size_], value);
	   }
	   ++size_;
	}

	void PushBack(T&& value)
	{
		if (size_ == Capacity()) {
			size_t new_size = size_ * 2;
			if (size_ == 0) {
				new_size = 1;
			}

			RawMemory<T> new_data(new_size);
			if constexpr (std::is_nothrow_move_constructible_v<T> ||
					!std::is_copy_constructible_v<T>) {
				new (new_data + size_) T(std::move(value));

				std::uninitialized_move_n(data_.GetAddress(), size_,
										  new_data.GetAddress());
			} else {
				new (new_data + size_) T(std::move(value));

				std::uninitialized_copy_n(data_.GetAddress(), size_,
										  new_data.GetAddress());
			}

			std::destroy_n(data_.GetAddress(), size_);

			data_.Swap(new_data);
		} else {
			new (data_ + size_) T(std::move(value));
		}
		++size_;
	}

	void PopBack() noexcept
	{
		Destroy(&data_[size_ - 1]);
		--size_;
	}

	template <typename... Args>
	T& EmplaceBack(Args&&... args)
	{
		if (size_ == Capacity()) {
			size_t new_size = size_ * 2;
			if (size_ == 0) {
				new_size = 1;
			}

			RawMemory<T> new_data(new_size);
			T* ptr = nullptr;
			if constexpr (std::is_nothrow_move_constructible_v<T> ||
					!std::is_copy_constructible_v<T>) {
				new (new_data + size_) T(std::forward<Args>(args)...);
				ptr = new_data + size_;
				std::uninitialized_move_n(data_.GetAddress(), size_,
										  new_data.GetAddress());
			} else {
				new (new_data + size_) T(std::forward<Args>(args)...);
				ptr = new_data + size_;
				std::uninitialized_copy_n(data_.GetAddress(), size_,
										  new_data.GetAddress());
			}

			std::destroy_n(data_.GetAddress(), size_);

			data_.Swap(new_data);
			++size_;
			return *(ptr);
		} else {
			new (data_ + size_) T(std::forward<Args>(args)...);
			T* ptr = (data_ + size_);
			++size_;
			return *(ptr);
		}
	}

	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() noexcept
	{
		return data_.GetAddress();
	}

	iterator end() noexcept
	{
		return (data_ + size_);
	}

	const_iterator begin() const noexcept
	{
		return data_.GetAddress();
	}

	const_iterator end() const noexcept
	{
		return (data_ + size_);
	}

	const_iterator cbegin() const noexcept
	{
		return data_.GetAddress();
	}

	const_iterator cend() const noexcept
	{
		return (data_ + size_);
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args)
	{
		size_t index = pos - begin();
		iterator it = begin() + index;
		if (size_ == 0) {
			return &EmplaceBack(std::forward<Args>(args)...);
		}
		if (size_ == Capacity()) {
			/* На первом шаге нужно выделить новый блок сырой памяти с
			 * удвоенной вместимостью
			 */
			size_t new_capacity = size_ * 2;
			if (size_ == 0) {
				new_capacity = 1;
			}

			RawMemory<T> new_data(new_capacity);

			/* Затем сконструировать в ней вставляемый элемент, используя
			 * конструктор копирования или перемещения
			 */
			new (new_data + index) T(std::forward<Args>(args)...);

			try {
				/* Затем копируются либо перемещаются элементы, которые
				 * предшествуют вставленному элементу
				 */
				if constexpr (std::is_nothrow_move_constructible_v<T> ||
						!std::is_copy_constructible_v<T>) {
					std::uninitialized_move_n(data_.GetAddress(), index,
											  new_data.GetAddress());
				} else {
					std::uninitialized_copy_n(data_.GetAddress(), index,
											  new_data.GetAddress());
				}
			} catch (...) {
				/* Если исключение выбросится при их копировании, нужно
				 * разрушить ранее вставленный элемент в обработчике
				 */
				Destroy(new_data + index);
			}

			try {
				/* Затем копируются либо перемещаются элементы, которые следуют
				 * за вставляемым
				 */
				for (size_t i = index; i < size_; ++i) {
					if constexpr (std::is_nothrow_move_constructible_v<T> ||
							!std::is_copy_constructible_v<T>) {
						new (new_data + i + 1) T(std::move(*(data_ + i)));
					} else {
						new (new_data + i + 1) T(*(data_ + i));
					}
				}
			} catch (...) {
				/* Если выбросится исключение при конструировании элементов,
				 * следующих за pos, выделенные рамкой элементы нужно разрушить.
				 */
				DestroyN(new_data.GetAddress(), index);
			}

			/* На последнем шаге разрушаются исходные элементы, а занимаемая
			 * память возвращается обратно в кучу. Вектор обновляет свой размер
			 * и вместимость и ссылается на новый блок памяти.
			 */
			DestroyN(data_.GetAddress(), size_);
			data_.Swap(new_data);
		} else {
			// Сначала скопируйте или переместите значение во временный объект
			T tmp(std::forward<Args>(args)...);

			/* Сначала в неинициализированной области, следующей за последним
			 * элементом, создайте копию или переместите значение последнего
			 * элемента вектора
			 */
			new (data_ + size_) T(std::move(*(end() - 1)));

			// Затем переместите элементы диапазона [pos, end()-1) вправо на один элемент
			std::move_backward(it, end() - 1, end());

			/* После перемещения элементов нужно переместить временное значение
			 * во вставляемую позицию
			 */
			data_[index] = std::move(tmp);
		}

		size_++;
		return begin() + index;
	}

	iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/
	{
		size_t index = pos - begin();
		iterator it = begin() + index + 1;

		std::move(it, end(), begin() + index);
		Destroy(end() - 1);

		size_ -= 1;
		return begin() + index;
	}

	iterator Insert(const_iterator pos, const T& value)
	{
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value)
	{
		return Emplace(pos, std::forward<T>(value));
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;
};
