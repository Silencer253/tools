#include <stdexcept>
#include <utility>
#include <iostream>


// Исключение этого типа должно генерироватся при обращении к пустому optional
class BadOptionalAccess : public std::exception {
public:
	using exception::exception;

	virtual const char* what() const noexcept override {
		return "Bad optional access";
	}
};

template <typename T>
class Optional {
public:
	Optional() = default;
	Optional(const T& value)
	{
		::new (static_cast<void*>(data_)) T(value);
		is_initialized_ = true;
	}

	Optional(T&& value)
	{
		::new (static_cast<void*>(data_)) T(std::move(value));
		is_initialized_ = true;
	}

	Optional(const Optional& other)
	{
		if (other.is_initialized_) {
			const T *obj = reinterpret_cast<const T *>(other.data_);
			::new (static_cast<void*>(data_)) T(*obj);
			is_initialized_ = true;
		}
	}

	Optional(Optional&& other)
	{
		if (other.is_initialized_) {
			::new (static_cast<void*>(data_)) T(std::move(*other));
			is_initialized_ = true;
		}
	}

	Optional& operator=(const T& value)
	{
		if (is_initialized_) {
			T *this_obj = reinterpret_cast<T *>(data_);
			*this_obj = value;
		} else {
			::new (static_cast<void*>(data_)) T(value);
			is_initialized_ = true;
		}

		return *this;
	}

	Optional& operator=(T&& rhs)
	{
		if (is_initialized_) {
			T *this_obj = reinterpret_cast<T *>(data_);
			*this_obj = std::move(rhs);
		} else {
			::new (static_cast<void*>(data_)) T(std::move(rhs));
			is_initialized_ = true;
		}

		return *this;
	}

	Optional& operator=(const Optional& rhs)
	{
		if (rhs.is_initialized_ == false) {
			if (is_initialized_) {
				Value().~T();
				is_initialized_ = false;
			}
			return *this;
		}

		if (is_initialized_) {
			T *this_obj = reinterpret_cast<T *>(data_);
			const T *rhs_obj = reinterpret_cast<const T *>(rhs.data_);
			*this_obj = *rhs_obj;
		} else {
			const T *obj = reinterpret_cast<const T *>(rhs.data_);
			::new (static_cast<void*>(data_)) T(*obj);
			is_initialized_ = true;
		}

		return *this;
	}

	Optional& operator=(Optional&& rhs)
	{
		if (rhs.is_initialized_ == false) {
			if (is_initialized_) {
				Value().~T();
				is_initialized_ = false;
			}
			return *this;
		}

		if (is_initialized_) {
			T *this_obj = reinterpret_cast<T *>(data_);
			T *rhs_obj = reinterpret_cast<T *>(rhs.data_);
			*this_obj = std::move(*rhs_obj);
		} else {
			T *rhs_obj = reinterpret_cast<T *>(rhs.data_);
			::new (static_cast<void*>(data_)) T(std::move(*rhs_obj));
			is_initialized_ = true;
		}

		return *this;
	}

	~Optional()
	{
		if (is_initialized_) {
			T *obj = reinterpret_cast<T *>(data_);
			obj->T::~T();
			is_initialized_ = false;
		}
	}

	bool HasValue() const
	{
		return is_initialized_;
	}

	// Операторы * и -> не должны делать никаких проверок на пустоту Optional.
	// Эти проверки остаются на совести программиста
	T& operator*()
	{
		T *obj = reinterpret_cast<T *>(data_);
		return *obj;
	}

	const T& operator*() const
	{
		const T *obj = reinterpret_cast<const T *>(data_);
		return *obj;
	}

	T* operator->()
	{
		T *obj = reinterpret_cast<T *>(data_);
		return obj;
	}

	const T* operator->() const
	{
		const T *obj = reinterpret_cast<const T *>(data_);
		return obj;
	}

	// Метод Value() генерирует исключение BadOptionalAccess, если Optional пуст
	T& Value() {
		if (is_initialized_ == false) {
			throw BadOptionalAccess();
		}

		T *obj = reinterpret_cast<T *>(data_);
		return *obj;
	}

	const T& Value() const
	{
		if (is_initialized_ == false) {
			throw BadOptionalAccess();
		}

		const T *this_obj = reinterpret_cast<const T *>(data_);
		return *this_obj;
	}

	void Reset()
	{
		if (is_initialized_) {
			Value().~T();
		}
		is_initialized_ = false;
	}

private:
	// alignas нужен для правильного выравнивания блока памяти
	alignas(T) char data_[sizeof(T)];
	bool is_initialized_ = false;
};
