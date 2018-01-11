#pragma once
#include "cafe_hle_library_symbol.h"
#include "cafe/cafe_ppc_interface_invoke.h"
#include <memory>

namespace cafe::hle
{

struct LibraryFunction : LibrarySymbol
{
   LibraryFunction() :
      LibrarySymbol(LibrarySymbol::Function)
   {
   }

   virtual void call(cpu::Core *state) = 0;
   bool traceEnabled = true;
};

namespace internal
{

/**
 * Handles call to both global functions and member functions.
 */
template<typename FunctionType>
struct LibraryFunctionCall : LibraryFunction
{
   FunctionType func;

   virtual void call(cpu::Core *state) override
   {
      invoke(state, func);
   }
};

/**
 * Handles calls to constructors.
 */
template<typename ObjectType, typename... ArgTypes>
struct LibraryConstructorFunction : LibraryFunction
{
   static void wrapper(virt_ptr<ObjectType> obj, ArgTypes... args)
   {
      ::new(static_cast<void *>(obj.getRawPointer())) ObjectType(args...);
   }

   virtual void call(cpu::Core *state) override
   {
      invoke(state, &LibraryConstructorFunction::wrapper);
   }
};

/**
 * Handles calls to destructors.
 */
template<typename ObjectType>
struct LibraryDestructorFunction : LibraryFunction
{
   static void wrapper(virt_ptr<ObjectType> obj)
   {
      (obj.getRawPointer())->~ObjectType();
   }

   virtual void call(cpu::Core *state) override
   {
      invoke(state, &LibraryDestructorFunction::wrapper);
   }
};

template<typename FunctionType>
inline std::unique_ptr<LibraryFunction>
makeLibraryFunction(FunctionType func)
{
   auto libraryFunction = new LibraryFunctionCall<FunctionType>();
   libraryFunction->func = func;
   return std::unique_ptr<LibraryFunction> { reinterpret_cast<LibraryFunction *>(libraryFunction) };
}

template<typename ObjectType, typename... ArgTypes>
inline std::unique_ptr<LibraryFunction>
makeLibraryConstructorFunction()
{
   auto libraryFunction = new LibraryConstructorFunction<ObjectType, ArgTypes...>();
   return std::unique_ptr<LibraryFunction> { reinterpret_cast<LibraryFunction *>(libraryFunction) };
}

template<typename ObjectType>
inline std::unique_ptr<LibraryFunction>
makeLibraryDestructorFunction()
{
   auto libraryFunction = new LibraryDestructorFunction<ObjectType>();
   return std::unique_ptr<LibraryFunction> { reinterpret_cast<LibraryFunction *>(libraryFunction) };
}

} // namespace internal

} // cafe::hle
