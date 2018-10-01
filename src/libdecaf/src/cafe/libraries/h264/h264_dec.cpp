#pragma optimize("", off)
#include "h264.h"
#include "h264_dec.h"

#include "cafe/cafe_stackobject.h"
#include "cafe/libraries/cafe_hle_stub.h"

#include <array>
#include <common/align.h>
#include <common/decaf_assert.h>
#include <fmt/format.h>

#ifdef  DECAF_FFMPEG
extern "C"
{
   #include <libavcodec/avcodec.h>
   #include <libavformat/avformat.h>
   #include <libswscale/swscale.h>
}
#endif

namespace cafe::h264
{

#if 0
   static AVCodecContext* context;
   static AVFrame* frame;
   static AVPacket avpkt;
#endif

int32_t
H264DECCheckMemSegmentation(virt_ptr<void> memory,
	                        uint32_t size)
{
   if (!memory || !size) {
      return H264Error::InvalidParameter;
   }

   /*
   if (!UVDCheckSegmentViolation())
      return -1;
   */

   decaf_warn_stub();
   return H264Error::OK;
}


constexpr auto MinimumWidth = 64u;
constexpr auto MinimumHeight = 64u;
constexpr auto MaximumWidth = 2800u;
constexpr auto MaximumHeight = 1408u;

constexpr auto BaseMemoryRequirement = 0x480000u;

static uint32_t
getLevelMemoryRequirement(int32_t level)
{
   switch (level) {
   case 10:
      return 0x63000;
   case 11:
      return 0xE1000;
   case 12:
   case 13:
   case 20:
      return 0x252000;
   case 21:
      return 0x4A4000;
   case 22:
   case 30:
      return 0x7E9000;
   case 31:
      return 0x1194000;
   case 32:
      return 0x1400000;
   case 40:
   case 41:
      return 0x2000000;
   case 42:
      return 0x2200000;
   case 50:
      return 0x6BD0000;
   case 51:
      return 0xB400000;
   default:
      decaf_abort(fmt::format("Unexpected H264 level {}", level));
   }
}

H264Error
H264DECMemoryRequirement(int32_t profile,
                         int32_t level,
                         int32_t maxWidth,
                         int32_t maxHeight,
                         virt_ptr<uint32_t> outMemoryRequirement)
{
   if (maxWidth < MinimumWidth || maxHeight < MinimumHeight ||
       maxWidth > MaximumWidth || maxHeight > MaximumHeight) {
      return H264Error::InvalidParameter;
   }

   if (!outMemoryRequirement) {
      return H264Error::InvalidParameter;
   }

   if (level > 51) {
      return H264Error::InvalidParameter;
   }

   if (profile != 66 && profile != 77 && profile != 100) {
      return H264Error::InvalidParameter;
   }

   *outMemoryRequirement = BaseMemoryRequirement + getLevelMemoryRequirement(level) + 1023u;

#if 0
   AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
   if (!codec) {
      return H264Error::OutOfMemory;
   }

   context = avcodec_alloc_context3(codec);
   if (!context) {
      return H264Error::OutOfMemory;
   }

   context->bit_rate = 400000;
   context->width = maxWidth;
   context->height = maxHeight;
   context->time_base.num = 1;
   context->time_base.den = 25;
   context->gop_size = 10;
   context->max_b_frames = 1;
   context->pix_fmt = AV_PIX_FMT_YUV420P;
   context->profile = profile;
   context->level = level;

   if (avcodec_open2(context, codec, NULL) < 0) {
      return H264Error::OutOfMemory;
   }

   frame = av_frame_alloc();
   if (!frame) {
      return H264Error::OutOfMemory;
   }

   frame->width = maxWidth;
   frame->height = maxHeight;
#endif

   return H264Error::OK;
}

int32_t
H264DECEnd(virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}


int32_t
H264DECInitParam(int32_t memSize,
	             virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECSetParam(virt_ptr<void> memPtr,
                int32_t paramid,
                virt_ptr<void> param)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECOpen(virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECBegin(virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}

#if 0
static int32_t ffmpeg_encoder_init_frame(AVFrame **framep,
	                                     int width,
	                                     int height)
{
   int ret;
   AVFrame *frame = av_frame_alloc();

   if (!frame) {
      gLog->error("Could not allocate video frame");
      return -1;
   }

   frame->format = context->pix_fmt;
   frame->width = width;
   frame->height = height;

   *framep = frame;

   return 0;
}
#endif


int32_t
H264DECExecute(virt_ptr<void> memPtr,
	           virt_ptr<void> StrFmPtr)
{
#if 0
   struct SwsContext *sws;
   sws = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width, frame->height, AV_PIX_FMT_NV12, 2, NULL, NULL, NULL);

   AVFrame* frame2;
   if (ffmpeg_encoder_init_frame(&frame2, frame->width, frame->height)) {
      return -1;
   }

   int num_bytes = avpicture_get_size((AVPixelFormat)(frame->format), frame->width, frame->height);

   if (num_bytes > 0) {
      uint8_t* frame2_buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
      int ret = avpicture_fill((AVPicture*)frame2, frame2_buffer, AV_PIX_FMT_NV12, frame->width, frame->height);

      int height = sws_scale(sws, frame->data, frame->linesize, 0, frame->height, frame2->data, frame2->linesize);

      int num_bytes2 = avpicture_get_size(AV_PIX_FMT_NV12, frame2->width, frame2->height);
      if (num_bytes2 > 0) {
         memcpy(StrFmPtr.getRawPointer(), frame2->data[0], num_bytes2);
      }
   }
//else
   decaf_warn_stub();
   int32_t imageSize = (width * height) + (width * (height / 2));

   if (imageSize > 0) {

      int p = 0;

      uint8_t* d = (uint8_t*)malloc(imageSize);

      uint8_t R = 128;
      uint8_t G = 32;
      uint8_t B = 192;

      uint8_t Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16;
      uint8_t V = (0.439 * R) - (0.368 * G) - (0.071 * B) + 128;
      uint8_t U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128;

      for (int h = 0; h < height; h++) {
         for (int w = 0; w < width; w++) {
            *(d + p++) = Y;
         }
      }
      for (int h = 0; h < height / 4; h++) {
		  for (int w = 0; w < width; w++) {
			  *(d + p++) = U;
			  *(d + p++) = V;
		  }
	  }

      if (imageSize > 0) {
         memcpy(StrFmPtr.getRawPointer(), d, imageSize);
      }
   }
#endif
   return 0x000000E4;
}



int32_t
H264DECFlush(virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECClose(virt_ptr<void> memPtr)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECSetBitstream(virt_ptr<void> memPtr,
                    const virt_ptr<uint8_t> bitstream,
                    const int32_t length,
                    const double timeStamp)
{
   int result = 0;

#if 0
   avpkt.size = length;
   if (avpkt.size == 0) {
      return -1;
   }

   frame->format = 1;

   avpkt.data = (uint8_t*)std::malloc(length);

   for (int i = 0; i < length; ++i) {
      avpkt.data[i] = bitstream[i];
   }

   int frame_count;
   result = avcodec_decode_video2(context, frame, &frame_count, &avpkt);

   if (result < 0) {
      gLog->error("Error decoding stream");
   }
   else {
      avpkt.size = result;
      result = 0;
   }
#endif

   return result;
}

int32_t
H264DECCheckDecunitLength(virt_ptr<void> memPtr,
                          const virt_ptr<uint8_t> buf,
                          int32_t totalBytes,
                          int32_t streamOffset,
                          virt_ptr<int32_t>length)
{
   decaf_warn_stub();
   return 0;
}

int32_t
H264DECCheckSkipableFrame(const virt_ptr<uint8_t> buf,
                          int32_t Length,
                          virt_ptr<int32_t>SkipFlag)
{
   decaf_warn_stub();
   return 0;
}

constexpr auto H264SliceTypeMask = 0x1Fu;
constexpr auto H264NalSps = 7u;

H264Error
H264DECFindDecstartpoint(const virt_ptr<uint8_t> buf,
                         int32_t totalBytes,
                         virt_ptr<int32_t> streamOffset)
{
   if (!buf || totalBytes < 4 || !streamOffset) {
      return H264Error::InvalidParameter;
   }

   for (auto i = 0; i < totalBytes - 4; i++) {
      auto start = buf[i];
      if (buf[i + 0] == 0 &&
          buf[i + 1] == 0 &&
          buf[i + 2] == 1 &&
          (buf[i + 3] & H264SliceTypeMask) == H264NalSps) {
         *streamOffset = ((i - 1) < 0) ? 0 : (i - 1);
         return H264Error::OK;
      }
   }

   return H264Error::GenericError;
}

int32_t
H264DECFindIdrpoint(const virt_ptr <uint8_t> buf,
                    int32_t totalBytes,
                    virt_ptr <int32_t> streamOffset)
{
   decaf_warn_stub();
   return 0;
}

class BitStream
{
public:
   BitStream(uint8_t *buffer, size_t size) :
      mBuffer(buffer),
      mSize(size),
      mBytePosition(0),
      mBitPosition(0)
   {
   }

   uint8_t readU1()
   {
      if (eof()) {
         return 0;
      }

      uint8_t value = (mBuffer[mBytePosition] >> (7 - mBitPosition)) & 1;
      mBitPosition++;
      if (mBitPosition == 8) {
         mBitPosition = 0;
         mBytePosition++;
      }

      return value;
   }

   uint32_t readU(size_t n)
   {
      uint32_t value = 0u;
      for (auto i = 0u; i < n; ++i) {
         value |= readU1() << (n - i - 1);
      }

      return value;
   }

   uint8_t readU3()
   {
      return static_cast<uint8_t>(readU(3));
   }

   uint8_t readU4()
   {
      return static_cast<uint8_t>(readU(4));
   }

   uint8_t readU5()
   {
      return static_cast<uint8_t>(readU(5));
   }

   uint8_t readU8()
   {
      if (eof()) {
         return 0;
      }

      if (mBitPosition == 0) {
         return mBuffer[mBytePosition++];
      }

      return static_cast<uint8_t>(readU(8));
   }

   uint16_t readU16()
   {
      return static_cast<uint16_t>(readU(16));
   }

   uint32_t readUE()
   {
      uint32_t r = 0;
      int i = 0;

      while (readU1() == 0 && i < 32 && !eof()) {
         i++;
      }

      r = readU(i);
      r += (1 << i) - 1;
      return r;
   }

   int32_t readSE()
   {
      int32_t r = static_cast<int32_t>(readUE());
      if (r & 0x01) {
         r = (r + 1) / 2;
      } else {
         r = -(r / 2);
      }
      return r;
   }

   int eof() {
      return mBytePosition >= mSize;
   }

private:
   uint8_t *mBuffer;
   size_t mSize;
   size_t mBytePosition;
   size_t mBitPosition;
};

struct H264SequenceParameterSet
{
   be2_val<uint8_t> complete;
   UNKNOWN(0x1);
   be2_val<uint16_t> profile_idc;
   be2_val<uint16_t> level_idc;
   be2_val<uint8_t> constraint_set;
   be2_val<uint8_t> seq_parameter_set_id;
   be2_val<uint8_t> chroma_format_idc;
   be2_val<uint8_t> residual_colour_transform_flag;
   be2_val<uint8_t> bit_depth_luma_minus8;
   be2_val<uint8_t> bit_depth_chroma_minus8;
   be2_val<uint8_t> qpprime_y_zero_transform_bypass_flag;
   be2_val<uint8_t> seq_scaling_matrix_present_flag;
   be2_array<int8_t, 6 * 16> scalingList4x4;
   be2_array<int8_t, 2 * 64> scalingList8x8;
   be2_val<uint16_t> log2_max_frame_num_minus4;
   be2_val<uint16_t> pic_order_cnt_type;
   be2_val<uint16_t> log2_max_pic_order_cnt_lsb_minus4;
   be2_val<uint16_t> delta_pic_order_always_zero_flag;
   PADDING(0x2);
   be2_val<int32_t> offset_for_non_ref_pic;
   be2_val<int32_t> offset_for_top_to_bottom_field;
   be2_val<uint16_t> num_ref_frames_in_pic_order_cnt_cycle;
   PADDING(0x2);
   be2_array<int32_t, 256> offset_for_ref_frame;
   be2_val<uint16_t> num_ref_frames;
   be2_val<uint8_t> gaps_in_frame_num_value_allowed_flag;
   PADDING(0x1);
   be2_val<uint16_t> pic_width_in_mbs;
   be2_val<uint16_t> pic_height_in_map_units;
   be2_val<uint8_t> frame_mbs_only_flag;
   be2_val<uint8_t> mb_adaptive_frame_field_flag;
   be2_val<uint8_t> direct_8x8_inference_flag;
   be2_val<uint8_t> frame_cropping_flag;
   be2_val<uint16_t> frame_crop_left_offset;
   be2_val<uint16_t> frame_crop_right_offset;
   be2_val<uint16_t> frame_crop_top_offset;
   be2_val<uint16_t> frame_crop_bottom_offset;
   be2_val<uint8_t> vui_parameters_present_flag;
   be2_val<uint8_t> vui_aspect_ratio_info_present_flag;
   be2_val<uint8_t> vui_aspect_ratio_idc;
   PADDING(0x1);
   be2_val<uint16_t> vui_sar_width;
   be2_val<uint16_t> vui_sar_height;
   be2_val<uint8_t> vui_overscan_info_present_flag;
   be2_val<uint8_t> vui_overscan_appropriate_flag;
   be2_val<uint8_t> vui_video_signal_type_present_flag;
   be2_val<uint8_t> vui_video_format;
   be2_val<uint8_t> vui_video_full_range_flag;
   be2_val<uint8_t> vui_colour_description_present_flag;
   be2_val<uint8_t> vui_colour_primaries;
   be2_val<uint8_t> vui_transfer_characteristics;
   be2_val<uint8_t> vui_matrix_coefficients;
   be2_val<uint8_t> vui_chroma_loc_info_present_flag;
   be2_val<uint8_t> vui_chroma_sample_loc_type_top_field;
   be2_val<uint8_t> vui_chroma_sample_loc_type_bottom_field;
   be2_val<uint8_t> vui_timing_info_present_flag;
   PADDING(0x3);
   be2_val<uint32_t> vui_num_units_in_tick;
   be2_val<uint32_t> vui_time_scale;
   be2_val<uint8_t> vui_fixed_frame_rate_flag;
   be2_val<uint8_t> vui_nal_hrd_parameters_present_flag;
   be2_val<uint8_t> vui_vcl_hrd_parameters_present_flag;
   PADDING(0x1);
   be2_val<uint16_t> hrd_cpb_cnt_minus1;
   be2_val<uint16_t> hrd_bit_rate_scale;
   be2_val<uint16_t> hrd_cpb_size_scale;
   be2_array<uint16_t, 100> hrd_bit_rate_value_minus1;
   be2_array<uint16_t, 100> hrd_cpb_size_value_minus1;
   be2_array<uint16_t, 100> hrd_cbr_flag;
   be2_val<uint16_t> hrd_initial_cpb_removal_delay_length_minus1;
   be2_val<uint16_t> hrd_cpb_removal_delay_length_minus1;
   be2_val<uint16_t> hrd_dpb_output_delay_length_minus1;
   be2_val<uint16_t> hrd_time_offset_length;
   be2_val<uint8_t> vui_low_delay_hrd_flag;
   be2_val<uint8_t> vui_pic_struct_present_flag;
   be2_val<uint8_t> vui_bitstream_restriction_flag;
   be2_val<uint8_t> vui_motion_vectors_over_pic_boundaries_flag;
   be2_val<uint16_t> vui_max_bytes_per_pic_denom;
   be2_val<uint16_t> vui_max_bits_per_mb_denom;
   be2_val<uint16_t> vui_log2_max_mv_length_horizontal;
   be2_val<uint16_t> vui_log2_max_mv_length_vertical;
   be2_val<uint16_t> vui_num_reorder_frames;
   be2_val<uint16_t> unk0x7B0;
   be2_val<uint16_t> vui_max_dec_frame_buffering;
   be2_val<uint32_t> max_pic_order_cnt_lsb;
   be2_val<uint16_t> unk0x7B8;
   UNKNOWN(0x2);
   be2_val<uint16_t> pic_size_in_mbs;
   UNKNOWN(0x2);
   be2_val<uint32_t> max_frame_num;
   be2_val<uint8_t> log2_max_frame_num;
   be2_val<uint8_t> unk0x7C5;
   be2_val<uint16_t> unk0x7C6;
   be2_val<uint16_t> pic_width;
   be2_val<uint16_t> pic_height;
   be2_val<uint16_t> unk0x7CC;
   UNKNOWN(0x95E - 0x7CE);
   be2_val<uint8_t> unk0x95E;
   be2_val<uint8_t> unk0x95F;
   be2_val<uint8_t> unk0x960;
};

static void
read_scaling_list(BitStream &bs, int8_t *scalingList, int sizeOfScalingList)
{
   int lastScale = 8;
   int nextScale = 8;

   for (int i = 0; i < sizeOfScalingList; ++i) {
      if (nextScale != 0) {
         nextScale = (lastScale + bs.readSE() + 256) % 256;
      }

      scalingList[i] = (nextScale == 0) ? lastScale : nextScale;
      lastScale = scalingList[i];
   }
}

#define SAR_Extended 255 // Extended_SAR

static void
read_hrd_parameters(BitStream &bs, virt_ptr<H264SequenceParameterSet> sps)
{
   sps->hrd_cpb_cnt_minus1 = static_cast<uint16_t>(bs.readUE());
   if (sps->hrd_cpb_cnt_minus1 < 32) {
      sps->hrd_bit_rate_scale = bs.readU4();
      sps->hrd_cpb_size_scale = bs.readU4();

      for (auto i = 0; i <= sps->hrd_cpb_cnt_minus1; ++i) {
         sps->hrd_bit_rate_value_minus1[i] = static_cast<uint16_t>(bs.readUE());
         sps->hrd_cpb_size_value_minus1[i] = static_cast<uint16_t>(bs.readUE());
         sps->hrd_cbr_flag[i] = bs.readU1();
      }

      sps->hrd_initial_cpb_removal_delay_length_minus1 = bs.readU5();
      sps->hrd_cpb_removal_delay_length_minus1 = bs.readU5();
      sps->hrd_dpb_output_delay_length_minus1 = bs.readU5();
      sps->hrd_time_offset_length = bs.readU5();
   }
}

static void
read_vui_parameters(BitStream &bs, virt_ptr<H264SequenceParameterSet> sps)
{
   sps->vui_aspect_ratio_info_present_flag = bs.readU1();
   if (sps->vui_aspect_ratio_info_present_flag) {
      sps->vui_aspect_ratio_idc = bs.readU8();
      if (sps->vui_aspect_ratio_idc == SAR_Extended) {
         sps->vui_sar_width = bs.readU16();
         sps->vui_sar_height = bs.readU16();
      }
   }

   sps->vui_overscan_info_present_flag = bs.readU1();
   if (sps->vui_overscan_info_present_flag) {
      sps->vui_overscan_appropriate_flag = bs.readU1();
   }

   sps->vui_video_signal_type_present_flag = bs.readU1();
   if (sps->vui_video_signal_type_present_flag) {
      sps->vui_video_format = bs.readU3();
      sps->vui_video_full_range_flag = bs.readU1();
      sps->vui_colour_description_present_flag = bs.readU1();

      if (sps->vui_colour_description_present_flag) {
         sps->vui_colour_primaries = bs.readU8();
         sps->vui_transfer_characteristics = bs.readU8();
         sps->vui_matrix_coefficients = bs.readU8();
      }
   }

   sps->vui_chroma_loc_info_present_flag = bs.readU1();
   if (sps->vui_chroma_loc_info_present_flag) {
      sps->vui_chroma_sample_loc_type_top_field = static_cast<uint8_t>(bs.readUE());
      sps->vui_chroma_sample_loc_type_bottom_field = static_cast<uint8_t>(bs.readUE());
   }

   sps->vui_timing_info_present_flag = bs.readU1();
   if (sps->vui_timing_info_present_flag) {
      sps->vui_num_units_in_tick = bs.readU(32);
      sps->vui_time_scale = bs.readU(32);
      sps->vui_fixed_frame_rate_flag = bs.readU1();
   }

   sps->vui_nal_hrd_parameters_present_flag = bs.readU1();
   if (sps->vui_nal_hrd_parameters_present_flag) {
      sps->unk0x95E = uint8_t { 1 };
      read_hrd_parameters(bs, sps);
   }

   sps->vui_vcl_hrd_parameters_present_flag = bs.readU1();
   if (sps->vui_vcl_hrd_parameters_present_flag) {
      sps->unk0x95F = uint8_t { 1 };
      read_hrd_parameters(bs, sps);
   }

   if (sps->vui_nal_hrd_parameters_present_flag || sps->vui_vcl_hrd_parameters_present_flag) {
      sps->unk0x960 = uint8_t { 1 };
      sps->vui_low_delay_hrd_flag = bs.readU1();
   }

   sps->vui_pic_struct_present_flag = bs.readU1();
   sps->vui_bitstream_restriction_flag = bs.readU1();
   if (sps->vui_bitstream_restriction_flag) {
      sps->vui_motion_vectors_over_pic_boundaries_flag = bs.readU1();
      sps->vui_max_bytes_per_pic_denom = static_cast<uint16_t>(bs.readUE());
      sps->vui_max_bits_per_mb_denom = static_cast<uint16_t>(bs.readUE());
      sps->vui_log2_max_mv_length_horizontal = static_cast<uint16_t>(bs.readUE());
      sps->vui_log2_max_mv_length_vertical = static_cast<uint16_t>(bs.readUE());
      sps->vui_num_reorder_frames = static_cast<uint16_t>(bs.readUE());
      sps->unk0x7B0 = uint16_t { 5 };
      sps->vui_max_dec_frame_buffering = static_cast<uint16_t>(bs.readUE() + 1);
   }
}

H264Error
decode_sequence_ps(uint8_t *buffer, uint32_t bufferSize, virt_ptr<H264SequenceParameterSet> sps)
{
   auto bs = BitStream { buffer, bufferSize };
   sps->profile_idc = bs.readU8();
   if (sps->profile_idc != 66 &&
       sps->profile_idc != 77 &&
       sps->profile_idc != 100) {
      return H264Error::InvalidProfile;
   }

   sps->constraint_set = bs.readU8();
   sps->level_idc = bs.readU8();
   sps->seq_parameter_set_id = static_cast<uint8_t>(bs.readUE());

   if (sps->profile_idc == 100 || sps->profile_idc == 110 ||
       sps->profile_idc == 122 || sps->profile_idc == 144) {
      sps->chroma_format_idc = static_cast<uint8_t>(bs.readUE());
      if (sps->chroma_format_idc == 3) {
         sps->residual_colour_transform_flag = bs.readU1();
      }

      sps->bit_depth_luma_minus8 = static_cast<uint8_t>(bs.readUE());
      sps->bit_depth_chroma_minus8 = static_cast<uint8_t>(bs.readUE());
      sps->qpprime_y_zero_transform_bypass_flag = bs.readU1();
      sps->seq_scaling_matrix_present_flag = bs.readU1();

      if (sps->seq_scaling_matrix_present_flag) {
         for (auto i = 0u; i < 8; ++i) {
            auto seq_scaling_list_present_flag = bs.readU1();
            if (seq_scaling_list_present_flag) {
               if (i < 6) {
                  read_scaling_list(bs, (virt_addrof(sps->scalingList4x4) + 16 * i).getRawPointer(), 16);
               } else {
                  read_scaling_list(bs, (virt_addrof(sps->scalingList8x8) + 64 * i).getRawPointer(), 64);
               }
            }
         }
      }
   }

   sps->log2_max_frame_num_minus4 = static_cast<uint16_t>(bs.readUE());
   if (sps->log2_max_frame_num_minus4 > 12) {
      return static_cast<H264Error>(26);
   }

   sps->log2_max_frame_num = static_cast<uint8_t>(sps->log2_max_frame_num_minus4 + 4);
   sps->max_frame_num = 1u << sps->log2_max_frame_num;

   sps->pic_order_cnt_type = static_cast<uint16_t>(bs.readUE());
   if (sps->pic_order_cnt_type > 2) {
      return H264Error::InvalidSps;
   }

   if (sps->pic_order_cnt_type == 0) {
      sps->log2_max_pic_order_cnt_lsb_minus4 = static_cast<uint16_t>(bs.readUE());
      sps->max_pic_order_cnt_lsb = 1u << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
   } else if (sps->pic_order_cnt_type == 1) {
      sps->delta_pic_order_always_zero_flag = bs.readU1();
      sps->offset_for_non_ref_pic = bs.readSE();
      sps->offset_for_top_to_bottom_field = bs.readSE();
      sps->num_ref_frames_in_pic_order_cnt_cycle = static_cast<uint16_t>(bs.readUE());

      for (int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; ++i) {
         sps->offset_for_ref_frame[i] = bs.readSE();
      }
   }

   sps->num_ref_frames = static_cast<uint16_t>(bs.readUE());
   if (sps->num_ref_frames > 16) {
      return H264Error::InvalidSps;
   }

   sps->gaps_in_frame_num_value_allowed_flag = bs.readU1();
   sps->pic_width_in_mbs = static_cast<uint16_t>(bs.readUE() + 1);
   sps->pic_width = static_cast<uint16_t>(16 * sps->pic_width_in_mbs);
   sps->pic_height_in_map_units = static_cast<uint16_t>(bs.readUE() + 1);
   sps->pic_height = static_cast<uint16_t>(16 * sps->pic_height_in_map_units);
   sps->pic_size_in_mbs = static_cast<uint16_t>(sps->pic_width_in_mbs * sps->pic_height_in_map_units);
   sps->frame_mbs_only_flag = bs.readU1();
   if (!sps->frame_mbs_only_flag) {
      sps->mb_adaptive_frame_field_flag = bs.readU1();
   }
   sps->unk0x7B8 = static_cast<uint16_t>((2 - sps->frame_mbs_only_flag) * sps->pic_height_in_map_units);

   sps->direct_8x8_inference_flag = bs.readU1();
   sps->frame_cropping_flag = bs.readU1();
   if (sps->frame_cropping_flag) {
      sps->frame_crop_left_offset = static_cast<uint16_t>(bs.readUE());
      sps->frame_crop_right_offset = static_cast<uint16_t>(bs.readUE());
      sps->frame_crop_top_offset = static_cast<uint16_t>(bs.readUE());
      sps->frame_crop_bottom_offset = static_cast<uint16_t>(bs.readUE());
   }

   if (sps->level_idc > 51) {
      sps->level_idc = uint16_t { 51 };
   }

   sps->unk0x7B0 = uint16_t { 5 };
   sps->vui_parameters_present_flag = bs.readU1();
   if (sps->vui_parameters_present_flag) {
      read_vui_parameters(bs, sps);
   }

   if (sps->unk0x7B0 > 5) {
      sps->unk0x7B0 = uint16_t { 5 };
   }

   sps->complete = uint8_t { 1 };
   sps->unk0x7CC = static_cast<uint16_t>(sps->pic_width_in_mbs * sps->unk0x7B8);
   sps->unk0x7C6 = static_cast<uint16_t>(sps->pic_width_in_mbs * sps->pic_height_in_map_units);
   return H264Error::OK;
}

void avLogCallback(void*, int, const char* fmt, va_list va)
{
   char buffer[512];
   buffer[0] = 0;
   vsprintf_s(buffer, 512, fmt, va);
   gLog->debug("{}", buffer);
}

H264Error
H264DECGetImageSize(const virt_ptr<uint8_t> buf,
                    int32_t totalBytes,
                    int32_t streamOffset,
                    virt_ptr<int32_t> width,
                    virt_ptr<int32_t> height)
{
   auto spsStart = virt_ptr<uint8_t> { nullptr };

   for (auto i = 0; i < totalBytes - 4; i++) {
      auto start = buf[i];
      if (buf[i + 0] == 0 &&
          buf[i + 1] == 0 &&
          buf[i + 2] == 1 &&
          (buf[i + 3] & H264SliceTypeMask) == H264NalSps) {
         spsStart = buf + i + 4;
         break;
      }
   }

   if (!spsStart) {
      return H264Error::GenericError;
   }

   auto spsMaxSize = totalBytes - (spsStart - buf);

   // Try to decode the SPS using ffmpeg
   av_register_all();
   av_log_set_level(AV_LOG_DEBUG);
   av_log_set_callback(avLogCallback);
   char buffer[] = { 0x00, 0x00, 0x00, 0x01, 0x68, 0xff };
   auto nalUnitHeaderSize = 5;
   auto nalFrameBuffer = (uint8_t *)av_malloc(spsMaxSize + nalUnitHeaderSize + sizeof(buffer) + AV_INPUT_BUFFER_PADDING_SIZE);
   nalFrameBuffer[0] = 0;
   nalFrameBuffer[1] = 0;
   nalFrameBuffer[2] = 0;
   nalFrameBuffer[3] = 1;
   nalFrameBuffer[4] = 0x67;
   memcpy(nalFrameBuffer + nalUnitHeaderSize, spsStart.getRawPointer(), spsMaxSize);
   memcpy(nalFrameBuffer + spsMaxSize + nalUnitHeaderSize, buffer, sizeof(buffer));
   memset(nalFrameBuffer + spsMaxSize + nalUnitHeaderSize + sizeof(buffer), 0, AV_INPUT_BUFFER_PADDING_SIZE);
   auto codec = avcodec_find_decoder(AV_CODEC_ID_H264);
   auto context = avcodec_alloc_context3(codec);
   context->debug = ~0;
   context->extradata = nalFrameBuffer;
   context->extradata_size = nalUnitHeaderSize + spsMaxSize + sizeof(buffer);
   auto error = avcodec_open2(context, codec, NULL);
   // avLogCallback will have been called with a message that includes the macroblock width & height
   // but context->width is not updated. Apparently ffmpeg requires a frame before it updates
   // these parameters.

   // Decode SPS
   StackObject<H264SequenceParameterSet> sps;
   decode_sequence_ps(spsStart.getRawPointer(), spsMaxSize, sps);

   *width = sps->pic_width;
   *height = sps->pic_height;

   if (!sps->frame_mbs_only_flag) {
      *height = 2 * sps->pic_height;
   }

   if (!*width || !*height) {
      return H264Error::GenericError;
   }

   return H264Error::OK;
}

void
Library::registerDecSymbols()
{
   RegisterFunctionExport(H264DECBegin);
   RegisterFunctionExport(H264DECCheckDecunitLength);
   RegisterFunctionExport(H264DECCheckMemSegmentation);
   RegisterFunctionExport(H264DECCheckSkipableFrame);
   RegisterFunctionExport(H264DECClose);
   RegisterFunctionExport(H264DECEnd);
   RegisterFunctionExport(H264DECExecute);
   RegisterFunctionExport(H264DECFindDecstartpoint);
   RegisterFunctionExport(H264DECFindIdrpoint);
   RegisterFunctionExport(H264DECFlush);
   RegisterFunctionExport(H264DECGetImageSize);
   RegisterFunctionExport(H264DECInitParam);
   RegisterFunctionExport(H264DECMemoryRequirement);
   RegisterFunctionExport(H264DECOpen);
   RegisterFunctionExport(H264DECSetBitstream);
   RegisterFunctionExport(H264DECSetParam);
   RegisterFunctionExportName("H264DECSetParam_FPTR_OUTPUT", H264DECSetParam);
   RegisterFunctionExportName("H264DECSetParam_OUTPUT_PER_FRAME", H264DECSetParam);
   RegisterFunctionExportName("H264DECSetParam_USER_MEMORY", H264DECSetParam);
}

} // namespace cafe::h264_dec