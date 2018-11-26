#include "h264.h"
#include "h264_decode.h"
#include "h264_stream.h"

#include "cafe/libraries/cafe_hle_stub.h"
#include "cafe/cafe_stackobject.h"

#include <common/align.h>
#include <common/decaf_assert.h>
#include <fmt/format.h>

#ifdef DECAF_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
}
#endif // ifdef DECAF_FFMPEG

namespace cafe::h264
{

// This is decaf specific stuff - not the same as in h264.rpl
struct H264CodecMemory
{
#ifdef DECAF_FFMPEG
   AVCodecContext *context;
   AVFrame *frame;
   AVBSFContext *bsf;
   AVPacket *packet;
   AVCodecParserContext *parser;
   SwsContext *sws;
   int swsWidth;
   int swsHeight;
   int inputFrameIndex;
   int outputFrameIndex;
#endif // ifdef DECAF_FFMPEG
};

constexpr auto WorkMemoryAlign = 0x100u;

constexpr auto MinimumWidth = 64u;
constexpr auto MinimumHeight = 64u;
constexpr auto MaximumWidth = 2800u;
constexpr auto MaximumHeight = 1408u;

constexpr auto BaseMemoryRequirement = 0x480000u;

namespace internal
{

virt_ptr<H264WorkMemory>
getWorkMemory(virt_ptr<void> memory)
{
   auto workMemory = byte_swap<virt_addr>(*virt_cast<virt_addr *>(memory));
   auto addrDiff = workMemory - virt_cast<virt_addr>(memory);
   if (addrDiff > WorkMemoryAlign || addrDiff < 0) {
      return nullptr;
   }

   return virt_cast<H264WorkMemory *>(workMemory);
}

static void
initialiseWorkMemory(virt_ptr<H264WorkMemory> workMemory,
                     virt_addr alignedMemoryEnd)
{
   auto baseAddress = virt_cast<virt_addr>(workMemory);
   workMemory->streamMemory = virt_cast<H264StreamMemory *>(baseAddress + 0x100u);
   workMemory->unk0x10 = virt_cast<void *>(baseAddress + 0x439C00);
   workMemory->bitStream = virt_cast<H264Bitstream *>(baseAddress + 0x439D00);
   workMemory->database = virt_cast<void *>(baseAddress + 0x439E00);
   workMemory->codecMemory = virt_cast<H264CodecMemory *>(baseAddress + 0x442300);
   workMemory->l1Memory = virt_cast<void *>(baseAddress + 0x445F00);
   workMemory->unk0x24 = virt_cast<void *>(baseAddress + 0x447F00);
   workMemory->internalFrameMemory = virt_cast<void *>(baseAddress + 0x44FC00);

   workMemory->streamMemory->workMemoryEnd =
      virt_cast<void *>(alignedMemoryEnd);
   workMemory->streamMemory->unkMemory =
      virt_cast<void *>(alignedMemoryEnd - 0x450000 + 0x300);
}

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

} // namespace internal


/**
 * Calculate the amount of memory required for the specified parameters.
 */
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

   *outMemoryRequirement =
      BaseMemoryRequirement + internal::getLevelMemoryRequirement(level) + 1023u;
   return H264Error::OK;
}


/**
 * Initialise a H264 decoder in the given memory.
 */
H264Error
H264DECInitParam(int32_t memorySize,
                 virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   if (memorySize < 0x44F900) {
      return H264Error::OutOfMemory;
   }

   std::memset(memory.get(), 0, memorySize);

   // Calculate aligned memory
   auto alignedMemory = align_up(memory, WorkMemoryAlign);
   auto alignOffset =
      virt_cast<virt_addr>(memory) - virt_cast<virt_addr>(alignedMemory);

   auto alignedMemoryStart = virt_cast<virt_addr>(alignedMemory);
   auto alignedMemoryEnd = alignedMemoryStart + memorySize - alignOffset;

   // Write aligned memory start reversed to memory
   *virt_cast<virt_addr *>(memory) = byte_swap(alignedMemoryStart);

   // Initialise our memory
   auto workMemory = virt_cast<H264WorkMemory *>(alignedMemoryStart);
   internal::initialiseWorkMemory(workMemory, alignedMemoryEnd);

   // TODO: More things.
   return H264Error::OK;
}


