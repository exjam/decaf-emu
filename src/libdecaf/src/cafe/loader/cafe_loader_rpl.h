#pragma once
#include <libcpu/be2_struct.h>

namespace cafe::loader::rpl
{

enum e_machine
{
   EM_PPC = 20 // PowerPC
};

enum e_encoding
{
   ELFDATANONE = 0,
   ELFDATA2LSB = 1,
   ELFDATA2MSB = 2
};

enum e_class
{
   ELFCLASSNONE = 0,
   ELFCLASS32 = 1,
   ELFCLASS64 = 2
};

enum e_elf_version
{
   EV_NONE = 0,
   EV_CURRENT = 1,
};

enum e_type
{
   ET_NONE        = 0,        // No file type
   ET_REL         = 1,        // Relocatable file
   ET_EXEC        = 2,        // Executable file
   ET_DYN         = 3,        // Shared object file
   ET_CORE        = 4,        // Core file
   ET_LOPROC      = 0xff00,   // Beginning of processor-specific codes
   ET_CAFE_RPL    = 0xff01,   // Cafe RPL file
   ET_HIPROC      = 0xffff,   // Processor-specific
};

enum e_abi
{
   EABI_CAFE      = 0xcafe   // WiiU CafeOS
};

enum sh_flags
{
   SHF_WRITE      = 0x1,
   SHF_ALLOC      = 0x2,
   SHF_EXECINSTR  = 0x4,
   SHF_TLS        = 0x04000000,
   SHF_DEFLATED   = 0x08000000,
   SHF_MASKPROC   = 0xF0000000,
};

enum sh_type
{
   SHT_NULL          = 0, // No associated section (inactive entry).
   SHT_PROGBITS      = 1, // Program-defined contents.
   SHT_SYMTAB        = 2, // Symbol table.
   SHT_STRTAB        = 3, // String table.
   SHT_RELA          = 4, // Relocation entries; explicit addends.
   SHT_HASH          = 5, // Symbol hash table.
   SHT_DYNAMIC       = 6, // Information for dynamic linking.
   SHT_NOTE          = 7, // Information about the file.
   SHT_NOBITS        = 8, // Data occupies no space in the file.
   SHT_REL           = 9, // Relocation entries; no explicit addends.
   SHT_SHLIB         = 10, // Reserved.
   SHT_DYNSYM        = 11, // Symbol table.
   SHT_INIT_ARRAY    = 14, // Pointers to initialization functions.
   SHT_FINI_ARRAY    = 15, // Pointers to termination functions.
   SHT_PREINIT_ARRAY = 16, // Pointers to pre-init functions.
   SHT_GROUP         = 17, // Section group.
   SHT_SYMTAB_SHNDX  = 18, // Indices for SHN_XINDEX entries.
   SHT_LOPROC        = 0x70000000, // Lowest processor arch-specific type.
   SHT_HIPROC        = 0x7fffffff, // Highest processor arch-specific type.
   SHT_LOUSER        = 0x80000000, // Lowest type reserved for applications.
   SHT_RPL_EXPORTS   = 0x80000001, // RPL Exports
   SHT_RPL_IMPORTS   = 0x80000002, // RPL Imports
   SHT_RPL_CRCS      = 0x80000003, // RPL CRCs
   SHT_RPL_FILEINFO  = 0x80000004, // RPL FileInfo
   SHT_HIUSER        = 0xffffffff, // Highest type reserved for applications.
};

enum st_binding
{
   STB_LOCAL         = 0, // Local symbol, not visible outside obj file containing def
   STB_GLOBAL        = 1, // Global symbol, visible to all object files being combined
   STB_WEAK          = 2, // Weak symbol, like global but lower-precedence
   STB_GNU_UNIQUE    = 10,
   STB_LOOS          = 10, // Lowest operating system-specific binding type
   STB_HIOS          = 12, // Highest operating system-specific binding type
   STB_LOPROC        = 13, // Lowest processor-specific binding type
   STB_HIPROC        = 15, // Highest processor-specific binding type
};

enum st_type
{
   STT_NOTYPE        = 0, // Symbol's type is not specified
   STT_OBJECT        = 1, // Symbol is a data object (variable, array, etc.)
   STT_FUNC          = 2, // Symbol is executable code (function, etc.)
   STT_SECTION       = 3, // Symbol refers to a section
   STT_FILE          = 4, // Local, absolute symbol that refers to a file
   STT_COMMON        = 5, // An uninitialized common block
   STT_TLS           = 6, // Thread local data object
   STT_LOOS          = 7, // Lowest operating system-specific symbol type
   STT_HIOS          = 8, // Highest operating system-specific symbol type
   STT_GNU_IFUNC     = 10, // GNU indirect function
   STT_LOPROC        = 13, // Lowest processor-specific symbol type
   STT_HIPROC        = 15, // Highest processor-specific symbol type
};

enum st_shndx
{
   SHN_UNDEF         = 0,      // Undefined
   SHN_LORESERVE     = 0xff00, // Reserved range
   SHN_HIPROC        = 0xff1f, // Reserved range
   SHN_ABS           = 0xfff1, // Absolute symbols
   SHN_COMMON        = 0xfff2, // Common symbols
   SHN_XINDEX        = 0xffff, // Escape -- index stored elsewhere
   SHN_HIRESERVE     = 0xffff,
};

enum RelocationType // r_info & 0xff
{
   R_PPC_NONE = 0,
   R_PPC_ADDR32 = 1,
   R_PPC_ADDR24 = 2,
   R_PPC_ADDR16 = 3,
   R_PPC_ADDR16_LO = 4,
   R_PPC_ADDR16_HI = 5,
   R_PPC_ADDR16_HA = 6,
   R_PPC_ADDR14 = 7,
   R_PPC_ADDR14_BRTAKEN = 8,
   R_PPC_ADDR14_BRNTAKEN = 9,
   R_PPC_REL24 = 10,
   R_PPC_REL14 = 11,
   R_PPC_REL14_BRTAKEN = 12,
   R_PPC_REL14_BRNTAKEN = 13,
   R_PPC_GOT16 = 14,
   R_PPC_GOT16_LO = 15,
   R_PPC_GOT16_HI = 16,
   R_PPC_GOT16_HA = 17,
   R_PPC_PLTREL24 = 18,
   R_PPC_JMP_SLOT = 21,
   R_PPC_RELATIVE = 22,
   R_PPC_LOCAL24PC = 23,
   R_PPC_REL32 = 26,
   R_PPC_TLS = 67,
   R_PPC_DTPMOD32 = 68,
   R_PPC_TPREL16 = 69,
   R_PPC_TPREL16_LO = 70,
   R_PPC_TPREL16_HI = 71,
   R_PPC_TPREL16_HA = 72,
   R_PPC_TPREL32 = 73,
   R_PPC_DTPREL16 = 74,
   R_PPC_DTPREL16_LO = 75,
   R_PPC_DTPREL16_HI = 76,
   R_PPC_DTPREL16_HA = 77,
   R_PPC_DTPREL32 = 78,
   R_PPC_GOT_TLSGD16 = 79,
   R_PPC_GOT_TLSGD16_LO = 80,
   R_PPC_GOT_TLSGD16_HI = 81,
   R_PPC_GOT_TLSGD16_HA = 82,
   R_PPC_GOT_TLSLD16 = 83,
   R_PPC_GOT_TLSLD16_LO = 84,
   R_PPC_GOT_TLSLD16_HI = 85,
   R_PPC_GOT_TLSLD16_HA = 86,
   R_PPC_GOT_TPREL16 = 87,
   R_PPC_GOT_TPREL16_LO = 88,
   R_PPC_GOT_TPREL16_HI = 89,
   R_PPC_GOT_TPREL16_HA = 90,
   R_PPC_GOT_DTPREL16 = 91,
   R_PPC_GOT_DTPREL16_LO = 92,
   R_PPC_GOT_DTPREL16_HI = 93,
   R_PPC_GOT_DTPREL16_HA = 94,
   R_PPC_TLSGD = 95,
   R_PPC_TLSLD = 96,
   R_PPC_EMB_SDA21 = 109,
   R_PPC_REL16 = 249,
   R_PPC_REL16_LO = 250,
   R_PPC_REL16_HI = 251,
   R_PPC_REL16_HA = 252,
};

#pragma pack(push, 1)

struct Header
{
   static const unsigned Magic = 0x7F454C46;

