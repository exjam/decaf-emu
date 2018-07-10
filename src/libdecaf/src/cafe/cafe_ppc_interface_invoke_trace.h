#pragma once
#include "cafe_ppc_interface_invoke.h"

#include <fmt/format.h>

namespace cafe
{

namespace detail
{
template<typename HostFunctionType, typename FunctionTraitsType, std::size_t... I>
constexpr void
invoke_trace_host_impl(cpu::Core *core,
                       HostFunctionType &&,
                       const char *name,
                       FunctionTraitsType &&,
                       std::index_sequence<I...>)
{
   auto param_info = typename FunctionTraitsType::param_info { };

   if constexpr (FunctionTraitsType::has_return_value) {
      auto return_info = typename FunctionTraitsType::return_info { };

      if constexpr (FunctionTraitsType::is_member_function) {
         auto obj = readParam(core, typename FunctionTraitsType::object_info { });
         readParam(core, std::get<I>(param_info))...;
      } else {
         writeParam(core,
                    return_info,
                    func(readParam(core, std::get<I>(param_info))...));
      }
   } else {
      if constexpr (FunctionTraitsType::is_member_function) {
         auto obj = readParam(core, typename FunctionTraitsType::object_info { });
         (obj.getRawPointer()->*func)(readParam(core, std::get<I>(param_info))...);
      } else {
         func(readParam(core, std::get<I>(param_info))...);
      }
   }
}

} // namespace detail

//! Trace log a host function call from a guest context
template<typename FunctionType>
void
invoke_trace(cpu::Core *core,
             FunctionType fn,
             const char *name)
{
   using func_traits = detail::function_traits<FunctionType>;
   invoke_trace_host_impl(core,
                          fn, name,
                          func_traits { },
                          std::make_index_sequence<func_traits::num_args> {});
}

} // namespace cafe