/**
 * Set H264 decoder parameter.
 */
H264Error
H264DECSetParam(virt_ptr<void> memory,
                H264Parameter parameter,
                virt_ptr<void> value)
{
   if (!memory || !value) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   auto streamMemory = workMemory->streamMemory;

   switch (parameter) {
   case H264Parameter::FramePointerOutput:
      streamMemory->paramFramePointerOutput =
         virt_func_cast<H264DECFptrOutputFn>(virt_cast<virt_addr>(value));
      break;
   case H264Parameter::OutputPerFrame:
      streamMemory->paramOutputPerFrame = *virt_cast<uint8_t *>(value);
      break;
   case H264Parameter::UserMemory:
      streamMemory->paramUserMemory = value;
      break;
   case H264Parameter::Unknown0x20000030:
      streamMemory->param_0x20000030 = *virt_cast<uint32_t *>(value);
      break;
   case H264Parameter::Unknown0x20000040:
      streamMemory->param_0x20000040 = *virt_cast<uint32_t *>(value);
      break;
   case H264Parameter::Unknown0x20000010:
      // TODO
   default:
      return H264Error::InvalidParameter;
   }

   return H264Error::OK;
}


/**
 * Set the callback which is called when a frame is output from the decoder.
 */
H264Error
H264DECSetParam_FPTR_OUTPUT(virt_ptr<void> memory,
                            H264DECFptrOutputFn value)
{
   if (!memory || !value) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   workMemory->streamMemory->paramFramePointerOutput = value;
   return H264Error::OK;
}


/**
 * Set whether the decoder should internally buffer frames or call the callback
 * immediately as soon as a frame is emitted.
 */
H264Error
H264DECSetParam_OUTPUT_PER_FRAME(virt_ptr<void> memory,
                                 uint32_t value)
{
   if (!memory || !value) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   if (value) {
      workMemory->streamMemory->paramOutputPerFrame = uint8_t { 1 };
   } else {
      workMemory->streamMemory->paramOutputPerFrame = uint8_t { 0 };
   }

   return H264Error::OK;
}


/**
 * Set a user memory pointer which is passed to the frame output callback.
 */
H264Error
H264DECSetParam_USER_MEMORY(virt_ptr<void> memory,
                            virt_ptr<void> value)
{
   if (!memory || !value) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   workMemory->streamMemory->paramUserMemory = value;
   return H264Error::OK;
}

