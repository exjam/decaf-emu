#pragma once

namespace cafe
{

enum class LibraryId
{
   avm,
   camera,
   coreinit,
   dmae,
   erreula,
   gx2,
   mic,
   nn_ac,
   nn_acp,
   nn_act,
   nn_aoc,
   nn_boss,
   nn_cmpt,
   nn_fp,
   nn_ndm,
   nn_nfp,
   nn_olv,
   nn_save,
   nn_temp,
   nsyskbd,
   nsysnet,
   padscore,
   proc_ui,
   sci,
   snd_core,
   snd_user,
   swkbd,
   sysapp,
   tcl,
   vpad,
   zlib125
};

class Library
{
public:
   Library(LibraryId id) :
      mID(id)
   {
   }

private:
   LibraryId mID;
};

} // namespace cafe
