#pragma optimize("", off)
#include "registerswindow.h"
#include "kcollapsiblegroupbox.h"

#include <QFontDatabase>
#include <QTextBlock>
#include <QTextDocument>
#include <QVBoxLayout>

int regValueBlock = 0;
int regValuePosition = 0;

template<typename T>
static QString toHexString(T value, int width)
{
   return QString{ "%1" }.arg(value, width / 4, 16, QLatin1Char{ '0' }).toUpper();
}
RegistersWindow::RegistersWindow(QWidget *parent) :
   QPlainTextEdit(parent)
{
   // Set to fixed width font
   auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
   if (!(font.styleHint() & QFont::Monospace)) {
      font.setFamily("Monospace");
      font.setStyleHint(QFont::TypeWriter);
   }
   setFont(font);

   // Text colour to red when changed
   mChangedPalette.setColor(QPalette::Text, Qt::red);

   setReadOnly(true);


   auto cursor = QTextCursor{ document() };
   cursor.beginEditBlock();
   document()->clear();

   auto registerNameWidth = 10;
   for (auto i = 0; i < 32; ++i) {
      if (i != 0) {
         cursor.insertBlock();
      }

      cursor.insertText(QString{ "R%1" }.arg(i, 2, 10, QLatin1Char{ '0' }));
      cursor.insertText(QString{ ' ' }.repeated(registerNameWidth - cursor.positionInBlock()));

      mRegisterCursors.gprs[i] = { cursor.blockNumber(), cursor.positionInBlock() };
      cursor.insertText(toHexString(0, 32));
   }

   cursor.insertBlock();
   cursor.insertText(QString { "LR" });
   cursor.insertText(QString { ' ' }.repeated(registerNameWidth - cursor.positionInBlock()));
   mRegisterCursors.lr = { cursor.blockNumber(), cursor.positionInBlock() };
   cursor.insertText(toHexString(0, 32));

   cursor.insertBlock();
   cursor.insertText(QString { "CTR" });
   cursor.insertText(QString { ' ' }.repeated(registerNameWidth - cursor.positionInBlock()));
   mRegisterCursors.ctr = { cursor.blockNumber(), cursor.positionInBlock() };
   cursor.insertText(toHexString(0, 32));

   cursor.insertBlock();
   cursor.insertText(QString { "XER" });
   cursor.insertText(QString { ' ' }.repeated(registerNameWidth - cursor.positionInBlock()));
   mRegisterCursors.xer = { cursor.blockNumber(), cursor.positionInBlock() };
   cursor.insertText(toHexString(0, 32));

   cursor.insertBlock();
   cursor.insertText(QString { "MSR" });
   cursor.insertText(QString { ' ' }.repeated(registerNameWidth - cursor.positionInBlock()));
   mRegisterCursors.msr = { cursor.blockNumber(), cursor.positionInBlock() };
   cursor.insertText(toHexString(0, 32));

   cursor.endEditBlock();
   /*
   {
      auto group = new KCollapsibleGroupBox { };
      auto layout = new QGridLayout { };
      group->setTitle(tr("Condition Registers"));
      group->setExpanded(true);
      group->setLayout(layout);

      for (auto i = 0; i < 2; ++i) {
         auto header = new QLineEdit { "O Z + -" };
         header->setStyleSheet("background: transparent; border: 0px;");
         header->setFont(font);
         header->setReadOnly(true);
         header->setToolTip(tr("Overflow Zero Positive Negative"));
         layout->addWidget(header, 0, 1 + i * 2);
      }

      for (auto i = 0; i < 8; ++i) {
         auto row = 1 + i % 4;
         auto column = 2 * (i / 4);
         auto label = new QLabel { QString { "crf%1" }.arg(i) };
         label->setToolTip(tr("Condition Register Field %1").arg(i));

         auto value = new QLineEdit { "0 0 0 0" };
         value->setInputMask("B B B B");
         value->setReadOnly(true);

         layout->addWidget(label, row, column + 0);
         layout->addWidget(value, row, column + 1);

         mRegisterWidgets.cr[i] = value;
      }

      baseLayout->addWidget(group);
      mGroups.cr = group;
   }

   {
      auto group = new KCollapsibleGroupBox { };
      auto layout = new QGridLayout { };
      group->setTitle(tr("Floating Point Registers"));
      group->setExpanded(true);
      group->setLayout(layout);

      for (auto i = 0; i < 32; ++i) {
         auto label = new QLabel { QString { "f%1" }.arg(i) };

         auto hexValue = new QLineEdit { "0000000000000000" };
         hexValue->setInputMask("HHHHHHHHHHHHHHHH");
         hexValue->setReadOnly(true);

         auto floatValue = new QLineEdit { "0.0" };
         floatValue->setReadOnly(true);

         layout->addWidget(label, i, 0);
         layout->addWidget(hexValue, i, 1);
         layout->addWidget(floatValue, i, 2);

         mRegisterWidgets.fprFloat[i] = floatValue;
         mRegisterWidgets.fprHex[i] = hexValue;
      }

      baseLayout->addWidget(group);
      mGroups.fpr = group;
   }

   {
      auto group = new KCollapsibleGroupBox { };
      auto layout = new QGridLayout { };
      group->setTitle(tr("Paired Single 1 Registers"));
      group->setExpanded(true);
      group->setLayout(layout);

      for (auto i = 0; i < 32; ++i) {
         auto label = new QLabel { QString { "p%1" }.arg(i) };

         auto hexValue = new QLineEdit { "0000000000000000" };
         hexValue->setInputMask("HHHHHHHHHHHHHHHH");
         hexValue->setReadOnly(true);

         auto floatValue = new QLineEdit { "0.0" };
         floatValue->setReadOnly(true);

         layout->addWidget(label, i, 0);
         layout->addWidget(hexValue, i, 1);
         layout->addWidget(floatValue, i, 2);

         mRegisterWidgets.ps1Float[i] = floatValue;
         mRegisterWidgets.ps1Hex[i] = hexValue;
      }

      baseLayout->addWidget(group);
      mGroups.ps1 = group;
   }

   baseLayout->addStretch(1);*/
}