   //! File identification.
   be2_val<uint32_t> magic;

   //! File class.
   be2_val<uint8_t> fileClass;

   //! Data encoding.
   be2_val<uint8_t> encoding;

   //! File version.
   be2_val<uint8_t> elfVersion;

   //! OS/ABI identification. (EABI_*)
   be2_val<uint16_t> abi;

   PADDING(7);

   //! Type of file (ET_*)
   be2_val<uint16_t> type;

   //! Required architecture for this file (EM_*)
   be2_val<uint16_t> machine;

   //! Must be equal to 1
   be2_val<uint32_t> version;

   //! Address to jump to in order to start program
   be2_val<uint32_t> entry;

   //! Program header table's file offset, in bytes
   be2_val<uint32_t> phoff;

   //! Section header table's file offset, in bytes
   be2_val<uint32_t> shoff;

   //! Processor-specific flags
   be2_val<uint32_t> flags;

   //! Size of ELF header, in bytes
   be2_val<uint16_t> ehsize;

   //! Size of an entry in the program header table
   be2_val<uint16_t> phentsize;

   //! Number of entries in the program header table
   be2_val<uint16_t> phnum;

   //! Size of an entry in the section header table
   be2_val<uint16_t> shentsize;

   //! Number of entries in the section header table
   be2_val<uint16_t> shnum;