#ifdef DECAF_FFMPEG
static int
ffmpegReceiveFrames(virt_ptr<H264WorkMemory> workMemory)
{
   auto result = 0;
   auto codecMemory = workMemory->codecMemory;
   auto streamMemory = workMemory->streamMemory;
   auto frame = codecMemory->frame;

   while (result == 0) {
      result = avcodec_receive_frame(codecMemory->context, frame);
      if (result != 0) {
         break;
      }

      // Get the decoded frame info
      auto &decodedFrameInfo = streamMemory->decodedFrameInfos[codecMemory->outputFrameIndex];
      codecMemory->outputFrameIndex =
         (codecMemory->outputFrameIndex + 1) % streamMemory->decodedFrameInfos.size();


      StackObject<H264DecodeResult> decodeResult;
      decodeResult->status = 100;
      decodeResult->timestamp = decodedFrameInfo.timestamp;

      const auto pitch = align_up(frame->width, 256);

      // Destroy previously created SWS if there is different width/height
      if (codecMemory->sws &&
         (codecMemory->swsWidth != frame->width ||
          codecMemory->swsHeight != frame->height)) {
         sws_freeContext(codecMemory->sws);
         codecMemory->sws = nullptr;
      }

      // Create SWS context if needed
      if (!codecMemory->sws) {
         codecMemory->sws =
            sws_getContext(frame->width, frame->height,
                           static_cast<AVPixelFormat>(frame->format),
                           frame->width, frame->height, AV_PIX_FMT_NV12,
                           0, nullptr, nullptr, nullptr);
      }

      // Use SWS to convert frame output to NV12 format
      decaf_check(codecMemory->sws);
      auto frameBuffer = virt_cast<uint8_t *>(decodedFrameInfo.buffer);
      uint8_t *dstBuffers[] = {
         frameBuffer.get(),
         frameBuffer.get() + frame->height * pitch,
      };
      int dstStride[] = {
         pitch, pitch
      };

      sws_scale(codecMemory->sws,
                frame->data, frame->linesize,
                0, frame->height,
                dstBuffers, dstStride);

      decodeResult->framebuffer = frameBuffer;
      decodeResult->width = frame->width;
      decodeResult->height = frame->height;
      decodeResult->nextLine = pitch;

      // Copy crop
      if (frame->crop_top || frame->crop_bottom || frame->crop_left || frame->crop_right) {
         decodeResult->cropEnableFlag = uint8_t { 1 };
      } else {
         decodeResult->cropEnableFlag = uint8_t { 0 };
      }

      decodeResult->cropTop = static_cast<int32_t>(frame->crop_top);
      decodeResult->cropBottom = static_cast<int32_t>(frame->crop_bottom);
      decodeResult->cropLeft = static_cast<int32_t>(frame->crop_left);
      decodeResult->cropRight = static_cast<int32_t>(frame->crop_right);

      // Copy pan scan
      decodeResult->panScanEnableFlag = uint8_t { 0 };
      decodeResult->panScanTop = 0;
      decodeResult->panScanBottom = 0;
      decodeResult->panScanLeft = 0;
      decodeResult->panScanRight = 0;

      for (auto i = 0; i < frame->nb_side_data; ++i) {
         auto sideData = frame->side_data[i];
         if (sideData->type == AV_FRAME_DATA_PANSCAN) {
            auto panScan = reinterpret_cast<AVPanScan *>(sideData->data);

            decodeResult->panScanEnableFlag = uint8_t { 1 };
            decodeResult->panScanTop = panScan->position[0][0];
            decodeResult->panScanLeft = panScan->position[0][1];
            decodeResult->panScanRight = decodeResult->panScanLeft + panScan->width;
            decodeResult->panScanBottom = decodeResult->panScanTop + panScan->height;
         }
      }

      // Copy vui_parameters from decoded frame info
      decodeResult->vui_parameters_present_flag = decodedFrameInfo.vui_parameters_present_flag;
      if (decodeResult->vui_parameters_present_flag) {
         decodeResult->vui_parameters = virt_addrof(decodedFrameInfo.vui_parameters);
      } else {
         decodeResult->vui_parameters = nullptr;
      }

      // Invoke the frame output callback, right now this is 1 frame at a time
      // in future we may want to hoist this outside of the loop and emit N frames
      StackArray<virt_ptr<H264DecodeResult>, 5> results;
      StackObject<H264DecodeOutput> output;
      output->frameCount = 1;
      output->decodeResults = results;
      output->userMemory = streamMemory->paramUserMemory;
      results[0] = decodeResult;

      cafe::invoke(cpu::this_core::state(),
                   streamMemory->paramFramePointerOutput,
                   output);
   }

   if (result != AVERROR_EOF && result != AVERROR(EAGAIN)) {
      char buffer[255];
      av_strerror(result, buffer, 255);
      gLog->error("avcodec_receive_frame error: {}", buffer);
   }

   return result;
}
#endif // ifdef DECAF_FFMPEG


/**
 * Open a H264 decoder.
 */
