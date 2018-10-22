#include "ios_acp_nn_miscservice.h"

#include "nn/acp/nn_acp_result.h"

using namespace nn::acp;

namespace ios::acp::internal
{

namespace detail
{

template<int, int, typename... Types>
struct ServerIpcDataHelper;

template<int ServiceId, int CommandId, typename... ParameterTypes, typename... ResponseTypes>
struct ServerIpcDataHelper<ServiceId, CommandId, std::tuple<ParameterTypes...>, std::tuple<ResponseTypes...>>
{
   ServerIpcDataHelper(const nn::CommandHandlerArgs &args)
   {
      mResponseBuffer = args.responseBuffer;
   }

   template<typename Type>
   void WriteResponseValue(size_t &offset, const Type &value)
   {
      auto ptr = phys_cast<Type *>(phys_cast<phys_addr>(mResponseBuffer) + offset);
      *ptr = value;
      offset += sizeof(Type);
   }

   void WriteResponse(const ResponseTypes &... responses)
   {
      // TODO: Handle managed buffers

      // Write response values
      auto offset = size_t { 0 };
      (WriteResponseValue(offset, responses), ...);
   }

   phys_ptr<void> mResponseBuffer;
};

} // namespace detail

template<typename CommandType>
struct ServerIpcData;

template<typename CommandType>
struct ServerIpcData : detail::ServerIpcDataHelper<CommandType::service, CommandType::command, typename CommandType::parameters, typename CommandType::response>
{
   ServerIpcData(const nn::CommandHandlerArgs &args) :
      detail::ServerIpcDataHelper<CommandType::service, CommandType::command, typename CommandType::parameters, typename CommandType::response>(args)
   {
   }
};

nn::Result
getNetworkTime(nn::CommandHandlerArgs &args)
{
   auto command = ServerIpcData<MiscService::GetNetworkTime> { args };
   command.WriteResponse(946684800000001ull, 12321);
   return ResultSuccess;
}

nn::Result
MiscService::commandHandler(uint32_t unk1,
                            nn::CommandId command,
                            nn::CommandHandlerArgs &args)
{
   switch (command) {
   case GetNetworkTime::command:
      return getNetworkTime(args);
   default:
      return ResultSuccess;
   }
}

} // namespace ios::acp::internal