   //! Sect hdr table index of sect name string table
   be2_val<uint16_t> shstrndx;
};
CHECK_OFFSET(Header, 0x00, magic);
CHECK_OFFSET(Header, 0x04, fileClass);
CHECK_OFFSET(Header, 0x05, encoding);
CHECK_OFFSET(Header, 0x06, elfVersion);
CHECK_OFFSET(Header, 0x07, abi);
CHECK_OFFSET(Header, 0x10, type);
CHECK_OFFSET(Header, 0x12, machine);
CHECK_OFFSET(Header, 0x14, version);
CHECK_OFFSET(Header, 0x18, entry);
CHECK_OFFSET(Header, 0x1C, phoff);
CHECK_OFFSET(Header, 0x20, shoff);
CHECK_OFFSET(Header, 0x24, flags);
CHECK_OFFSET(Header, 0x28, ehsize);
CHECK_OFFSET(Header, 0x2A, phentsize);
CHECK_OFFSET(Header, 0x2C, phnum);
CHECK_OFFSET(Header, 0x2E, shentsize);
CHECK_OFFSET(Header, 0x30, shnum);
CHECK_OFFSET(Header, 0x32, shstrndx);
CHECK_SIZE(Header, 0x34);

struct ProgramHeader
{
   be2_val<uint32_t> type;
   be2_val<uint32_t> offset;
   be2_val<uint32_t> vaddr;
   be2_val<uint32_t> paddr;
   be2_val<uint32_t> filesz;
   be2_val<uint32_t> memsz;
   be2_val<uint32_t> flags;
   be2_val<uint32_t> align;
};
CHECK_SIZE(ProgramHeader, 0x20);

struct SectionHeader
{
   //! Section name (index into string table)
   be2_val<uint32_t> name;

   //! Section type (SHT_*)
   be2_val<uint32_t> type;