H264Error
H264DECOpen(virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

#ifdef DECAF_FFMPEG
   auto codec = avcodec_find_decoder(AV_CODEC_ID_H264);
   if (!codec) {
      return H264Error::GenericError;
   }

   auto context = avcodec_alloc_context3(codec);
   if (!context) {
      return H264Error::GenericError;
   }

   context->flags |= AV_CODEC_FLAG_LOW_DELAY;
   context->thread_type = FF_THREAD_SLICE;
   context->pix_fmt = AV_PIX_FMT_NV12;

   if (avcodec_open2(context, codec, NULL) < 0) {
      return H264Error::GenericError;
   }

   auto filter = av_bsf_get_by_name("extract_extradata");
   auto result = av_bsf_alloc(filter, &workMemory->codecMemory->bsf);
   workMemory->codecMemory->bsf->time_base_in = { };
   result = avcodec_parameters_from_context(workMemory->codecMemory->bsf->par_in,
                                   context);
   result = av_bsf_init(workMemory->codecMemory->bsf);

   workMemory->codecMemory->context = context;
   workMemory->codecMemory->frame = av_frame_alloc();
   workMemory->codecMemory->packet = av_packet_alloc();
#else
   decaf_warn_stub();
#endif // ifdef DECAF_FFMPEG

   return H264Error::OK;
}


/**
 * Prepare for decoding.
 */
H264Error
H264DECBegin(virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   std::memset(virt_addrof(workMemory->streamMemory->decodedFrameInfos).get(),
               0,
               sizeof(workMemory->streamMemory->decodedFrameInfos));

#ifdef DECAF_FFMPEG
   // Open a new parser, because there is no reset function for it and I don't
   // know if it has internal state which is important :).
   workMemory->codecMemory->parser = av_parser_init(AV_CODEC_ID_H264);
   workMemory->codecMemory->inputFrameIndex = 0;
   workMemory->codecMemory->outputFrameIndex = 0;
#else
   decaf_warn_stub();
#endif // ifdef DECAF_FFMPEG

   return H264Error::OK;
}


/**
 * Set the bit stream to be read for decoding.
 */
H264Error
H264DECSetBitstream(virt_ptr<void> memory,
                    virt_ptr<uint8_t> buffer,
                    uint32_t bufferLength,
                    double timestamp)
{
   if (!memory || !buffer || bufferLength < 4) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   workMemory->bitStream->buffer = buffer;
   workMemory->bitStream->buffer_length = bufferLength;
   workMemory->bitStream->bit_position = 0u;
   workMemory->bitStream->timestamp = timestamp;
   return H264Error::OK;
}


/**
 * Perform decoding of the bitstream and put the output frame into frameBuffer.
 */
