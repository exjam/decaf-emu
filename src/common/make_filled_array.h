#pragma once
#include <array>

template<size_t Size, typename Type>
static auto make_filled_array(const Type &value)
{
   std::array<Type, Size> a;
   a.fill(value);
   return a;
}
