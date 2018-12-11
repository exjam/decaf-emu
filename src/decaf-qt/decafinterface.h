#pragma once
#include <libdecaf/decaf_eventlistener.h>
#include <libdecaf/decaf_graphics.h>
#include <libdecaf/decaf_sound.h>

#include <QObject>
#include <QString>
#include "inputdriver.h"

class DecafInterface : public QObject, public decaf::EventListener
{
   Q_OBJECT

public:
   DecafInterface(InputDriver *inputDriver, decaf::SoundDriver *soundDriver);

public slots:
   void startGame(QString path);
   void shutdown();
   void writeConfigFile();

signals:
   void titleLoaded(quint64 id, const QString &name);

protected:
   void onGameLoaded(const decaf::GameInfo &info) override;

private:
   bool mStarted = false;
   InputDriver *mInputDriver;
   decaf::SoundDriver *mSoundDriver;
};
