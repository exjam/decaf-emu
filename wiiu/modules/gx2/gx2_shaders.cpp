#include "gx2.h"
#include "gx2_shaders.h"

uint32_t
GX2CalcGeometryShaderInputRingBufferSize(uint32_t ringItemSize)
{
   // From gx2.rpl but its likely this is custom to our implementation
   return ringItemSize << 12;
}

uint32_t
GX2CalcGeometryShaderOutputRingBufferSize(uint32_t ringItemSize)
{
   // From gx2.rpl but its likely this is custom to our implementation
   return ringItemSize << 14;
}

uint32_t
GX2CalcFetchShaderSizeEx(uint32_t attribs, FetchShaderType fetchShaderType, TessellationMode tessellationMode)
{
   // TODO: Do we need to reverse the real algo? Or does this depend on our custom impl
   return attribs * 4 + 32;
}

void
GX2::registerShaderFunctions()
{
   RegisterSystemFunction(GX2CalcGeometryShaderInputRingBufferSize);
   RegisterSystemFunction(GX2CalcGeometryShaderOutputRingBufferSize);
   RegisterSystemFunction(GX2CalcFetchShaderSizeEx);
}
