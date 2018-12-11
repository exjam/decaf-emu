#pragma optimize("", off)
#include "sounddriver.h"

#ifdef QT_MULTIMEDIA_LIB
bool
SoundDriver::start(unsigned outputRate, unsigned numChannels)
{
   QAudioFormat format;
   format.setChannelCount(numChannels);
   format.setCodec("audio/pcm");
   format.setSampleRate(outputRate);
   format.setSampleSize(16);
   format.setSampleType(QAudioFormat::SignedInt);
   format.setByteOrder(QAudioFormat::LittleEndian);

   QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
   if (!info.isFormatSupported(format)) {
      qWarning() << "Raw audio format not supported by backend, cannot play audio.";
      return false;
   }

   mAudioOutput = new QAudioOutput(format, this);
   mAudioIo = mAudioOutput->start();
   return true;
}

void
SoundDriver::output(int16_t *samples, unsigned numSamples)
{
   mAudioIo->write(reinterpret_cast<const char *>(samples), numSamples * sizeof(int16_t));
}

void
SoundDriver::stop()
{
   mAudioOutput->stop();
   delete mAudioOutput;

   mAudioOutput = nullptr;
   mAudioIo = nullptr;
}
#endif
