#pragma once
#ifdef QT_MULTIMEDIA_LIB
#include <QAudioOutput>
#include <QObject>
#include <QDebug>

#include <libdecaf/decaf_sound.h>

class SoundDriver : public QObject, public decaf::SoundDriver
{
public:
   ~SoundDriver() override = default;

   bool start(unsigned outputRate, unsigned numChannels) override;
   void output(int16_t *samples, unsigned numSamples) override;
   void stop() override;

private:
   QAudioOutput *mAudioOutput;
   QIODevice *mAudioIo;
};
#endif