void
RegistersWindow::setDebugData(DebugData *debugData)
{
   mDebugData = debugData;
   connect(mDebugData, &DebugData::dataChanged, this, &RegistersWindow::debugDataChanged);
}

#if 0

void
RegistersWindow::updateRegisterValue(QLineEdit *lineEdit,
                                     uint32_t value)
{
   auto text = toHexString(value, 32);
   if (text != lineEdit->text()) {
      lineEdit->setText(text);
      lineEdit->setPalette(mChangedPalette);
   } else if (!mDebugPaused) {
      lineEdit->setPalette({ });
   }
}

void
RegistersWindow::updateRegisterValue(QLineEdit *lineEditFloat,
                                     QLineEdit *lineEditHex,
                                     double value)
{
   auto hexText = toHexString(*reinterpret_cast<uint64_t *>(&value), 64);

   if (hexText != lineEditHex->text()) {
      lineEditFloat->setText(QString { "%1" }.arg(value));
      lineEditFloat->setPalette(mChangedPalette);

      lineEditHex->setText(hexText);
      lineEditHex->setPalette(mChangedPalette);
   } else if (!mDebugPaused) {
      lineEditFloat->setPalette({ });
      lineEditHex->setPalette({ });
   }
}

void
RegistersWindow::updateConditionRegisterValue(QLineEdit *lineEdit, uint32_t value)
{
   auto text = QString { "%1 %2 %3 %4" }
      .arg((value >> 0) & 1)
      .arg((value >> 1) & 1)
      .arg((value >> 2) & 1)
      .arg((value >> 3) & 1);

   if (text != lineEdit->text()) {
      lineEdit->setText(text);
      lineEdit->setPalette(mChangedPalette);
   } else if (!mDebugPaused) {
      lineEdit->setPalette({ });
   }
}

for (auto i = 0; i < mRegisterWidgets.gpr.size(); ++i) {
   updateRegisterValue(mRegisterWidgets.gpr[i], thread->gpr[i]);
}

for (auto i = 0; i < mRegisterWidgets.cr.size(); ++i) {
   auto value = (thread->cr >> ((7 - i) * 4)) & 0xF;
   updateConditionRegisterValue(mRegisterWidgets.cr[i], value);
}

updateRegisterValue(mRegisterWidgets.lr, thread->lr);
updateRegisterValue(mRegisterWidgets.ctr, thread->ctr);
updateRegisterValue(mRegisterWidgets.xer, thread->xer);
updateRegisterValue(mRegisterWidgets.msr, thread->msr);

for (auto i = 0; i < mRegisterWidgets.fprFloat.size(); ++i) {
   updateRegisterValue(mRegisterWidgets.fprFloat[i],
      mRegisterWidgets.fprHex[i],
      thread->fpr[i]);
}

for (auto i = 0; i < mRegisterWidgets.ps1Float.size(); ++i) {
   updateRegisterValue(mRegisterWidgets.ps1Float[i],
      mRegisterWidgets.ps1Hex[i],
      thread->ps1[i]);
}
#endif

void
RegistersWindow::debugDataChanged()
{
   auto thread = mDebugData->activeThread();
   if (!thread) {
      return;
   }

   QTextCursor curosr = QTextCursor{ document()->findBlockByNumber(regValueBlock) };
   curosr.beginEditBlock();
   curosr.movePosition(QTextCursor::MoveOperation::NextCharacter, QTextCursor::MoveAnchor, regValuePosition);
   curosr.movePosition(QTextCursor::MoveOperation::EndOfWord, QTextCursor::KeepAnchor);
   curosr.insertText(toHexString(thread->gpr[1], 32));
   curosr.endEditBlock();

   if (!mDebugData->paused()) {
      mLastThreadState = *thread;
   }
}
