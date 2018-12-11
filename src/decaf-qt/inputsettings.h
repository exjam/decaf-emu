#pragma once
#include "ui_inputsettings.h"
#include "inputdriver.h"

#include <SDL_joystick.h>
#include <QDialog>
#include <QVector>
#include <QMap>

class InputEventFilter;
class SdlEventLoop;

struct JoystickInfo
{
   SDL_JoystickID id = -1;
   SDL_JoystickGUID guid;
   const char *name = nullptr;
};

class InputSettings : public QDialog
{
   Q_OBJECT

public:
   InputSettings(InputDriver *inputDriver, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
   ~InputSettings();

private slots:
   void addController();
   void removeController();
   void editController(int index);
   void controllerTypeChanged(int index);
   void buttonBoxClicked(QAbstractButton *button);

   void assignButton(QPushButton *button, ButtonType type);
   void caughtKeyPress(int key);

   void joystickConnected(SDL_JoystickID id, SDL_JoystickGUID guid, const char *name);
   void joystickDisconnected(SDL_JoystickID id, SDL_JoystickGUID guid);
   void joystickButton(SDL_JoystickID id, SDL_JoystickGUID guid, int key);
   void joystickAxisMotion(SDL_JoystickID id, SDL_JoystickGUID guid, int axis, float value);
   void joystickHatMotion(SDL_JoystickID id, SDL_JoystickGUID guid, int hat, int value);

private:
   void updateControllerListFromConfiguration();
   void showEvent(QShowEvent *) override;

private:
   Ui::InputSettings mUi;
   QVector<JoystickInfo> mJoysticks;
   InputEventFilter *mInputEventFilter;

   ButtonType mAssignButtonType;
   QPushButton *mAssignButton = nullptr;
   InputDriver *mInputDriver = nullptr;

   InputConfiguration mInputConfiguration;
};
