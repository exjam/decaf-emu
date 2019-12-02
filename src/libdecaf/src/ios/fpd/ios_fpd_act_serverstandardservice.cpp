#include "ios_fpd_act_accountdata.h"
#include "ios_fpd_act_server.h"
#include "ios_fpd_act_serverstandardservice.h"

#include "ios/ios_stackobject.h"
#include "ios/auxil/ios_auxil_usr_cfg_ipc.h"
#include "ios/nn/ios_nn_ipc_server_command.h"
#include "nn/act/nn_act_result.h"

#include <curl/curl.h>
#include <fmt/format.h>

using namespace nn::ipc;
using namespace nn::act;

namespace ios::fpd::internal
{

static void
setStandardCurlOptions(CURL *easy)
{
   curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1);
   curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 2);
   curl_easy_setopt(easy, CURLOPT_VERBOSE, 0);
   curl_easy_setopt(easy, CURLOPT_TIMEOUT, 60);

   /*
   curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, (int)sub_E30DC3F4);
   curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, (int)sub_E30DC554);
   curl_easy_setopt(easy, CURLOPT_WRITEHEADER, (int)a1);
   curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, (int)sub_E30DC4B0);
   curl_easy_setopt(easy, CURLOPT_FILE, (int)a1);
   curl_easy_setopt(easy, CURLOPT_PROGRESSFUNCTION, (int)sub_E30DC3FC);
   curl_easy_setopt(easy, CURLOPT_PROGRESSDATA, (int)a1);
   curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, (int)a1->field_202D);
   curl_easy_setopt(easy, 211u, 3);
   curl_easy_setopt(easy, 210u, a1->field_28);
   */
}
static curl_slist *
addNintendoHttpHeaders(curl_slist *headers)
{
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Platform-ID: {}", 1u).c_str());

   // Device Type comes from /dev/crypto ioctl 0x1C
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Device-Type: {}", 2u).c_str());

   // Comes from MCP_GetDeviceID (cached)
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Device-ID: {}", 2u).c_str());

   // Comes from MCP 0x40 (cached
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Serial-Number: {}", "serial").c_str());

   // MCP region number
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Region: {}", 4u).c_str());

   // String read from country.txt, using mcp country id
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Country: {}", "UK").c_str());

   // String read from language.txt, using mcp language id
   headers = curl_slist_append(headers, fmt::format("Accept-Language: {}", "EN").c_str());

   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Client-ID: {}", "a2efa818a34fa16b8afbc8a74eba3eda").c_str());
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Client-Secret: {}", "c91cdb5658bd4954ade78533a339cf9a").c_str());

   headers = curl_slist_append(headers, fmt::format("Accept: {}", "*/*").c_str());

   headers = curl_slist_append(headers, fmt::format("X-Nintendo-FPD-Version: {:04X}", 0u).c_str());

   // TODO: Lx/Dx/Sx/Tx/Jx (default: L1)
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Environment: {}", "L1").c_str());

   // TODO: Get title id
   auto titleId = uint64_t { 0 };
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Title-ID: {:016X}", titleId).c_str());

   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Unique-ID: {:05X}", (titleId & 0x0FFFFF00u) >> 8).c_str());

   auto titleVersion = uint32_t { 0 };
   headers = curl_slist_append(headers, fmt::format("X-Nintendo-Application-Version: {:04X}", titleVersion).c_str());

   return headers;
}

static nn::Result
acquireNexServiceToken(CommandHandlerArgs &args)
{
   auto command =
      ServerCommand<ActServerStandardService::AcquireNexServiceToken>{ args };
   auto slot = InvalidSlot;
   auto nexAuthenticationResult = OutBuffer<NexAuthenticationResult>{ };
   auto gameId = uint32_t{ 0 };
   auto parentalControlCheckEnabled = false;
   command.ReadRequest(slot, nexAuthenticationResult, gameId,
      parentalControlCheckEnabled);

   if (slot != CurrentUserSlot) {
      return ResultInvalidValue;
   }

   if (parentalControlCheckEnabled) {
      StackObject<uint8_t> net_communication_on_game;
      StackObject<auxil::UCSysConfig> settings;
      settings[0].access = 0x777u;
      settings[0].dataType = auxil::UCDataType::UnsignedByte;
      settings[0].dataSize = 1u;
      settings[0].data = phys_cast<void *>(net_communication_on_game);
      std::snprintf(
         phys_addrof(settings[0].name).getRawPointer(),
         settings[0].name.size(),
         "p_acct%u.net_communication_on_game",
         getSlotNoForAccount(getCurrentAccount()));

      auto result = auxil::UCReadSysConfig(getActUserConfigHandle(), 1, settings);
      if (!result) {
         return ResultUcError;
      }

      if (!*net_communication_on_game) {
         return ResultON_GAME_INTERNET_COMMUNICATION_RESTRICTED;
      }
   }

   // TODO: IOS Synchronous network request (but async at C++ level)
   // https://github.com/Kinnay/NintendoClients/wiki/Account-Server#get-providernex_tokenme

   curl_slist *headers = addNintendoHttpHeaders(nullptr);

   fmt::format("https://account.nintendo.net/v1/api/provider/nex_token/@me?game_server_id={:08X}", gameId);

   auto easy = curl_easy_init();
   auto multi = curl_multi_init();

   curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);

   curl_multi_add_handle(multi, easy);

   curl_easy_cleanup(easy);
   curl_multi_cleanup(multi);
   return nn::ResultSuccess;
}


static nn::Result
cancel(CommandHandlerArgs &args)
{
   auto command = ServerCommand<ActServerStandardService::Cancel> { args };
   return nn::ResultSuccess;
}

nn::Result
ActServerStandardService::commandHandler(uint32_t unk1,
                                         CommandId command,
                                         CommandHandlerArgs &args)
{
   switch (command) {
   case Cancel::command:
      return cancel(args);
   case AcquireNexServiceToken::command:
      return acquireNexServiceToken(args);
   default:
      return nn::ResultSuccess;
   }
}

} // namespace ios::fpd::internal