   //! Section flags (SHF_*)
   be2_val<uint32_t> flags;

   //! Address where section is to be loaded
   be2_val<uint32_t> addr;

   //! File offset of section data, in bytes
   be2_val<uint32_t> offset;

   //! Size of section, in bytes
   be2_val<uint32_t> size;

   //! Section type-specific header table index link
   be2_val<uint32_t> link;

   //! Section type-specific extra information
   be2_val<uint32_t> info;

   //! Section address alignment
   be2_val<uint32_t> addralign;

   //! Size of records contained within the section
   be2_val<uint32_t> entsize;
};
CHECK_OFFSET(SectionHeader, 0x00, name);
CHECK_OFFSET(SectionHeader, 0x04, type);
CHECK_OFFSET(SectionHeader, 0x08, flags);
CHECK_OFFSET(SectionHeader, 0x0C, addr);
CHECK_OFFSET(SectionHeader, 0x10, offset);
CHECK_OFFSET(SectionHeader, 0x14, size);
CHECK_OFFSET(SectionHeader, 0x18, link);
CHECK_OFFSET(SectionHeader, 0x1C, info);
CHECK_OFFSET(SectionHeader, 0x20, addralign);
CHECK_OFFSET(SectionHeader, 0x24, entsize);
CHECK_SIZE(SectionHeader, 0x28);

struct Symbol
{
   //! Symbol name (index into string table)
   be2_val<uint32_t> name;

   //! Value or address associated with the symbol
   be2_val<uint32_t> value;

   //! Size of the symbol
   be2_val<uint32_t> size;

   //! Symbol's type and binding attributes
   be2_val<uint8_t> info;

   //! Must be zero; reserved
   be2_val<uint8_t> other;

   //! Which section (header table index) it's defined in (SHN_*)
   be2_val<uint16_t> shndx;
};
CHECK_OFFSET(Symbol, 0x00, name);
CHECK_OFFSET(Symbol, 0x04, value);
CHECK_OFFSET(Symbol, 0x08, size);
CHECK_OFFSET(Symbol, 0x0C, info);
CHECK_OFFSET(Symbol, 0x0D, other);
CHECK_OFFSET(Symbol, 0x0E, shndx);
CHECK_SIZE(Symbol, 0x10);

struct Rela
{
   be2_val<uint32_t> offset;
   be2_val<uint32_t> info;
   be2_val<int32_t> addend;
};
CHECK_OFFSET(Rela, 0x00, offset);
CHECK_OFFSET(Rela, 0x04, info);
CHECK_OFFSET(Rela, 0x08, addend);
CHECK_SIZE(Rela, 0x0C);

struct DeflatedHeader
{
   be2_val<uint32_t> inflatedSize;
};
CHECK_OFFSET(DeflatedHeader, 0x00, inflatedSize);
CHECK_SIZE(DeflatedHeader, 0x04);

struct RPLFileInfo_v3_0
{
   static constexpr auto Version = 0xCAFE0300u;
   be2_val<uint32_t> version;
   be2_val<uint32_t> textSize;
   be2_val<uint32_t> textAlign;
   be2_val<uint32_t> dataSize;
   be2_val<uint32_t> dataAlign;
   be2_val<uint32_t> loadSize;
   be2_val<uint32_t> loadAlign;
   be2_val<uint32_t> tempSize;
   be2_val<uint32_t> trampAdjust;
   be2_val<uint32_t> sdaBase;
   be2_val<uint32_t> sda2Base;
   be2_val<uint32_t> stackSize;
   be2_val<uint32_t> filename;
   be2_array<uint32_t, 3> reserved;
};
CHECK_OFFSET(RPLFileInfo_v3_0, 0x00, version);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x04, textSize);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x08, textAlign);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x0C, dataSize);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x10, dataAlign);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x14, loadSize);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x18, loadAlign);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x1C, tempSize);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x20, trampAdjust);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x24, sdaBase);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x28, sda2Base);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x2C, stackSize);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x30, filename);
CHECK_OFFSET(RPLFileInfo_v3_0, 0x34, reserved);
CHECK_SIZE(RPLFileInfo_v3_0, 0x40);