H264Error
H264DECExecute(virt_ptr<void> memory,
               virt_ptr<void> frameBuffer)
{
   if (!memory || !frameBuffer) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   auto bitStream = workMemory->bitStream;
   auto codecMemory = workMemory->codecMemory;
   auto streamMemory = workMemory->streamMemory;

#ifdef DECAF_FFMPEG
   // Submit frames to ffmpeg
   if (bitStream->buffer_length) {
      auto packet = AVPacket { };
      av_init_packet(&packet);
      packet.data = bitStream->buffer.get();
      packet.size = bitStream->buffer_length;

      auto result = av_bsf_send_packet(codecMemory->bsf, &packet);
      if (result != 0) {
         return static_cast<H264Error>(result);
      }

      while (true) {
         result = av_bsf_receive_packet(codecMemory->bsf, codecMemory->packet);
         if (result < 0) {
            if (result != AVERROR_EOF && result != AVERROR(EAGAIN)) {
               char buffer[255];
               av_strerror(result, buffer, 255);
               gLog->error("av_bsf_receive_packet error: {}", buffer);
            }

            break;
         }

         // Update the decoded frame info for this frame
         auto &decodedFrameInfo = streamMemory->decodedFrameInfos[codecMemory->inputFrameIndex];
         codecMemory->inputFrameIndex =
            (codecMemory->inputFrameIndex + 1) % 6;

         decodedFrameInfo.buffer = frameBuffer;
         decodedFrameInfo.timestamp = bitStream->timestamp;

         // Check if there was a SPS / PPS in the packet so we can copy out the
         // vui parameters
         int extraDataSize;
         auto extraData = av_packet_get_side_data(codecMemory->packet,
                                                  AV_PKT_DATA_NEW_EXTRADATA,
                                                  &extraDataSize);
         if (extraData && extraDataSize) {
            StackObject<H264SequenceParameterSet> sps;
            if (internal::decodeNaluSps(extraData, extraDataSize, 0, sps) == H264Error::OK) {
               auto &vui = decodedFrameInfo.vui_parameters;
               // Copy VUI parameters from the SPS
               decodedFrameInfo.vui_parameters_present_flag = sps->vui_parameters_present_flag;
               vui.aspect_ratio_info_present_flag = sps->vui_aspect_ratio_info_present_flag;
               vui.aspect_ratio_idc = sps->vui_aspect_ratio_idc;
               vui.sar_width = sps->vui_sar_width;
               vui.sar_height = sps->vui_sar_height;
               vui.overscan_info_present_flag = sps->vui_overscan_info_present_flag;
               vui.overscan_appropriate_flag = sps->vui_overscan_appropriate_flag;
               vui.video_signal_type_present_flag = sps->vui_video_signal_type_present_flag;
               vui.video_format = sps->vui_video_format;
               vui.video_full_range_flag = sps->vui_video_full_range_flag;
               vui.colour_description_present_flag = sps->vui_colour_description_present_flag;
               vui.colour_primaries = sps->vui_colour_primaries;
               vui.transfer_characteristics = sps->vui_transfer_characteristics;
               vui.matrix_coefficients = sps->vui_matrix_coefficients;
               vui.chroma_loc_info_present_flag = sps->vui_chroma_loc_info_present_flag;
               vui.chroma_sample_loc_type_top_field = sps->vui_chroma_sample_loc_type_top_field;
               vui.chroma_sample_loc_type_bottom_field = sps->vui_chroma_sample_loc_type_bottom_field;
               vui.timing_info_present_flag = sps->vui_timing_info_present_flag;
               vui.num_units_in_tick = sps->vui_num_units_in_tick;
               vui.time_scale = sps->vui_time_scale;
               vui.fixed_frame_rate_flag = sps->vui_fixed_frame_rate_flag;
               vui.nal_hrd_parameters_present_flag = sps->vui_nal_hrd_parameters_present_flag;
               vui.vcl_hrd_parameters_present_flag = sps->vui_vcl_hrd_parameters_present_flag;
               vui.low_delay_hrd_flag = sps->vui_low_delay_hrd_flag;
               vui.pic_struct_present_flag = sps->vui_pic_struct_present_flag;
               vui.bitstream_restriction_flag = sps->vui_bitstream_restriction_flag;
               vui.motion_vectors_over_pic_boundaries_flag = sps->vui_motion_vectors_over_pic_boundaries_flag;
               vui.max_bytes_per_pic_denom = sps->vui_max_bytes_per_pic_denom;
               vui.max_bits_per_mb_denom = sps->vui_max_bits_per_mb_denom;
               vui.log2_max_mv_length_horizontal = sps->vui_log2_max_mv_length_horizontal;
               vui.log2_max_mv_length_vertical = sps->vui_log2_max_mv_length_vertical;
               vui.num_reorder_frames = sps->vui_num_reorder_frames;
               vui.max_dec_frame_buffering = sps->vui_max_dec_frame_buffering;
            }
         }

         result = avcodec_send_packet(codecMemory->context, codecMemory->packet);
         if (result != 0) {
            return static_cast<H264Error>(result);
         }

         av_packet_unref(codecMemory->packet);
      }

      bitStream->buffer_length = 0u;
   }

   auto result = ffmpegReceiveFrames(workMemory);
   if (result == 0) {
      return static_cast<H264Error>(0x80 | 100);
   }

   return static_cast<H264Error>(result);
#else
   decaf_warn_stub();
   return H264Error::GenericError;
#endif // ifdef DECAF_FFMPEG
}

