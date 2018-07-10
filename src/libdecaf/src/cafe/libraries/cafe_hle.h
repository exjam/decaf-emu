#pragma once
#include "cafe_hle_library.h"
#include <string_view>

namespace cafe::hle
{

void
initialiseLibraries();

Library *
getLibrary(LibraryId id);

Library *
getLibrary(std::string_view name);

} // namespace cafe::hle