struct RPLFileInfo_v4_1
{
   static constexpr auto Version = 0xCAFE0401u;
   be2_val<uint32_t> version;
   be2_val<uint32_t> textSize;
   be2_val<uint32_t> textAlign;
   be2_val<uint32_t> dataSize;
   be2_val<uint32_t> dataAlign;
   be2_val<uint32_t> loadSize;
   be2_val<uint32_t> loadAlign;
   be2_val<uint32_t> tempSize;
   be2_val<uint32_t> trampAdjust;
   be2_val<uint32_t> sdaBase;
   be2_val<uint32_t> sda2Base;
   be2_val<uint32_t> stackSize;
   be2_val<uint32_t> filename;
   be2_val<uint32_t> flags;
   be2_val<uint32_t> heapSize;
   be2_val<uint32_t> tagOffset;
};
CHECK_OFFSET(RPLFileInfo_v4_1, 0x00, version);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x04, textSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x08, textAlign);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x0C, dataSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x10, dataAlign);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x14, loadSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x18, loadAlign);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x1C, tempSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x20, trampAdjust);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x24, sdaBase);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x28, sda2Base);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x2C, stackSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x30, filename);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x34, flags);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x38, heapSize);
CHECK_OFFSET(RPLFileInfo_v4_1, 0x3C, tagOffset);
CHECK_SIZE(RPLFileInfo_v4_1, 0x40);

struct RPLFileInfo_v4_2
{
   static constexpr auto Version = 0xCAFE0402u;
   be2_val<uint32_t> version;
   be2_val<uint32_t> textSize;
   be2_val<uint32_t> textAlign;
   be2_val<uint32_t> dataSize;
   be2_val<uint32_t> dataAlign;
   be2_val<uint32_t> loadSize;
   be2_val<uint32_t> loadAlign;
   be2_val<uint32_t> tempSize;
   be2_val<uint32_t> trampAdjust;
   be2_val<uint32_t> sdaBase;
   be2_val<uint32_t> sda2Base;
   be2_val<uint32_t> stackSize;
   be2_val<uint32_t> filename;
   be2_val<uint32_t> flags;
   be2_val<uint32_t> heapSize;
   be2_val<uint32_t> tagOffset;
   be2_val<uint32_t> minVersion;
   be2_val<int32_t> compressionLevel;
   be2_val<uint32_t> trampAddition;
   be2_val<uint32_t> fileInfoPad;
   be2_val<uint32_t> cafeSdkVersion;
   be2_val<uint32_t> cafeSdkRevision;
   be2_val<int16_t> tlsModuleIndex;
   be2_val<uint16_t> tlsAlignShift;
   be2_val<uint32_t> runtimeFileInfoSize;
};
CHECK_OFFSET(RPLFileInfo_v4_2, 0x00, version);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x04, textSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x08, textAlign);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x0C, dataSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x10, dataAlign);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x14, loadSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x18, loadAlign);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x1C, tempSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x20, trampAdjust);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x24, sdaBase);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x28, sda2Base);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x2C, stackSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x30, filename);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x34, flags);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x38, heapSize);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x3C, tagOffset);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x40, minVersion);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x44, compressionLevel);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x48, trampAddition);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x4C, fileInfoPad);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x50, cafeSdkVersion);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x54, cafeSdkRevision);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x58, tlsModuleIndex);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x5A, tlsAlignShift);
CHECK_OFFSET(RPLFileInfo_v4_2, 0x5C, runtimeFileInfoSize);
CHECK_SIZE(RPLFileInfo_v4_2, 0x60);

#pragma pack(pop)

} // namespace cafe::loader::rpl
