#ifndef AURA_COPY_HPP
#define AURA_COPY_HPP

#include <aura/backend.hpp>
#include <aura/device_array.hpp>

namespace aura {

/// copy to device array
template <typename T>
void copy(device_array<T>& dst, const T* src, backend::feed& f) 
{
	backend::copy<T>(dst.begin(), src, dst.size(), f);
}

/// copy from device array
template <typename T>
void copy(T* dst, const device_array<T>& src, feed& f) 
{
	backend::copy<T>(dst, src.begin(), src.size(), f);
}

/// copy from std::vector to device array
template <typename T>
void copy(device_array<T>& dst, const std::vector<T>& src, feed& f)
{
	backend::copy<T>(dst.begin(), &src[0], dst.size(), f);
}

/// copy from device array to std::vector
template <typename T>
void copy(std::vector<T>& dst, const device_array<T>& src, feed& f)
{
	backend::copy<T>(&dst[0], src.begin(), src.size(), f);
}
} // namespace aura

#endif // AURA_COPY_HPP