H264Error
H264DECFlush(virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

#ifdef DECAF_FFMPEG
   if (workMemory->codecMemory->context) {
      // Send a null packet to flush ffmpeg decoder
      auto packet = AVPacket { };
      av_init_packet(&packet);
      packet.data = nullptr;
      packet.size = 0;
      avcodec_send_packet(workMemory->codecMemory->context, &packet);

      // Receive the flushed frames
      ffmpegReceiveFrames(workMemory);
   }
#else
   decaf_warn_stub();
#endif // ifdef DECAF_FFMPEG

   return H264Error::OK;
}

H264Error
H264DECEnd(virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   // Flush the stream
   H264DECFlush(memory);

#ifdef DECAF_FFMPEG
   // Reset the context
   if (workMemory->codecMemory->context) {
      avcodec_flush_buffers(workMemory->codecMemory->context);
   }

   if (workMemory->codecMemory->parser) {
      av_parser_close(workMemory->codecMemory->parser);
      workMemory->codecMemory->parser = nullptr;
   }
#else
   decaf_warn_stub();
#endif // ifdef DECAF_FFMPEG

   return H264Error::OK;
}

H264Error
H264DECClose(virt_ptr<void> memory)
{
   if (!memory) {
      return H264Error::InvalidParameter;
   }

   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

#ifdef DECAF_FFMPEG
   av_frame_free(&workMemory->codecMemory->frame);
   avcodec_free_context(&workMemory->codecMemory->context);
   av_bsf_free(&workMemory->codecMemory->bsf);
   av_packet_free(&workMemory->codecMemory->packet);

   sws_freeContext(workMemory->codecMemory->sws);
   workMemory->codecMemory->sws = nullptr;

   // Just in case someone did not call H264DECEnd
   if (workMemory->codecMemory->parser) {
      av_parser_close(workMemory->codecMemory->parser);
      workMemory->codecMemory->parser = nullptr;
   }
#else
   decaf_warn_stub();
#endif // ifdef DECAF_FFMPEG

   return H264Error::OK;
}

H264Error
H264DECCheckMemSegmentation(virt_ptr<void> memory,
                            uint32_t size)
{
   if (!memory || !size) {
      return H264Error::InvalidParameter;
   }

   return H264Error::OK;
}

#if 0
// ffmpeg based H264DECCheckDecunitLength
H264Error
H264DECCheckDecunitLength(virt_ptr<void> memory,
                          virt_ptr<const uint8_t> buffer,
                          int32_t bufferLength,
                          int32_t offset,
                          virt_ptr<int32_t> outLength)
{
   auto workMemory = internal::getWorkMemory(memory);
   if (!workMemory) {
      return H264Error::InvalidParameter;
   }

   uint8_t *outBuf = nullptr;
   int outBufSize = 0;
   auto ret = av_parser_parse2(workMemory->codecMemory->parser,
                               workMemory->codecMemory->context,
                               &outBuf, &outBufSize,
                               buffer.get() + offset, bufferLength - offset,
                               AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

   if (ret < 0) {
      return H264Error::GenericError;
   }

   *outLength = outBufSize;
   return H264Error::OK;
}
#endif // if 0

void
Library::registerDecodeSymbols()
{
   RegisterFunctionExport(H264DECMemoryRequirement);
   RegisterFunctionExport(H264DECInitParam);
   RegisterFunctionExport(H264DECSetParam);
   RegisterFunctionExport(H264DECSetParam_FPTR_OUTPUT);
   RegisterFunctionExport(H264DECSetParam_OUTPUT_PER_FRAME);
   RegisterFunctionExport(H264DECSetParam_USER_MEMORY);
   RegisterFunctionExport(H264DECBegin);
   RegisterFunctionExport(H264DECOpen);
   RegisterFunctionExport(H264DECSetBitstream);
   RegisterFunctionExport(H264DECExecute);
   RegisterFunctionExport(H264DECFlush);
   RegisterFunctionExport(H264DECClose);
   RegisterFunctionExport(H264DECEnd);
   RegisterFunctionExport(H264DECCheckMemSegmentation);
}

} // namespace cafe::h264