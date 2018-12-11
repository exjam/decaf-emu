#include "decafinterface.h"

#include <QCoreApplication>
#include <libconfig/config_toml.h>
#include <libdecaf/decaf.h>
#include <libdecaf/decaf_log.h>
#include <libdecaf/decaf_nullinputdriver.h>

DecafInterface::DecafInterface(InputDriver *inputDriver,
                               decaf::SoundDriver *soundDriver) :
   mInputDriver(inputDriver),
   mSoundDriver(soundDriver)
{
   QObject::connect(mInputDriver, &InputDriver::configurationUpdated,
                    this, &DecafInterface::writeConfigFile);

   // TODO: Read from command line QCoreApplication::arguments();

   // Read config
   decaf::createConfigDirectory();
   auto configPath = decaf::makeConfigPath("config.toml");
   try {
      auto toml = cpptoml::parse_file(configPath);
      config::loadFromTOML(toml);
      inputDriver->loadInputConfiguration(toml);
   } catch (cpptoml::parse_exception ex) {
   }

   // Initialise logging
   decaf::initialiseLogging("decaf-qt");
   decaf::addEventListener(this);

   decaf::setInputDriver(inputDriver);
   decaf::setSoundDriver(soundDriver);
}

void
DecafInterface::writeConfigFile()
{
   auto configPath = decaf::makeConfigPath("config.toml");
   try {
      // First read current file
      auto toml = cpptoml::parse_file(configPath);

      // Update it
      mInputDriver->writeInputConfiguration(toml);
      config::saveToTOML(toml);

      // Output
      std::ofstream out { configPath };
      out << (*toml);
   } catch (cpptoml::parse_exception ex) {
   }
}

void
DecafInterface::startGame(QString path)
{
   mStarted = true;
   decaf::initialise(path.toLocal8Bit().constData());
   decaf::start();
}

void
DecafInterface::shutdown()
{
   if (mStarted) {
      decaf::shutdown();
      mStarted = false;
   }
}

void
DecafInterface::onGameLoaded(const decaf::GameInfo &info)
{
   emit titleLoaded(info.titleId, QString::fromStdString(info.executable));
}
