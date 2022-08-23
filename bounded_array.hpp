#pragma once

#include <array>

template<class T, std::size_t N>
class bounded_array
{
  public:
    constexpr bounded_array() noexcept
      : elements_{{}},
        size_{0}
    {}

    constexpr std::size_t size() const
    {
      return size_;
    }

    constexpr void push_back(const T& value)
    {
      elements_[size_] = value;
      ++size_;
    }

    constexpr void clear()
    {
      size_ = 0;
    }

    static constexpr std::size_t capacity()
    {
      return N;
    }

    constexpr T& operator[](std::size_t i)
    {
      return elements_[i];
    }

    constexpr const T& operator[](std::size_t i) const
    {
      return elements_[i];
    }

  private:
    std::array<T,N> elements_;
    std::size_t size_;
};

