#pragma once
#include "latte_enum_cb.h"
#include "latte_enum_common.h"

#include <cstdint>
#include <common/bitfield.h>

namespace latte
{

// Blend function used for all render targets
BITFIELD(CB_BLENDN_CONTROL, uint32_t)
   BITFIELD_ENTRY(0, 5, CB_BLEND_FUNC, COLOR_SRCBLEND)
   BITFIELD_ENTRY(5, 3, CB_COMB_FUNC, COLOR_COMB_FCN)
   BITFIELD_ENTRY(8, 5, CB_BLEND_FUNC, COLOR_DESTBLEND)
   BITFIELD_ENTRY(13, 1, uint32_t, OPACITY_WEIGHT)
   BITFIELD_ENTRY(16, 5, CB_BLEND_FUNC, ALPHA_SRCBLEND)
   BITFIELD_ENTRY(21, 3, CB_COMB_FUNC, ALPHA_COMB_FCN)
   BITFIELD_ENTRY(24, 5, CB_BLEND_FUNC, ALPHA_DESTBLEND)
   BITFIELD_ENTRY(29, 1, bool, SEPARATE_ALPHA_BLEND)
BITFIELD_END

BITFIELD(CB_BLEND_RED, uint32_t)
   BITFIELD_ENTRY(0, 32, float, BLEND_RED)
BITFIELD_END

BITFIELD(CB_BLEND_GREEN, uint32_t)
   BITFIELD_ENTRY(0, 32, float, BLEND_GREEN)
BITFIELD_END

BITFIELD(CB_BLEND_BLUE, uint32_t)
   BITFIELD_ENTRY(0, 32, float, BLEND_BLUE)
BITFIELD_END

BITFIELD(CB_BLEND_ALPHA, uint32_t)
   BITFIELD_ENTRY(0, 32, float, BLEND_ALPHA)
BITFIELD_END

// This register controls color keying, which masks individual pixel writes based on comparing the
// source(pre - ROP) color and / or the dest(frame buffer) color to comparison values, after masking both by CLRCMP_MSK
BITFIELD(CB_CLRCMP_CONTROL, uint32_t)
   BITFIELD_ENTRY(0, 3, CB_CLRCMP_DRAW, CLRCMP_FCN_SRC)
   BITFIELD_ENTRY(8, 3, CB_CLRCMP_DRAW, CLRCMP_FCN_DST)
   BITFIELD_ENTRY(24, 2, CB_CLRCMP_SEL, CLRCMP_FCN_SEL)
BITFIELD_END

BITFIELD(CB_CLRCMP_SRC, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, CLRCMP_SRC)
BITFIELD_END

BITFIELD(CB_CLRCMP_DST, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, CLRCMP_DST)
BITFIELD_END

BITFIELD(CB_CLRCMP_MSK, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, CLRCMP_MSK)
BITFIELD_END

BITFIELD(CB_COLORN_BASE, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, BASE_256B)
BITFIELD_END

BITFIELD(CB_COLORN_TILE, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, BASE_256B)
BITFIELD_END

BITFIELD(CB_COLORN_FRAG, uint32_t)
   BITFIELD_ENTRY(0, 32, uint32_t, BASE_256B)
BITFIELD_END

BITFIELD(CB_COLORN_SIZE, uint32_t)
   BITFIELD_ENTRY(0, 10, uint32_t, PITCH_TILE_MAX)
   BITFIELD_ENTRY(10, 20, uint32_t, SLICE_TILE_MAX)
BITFIELD_END

BITFIELD(CB_COLORN_INFO, uint32_t)
   BITFIELD_ENTRY(0, 2, CB_ENDIAN, ENDIAN)
   BITFIELD_ENTRY(2, 6, CB_FORMAT, FORMAT)
   BITFIELD_ENTRY(8, 4, BUFFER_ARRAY_MODE, ARRAY_MODE)
   BITFIELD_ENTRY(12, 3, CB_NUMBER_TYPE, NUMBER_TYPE)
   BITFIELD_ENTRY(15, 1, BUFFER_READ_SIZE, READ_SIZE)
   BITFIELD_ENTRY(16, 2, CB_COMP_SWAP, COMP_SWAP)
   BITFIELD_ENTRY(18, 2, CB_TILE_MODE, TILE_MODE)
   BITFIELD_ENTRY(20, 1, bool, BLEND_CLAMP)
   BITFIELD_ENTRY(21, 1, bool, CLEAR_COLOR)
   BITFIELD_ENTRY(22, 1, bool, BLEND_BYPASS)
   BITFIELD_ENTRY(23, 1, bool, BLEND_FLOAT32)
   BITFIELD_ENTRY(24, 1, bool, SIMPLE_FLOAT)
   BITFIELD_ENTRY(25, 1, CB_ROUND_MODE, ROUND_MODE)
   BITFIELD_ENTRY(26, 1, bool, TILE_COMPACT)
   BITFIELD_ENTRY(27, 1, CB_SOURCE_FORMAT, SOURCE_FORMAT)
BITFIELD_END

// Selects slice index range for render target
BITFIELD(CB_COLORN_VIEW, uint32_t)
   BITFIELD_ENTRY(0, 11, uint32_t, SLICE_START)
   BITFIELD_ENTRY(13, 11, uint32_t, SLICE_MAX)
BITFIELD_END

BITFIELD(CB_COLORN_MASK, uint32_t)
   BITFIELD_ENTRY(0, 12, uint32_t, CMASK_BLOCK_MAX)
   BITFIELD_ENTRY(12, 20, uint32_t, FMASK_TILE_MAX)
BITFIELD_END

BITFIELD(CB_COLOR_CONTROL, uint32_t)
   BITFIELD_ENTRY(0, 1, bool, FOG_ENABLE)
   BITFIELD_ENTRY(1, 1, bool, MULTIWRITE_ENABLE)
   BITFIELD_ENTRY(2, 1, bool, DITHER_ENABLE)
   BITFIELD_ENTRY(3, 1, bool, DEGAMMA_ENABLE)
   BITFIELD_ENTRY(4, 3, CB_SPECIAL_OP, SPECIAL_OP)
   BITFIELD_ENTRY(7, 1, bool, PER_MRT_BLEND)
   BITFIELD_ENTRY(8, 8, uint8_t, TARGET_BLEND_ENABLE)
   BITFIELD_ENTRY(16, 8, uint8_t, ROP3)
BITFIELD_END

BITFIELD(CB_SHADER_CONTROL, uint32_t)
   BITFIELD_ENTRY(0, 1, bool, RT0_ENABLE)
   BITFIELD_ENTRY(1, 1, bool, RT1_ENABLE)
   BITFIELD_ENTRY(2, 1, bool, RT2_ENABLE)
   BITFIELD_ENTRY(3, 1, bool, RT3_ENABLE)
   BITFIELD_ENTRY(4, 1, bool, RT4_ENABLE)
   BITFIELD_ENTRY(5, 1, bool, RT5_ENABLE)
   BITFIELD_ENTRY(6, 1, bool, RT6_ENABLE)
   BITFIELD_ENTRY(7, 1, bool, RT7_ENABLE)
BITFIELD_END

// Contains color component mask fields for the colors output by the shader
BITFIELD(CB_SHADER_MASK, uint32_t)
   BITFIELD_ENTRY(0, 4, uint32_t, OUTPUT0_ENABLE)
   BITFIELD_ENTRY(4, 4, uint32_t, OUTPUT1_ENABLE)
   BITFIELD_ENTRY(8, 4, uint32_t, OUTPUT2_ENABLE)
   BITFIELD_ENTRY(12, 4, uint32_t, OUTPUT3_ENABLE)
   BITFIELD_ENTRY(16, 4, uint32_t, OUTPUT4_ENABLE)
   BITFIELD_ENTRY(20, 4, uint32_t, OUTPUT5_ENABLE)
   BITFIELD_ENTRY(24, 4, uint32_t, OUTPUT6_ENABLE)
   BITFIELD_ENTRY(28, 4, uint32_t, OUTPUT7_ENABLE)
BITFIELD_END

// Contains color component mask fields for writing the render targets. Red, green, blue, and alpha
// are components 0, 1, 2, and 3 in the pixel shader and are enabled by bits 0, 1, 2, and 3 in each field
BITFIELD(CB_TARGET_MASK, uint32_t)
   BITFIELD_ENTRY(0, 4, uint32_t, TARGET0_ENABLE)
   BITFIELD_ENTRY(4, 4, uint32_t, TARGET1_ENABLE)
   BITFIELD_ENTRY(8, 4, uint32_t, TARGET2_ENABLE)
   BITFIELD_ENTRY(12, 4, uint32_t, TARGET3_ENABLE)
   BITFIELD_ENTRY(16, 4, uint32_t, TARGET4_ENABLE)
   BITFIELD_ENTRY(20, 4, uint32_t, TARGET5_ENABLE)
   BITFIELD_ENTRY(24, 4, uint32_t, TARGET6_ENABLE)
   BITFIELD_ENTRY(28, 4, uint32_t, TARGET7_ENABLE)
BITFIELD_END

} // namespace latte
