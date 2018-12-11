#pragma once
#include <QObject>
#include <QVector>
#include <SDL_joystick.h>

#include <array>
#include <libdecaf/decaf_input.h>
#include <mutex>
#include <cpptoml.h>

enum class ButtonType
{
   A,
   B,
   X,
   Y,
   R,
   L,
   ZR,
   ZL,
   Plus,
   Minus,
   Home,
   Sync,
   DpadUp,
   DpadDown,
   DpadLeft,
   DpadRight,
   LeftStickPress,
   LeftStickUp,
   LeftStickDown,
   LeftStickLeft,
   LeftStickRight,
   RightStickPress,
   RightStickUp,
   RightStickDown,
   RightStickLeft,
   RightStickRight,
   MaxButtonType,
};

enum class ControllerType
{
   Invalid,
   Gamepad,
   WiiMote,
   ProController,
   ClassicController,
};

struct InputConfiguration
{
   struct Input
   {
      enum Source
      {
         Unassigned,
         KeyboardKey,
         JoystickButton,
         JoystickAxis,
         JoystickHat,
      };

      Source source = Source::Unassigned;
      int id = -1;

      // Only valid for Source==Joystick*
      SDL_JoystickGUID joystickGuid;
      int joystickDuplicateId = 0; // For when there are multiple controllers with the same GUID

      // Only valid for Type==JoystickAxis
      bool invert = false; // TODO: Do we want to allow invert on everything?
      // TODO: deadzone? etc

      // Only valid for Source==JoystickHat
      int hatValue = -1;

      // Not loaded from config file, assigned on controller connect!
      SDL_Joystick *joystick = nullptr;
      SDL_JoystickID joystickInstanceId = -1;
   };

   struct Controller
   {
      ControllerType type = ControllerType::Invalid;
      std::array<Input, static_cast<size_t>(ButtonType::MaxButtonType)> inputs { };
   };

   std::vector<Controller> controllers;
   std::array<Controller *, 4> wpad = { nullptr, nullptr, nullptr, nullptr };
   std::array<Controller *, 2> vpad = { nullptr, nullptr };
};

struct ConnectedJoystick
{
   SDL_Joystick *joystick = nullptr;
   SDL_JoystickGUID guid;
   SDL_JoystickID instanceId;

   // This is for when we have multiple controllers with the same guid
   int duplicateId = 0;
};

static inline bool operator ==(const SDL_JoystickGUID &lhs, const SDL_JoystickGUID &rhs)
{
   return memcmp(&lhs, &rhs, sizeof(SDL_JoystickGUID)) == 0;
}

class InputDriver : public QObject, public decaf::InputDriver
{
   Q_OBJECT

public:
   InputDriver(QObject *parent = nullptr);
   ~InputDriver();

   void enableButtonEvents() { mButtonEventsEnabled = true; }
   void disableButtonEvents() { mButtonEventsEnabled = false; }

   void loadInputConfiguration(std::shared_ptr<cpptoml::table> config);
   void writeInputConfiguration(std::shared_ptr<cpptoml::table> config);
   void updateInputConfiguration(InputConfiguration config);

   InputConfiguration getInputConfiguration()
   {
      std::unique_lock<std::mutex> lock { mConfigurationMutex };
      return mConfiguration;
   }

signals:
   void joystickConnected(SDL_JoystickID id, SDL_JoystickGUID guid, const char *name);
   void joystickDisconnected(SDL_JoystickID id, SDL_JoystickGUID guid);
   void joystickButtonDown(SDL_JoystickID id, SDL_JoystickGUID guid, int button);
   void joystickAxisMotion(SDL_JoystickID id, SDL_JoystickGUID guid, int axis, float value);
   void joystickHatMotion(SDL_JoystickID id, SDL_JoystickGUID guid, int hat, int value);
   void configurationUpdated();

private slots:
   void update();

private:
   void sampleVpadController(int channel, decaf::input::vpad::Status &status) override;
   void sampleWpadController(int channel, decaf::input::wpad::Status &status) override;
   void applyInputConfiguration(InputConfiguration config);

private:
   bool mButtonEventsEnabled = false;
   std::mutex mConfigurationMutex;
   InputConfiguration mConfiguration;
   std::vector<ConnectedJoystick> mJoysticks;
};
