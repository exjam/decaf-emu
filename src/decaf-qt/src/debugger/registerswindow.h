#pragma once
#include <array>
#include <cstdint>
#include <QWidget>
#include <QPlainTextEdit>

#include "debugdata.h"

class RegistersWindow : public QPlainTextEdit
{
   Q_OBJECT

   struct RegisterCursor
   {
      int block;
      int position;
   };

   struct RegisterCursors
   {
      std::array<RegisterCursor, 32> gprs;
      RegisterCursor lr;
      RegisterCursor ctr;
      RegisterCursor xer;
      RegisterCursor msr;
   };

public:
   RegistersWindow(QWidget *parent = nullptr);

   void setDebugData(DebugData *debugData);

private slots:
   void debugDataChanged();

private:
   DebugData *mDebugData = nullptr;

   QPalette mChangedPalette = { };
   decaf::debug::CafeThread mLastThreadState = { };
   RegisterCursors mRegisterCursors;
};
