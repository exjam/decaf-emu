#include "mainwindow.h"

#include "inputsettings.h"
#include "vulkanwindow.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>

#include <libdecaf/decaf.h>

MainWindow::MainWindow(QVulkanInstance *vulkanInstance, DecafInterface *decafInterface, InputDriver *inputDriver, QWidget* parent) :
   QMainWindow(parent),
   mDecafInterface(decafInterface),
   mInputDriver(inputDriver)
{
   mVulkanWindow = new VulkanWindow(vulkanInstance, decafInterface);

   QWidget* wrapper = QWidget::createWindowContainer((QWindow*)mVulkanWindow);
   wrapper->setFocusPolicy(Qt::StrongFocus);
   wrapper->setFocus();
   setCentralWidget(QWidget::createWindowContainer(mVulkanWindow));

   mUi.setupUi(this);

   QObject::connect(decafInterface, &DecafInterface::titleLoaded,
                    this, &MainWindow::titleLoaded);

   // Create recent file actions
   mRecentFilesSeparator = mUi.menuFile->insertSeparator(mUi.actionExit);
   mRecentFilesSeparator->setVisible(false);

   for (auto i = 0u; i < mRecentFileActions.size(); ++i) {
      mRecentFileActions[i] = new QAction(this);
      mRecentFileActions[i]->setVisible(false);
      QObject::connect(mRecentFileActions[i], &QAction::triggered,
                       this, &MainWindow::menuOpenRecentFile);
      mUi.menuFile->insertAction(mRecentFilesSeparator, mRecentFileActions[i]);
   }

   updateRecentFileActions();
}

void
MainWindow::menuOpenInputSettings()
{
   InputSettings dialog(mInputDriver, this);
   dialog.exec();
}

void
MainWindow::titleLoaded(quint64 id, const QString &name)
{
   setWindowTitle(QString("decaf-qt - %1").arg(name));
}

bool
MainWindow::loadFile(QString path)
{
   // You only get one chance to run a game out here buddy.
   mUi.actionOpen->setDisabled(true);
   for (auto i = 0u; i < mRecentFileActions.size(); ++i) {
      mRecentFileActions[i]->setDisabled(true);
   }

   // Tell decaf to start the game!
   mDecafInterface->startGame(path);

   // Update recent files list
   {
      QSettings settings;
      QStringList files = settings.value("recentFileList").toStringList();
      files.removeAll(path);
      files.prepend(path);
      while (files.size() > MaxRecentFiles) {
         files.removeLast();
      }

      settings.setValue("recentFileList", files);
   }

   updateRecentFileActions();
   return true;
}

void
MainWindow::menuOpenFile()
{
   auto fileName = QFileDialog::getOpenFileName(this, tr("Open Application"), "", tr("RPX Files (*.rpx);;cos.xml (cos.xml);;"));
   if (!fileName.isEmpty()) {
      loadFile(fileName);
   }
}

void
MainWindow::menuOpenRecentFile()
{

   QAction *action = qobject_cast<QAction *>(sender());
   if (action) {
      loadFile(action->data().toString());
   }
}

void
MainWindow::updateRecentFileActions()
{
   QSettings settings;
   QStringList files = settings.value("recentFileList").toStringList();
   int numRecentFiles = qMin(files.size(), MaxRecentFiles);

   for (int i = 0; i < numRecentFiles; ++i) {
      QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
      mRecentFileActions[i]->setText(text);
      mRecentFileActions[i]->setData(files[i]);
      mRecentFileActions[i]->setVisible(true);
   }

   for (int j = numRecentFiles; j < MaxRecentFiles; ++j) {
      mRecentFileActions[j]->setVisible(false);
   }

   mRecentFilesSeparator->setVisible(numRecentFiles > 0);
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
#if 0
   QMessageBox::StandardButton resBtn = QMessageBox::question(this, "Are you sure?",
                                                              tr("A game is running, are you sure you want to exit?\n"),
                                                              QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
                                                              QMessageBox::Yes);
   if (resBtn != QMessageBox::Yes) {
      event->ignore();
      return;
   }
#endif
   mDecafInterface->shutdown();
   event->accept();
}
