#include "PlatformImplementation.h"

#include <QScreen>

#include <unistd.h>
#include <pwd.h>

using namespace std;
using namespace mega;

static const QString kFinderSyncBundleId = QString::fromUtf8("mega.mac.MEGAShellExtFinder");
static const QString kFinderSyncPath = QString::fromUtf8("/Applications/MEGAsync.app/Contents/PlugIns/MEGAShellExtFinder.appex/");

void PlatformImplementation::initialize(int /*argc*/, char *[] /*argv*/)
{
    setMacXActivationPolicy();
    mShellNotifier = std::make_shared<SignalShellNotifier>();
}

void PlatformImplementation::fileSelector(QString title, QString defaultDir, bool multiSelection, QWidget* parent, std::function<void (QStringList)> func)
{
    if (defaultDir.isEmpty())
    {
        defaultDir = QLatin1String("/");
    }

    selectorsImpl(title,defaultDir,multiSelection, true, false, parent, func);
}

void PlatformImplementation::folderSelector(QString title, QString defaultDir, bool multiSelection, QWidget* parent, std::function<void (QStringList)> func)
{
    if (defaultDir.isEmpty())
    {
        defaultDir = QLatin1String("/");
    }

    selectorsImpl(title,defaultDir, multiSelection, false, true, parent, func);
}

void PlatformImplementation::fileAndFolderSelector(QString title, QString defaultDir, bool multiSelection, QWidget* parent, std::function<void (QStringList)> func)
{
    if (defaultDir.isEmpty())
    {
        defaultDir = QLatin1String("/");
    }

    selectorsImpl(title,defaultDir, multiSelection, true, true, parent, func);
}

void PlatformImplementation::raiseFileFolderSelectors()
{
    raiseFileSelectionPanels();
}

void PlatformImplementation::closeFileFolderSelectors(QWidget* parent)
{
    closeFileSelectionPanels(parent);
}

bool PlatformImplementation::startOnStartup(bool value)
{
   return startAtLogin(value);
}

bool PlatformImplementation::isStartOnStartupActive()
{
    return isStartAtLoginActive();
}

void PlatformImplementation::addFileManagerExtensionToSystem()
{
    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-a")
               << kFinderSyncPath;

    QProcess::startDetached(QString::fromUtf8("pluginkit"), scriptArgs);
}

bool PlatformImplementation::isFileManagerExtensionEnabled()
{
    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-m")
               << QString::fromUtf8("-i")
               << kFinderSyncBundleId;

    QProcess p;
    p.start(QString::fromAscii("pluginkit"), scriptArgs);
    if (!p.waitForFinished(2000))
    {
        return false;
    }

    QString out = QString::fromUtf8(p.readAllStandardOutput().trimmed());
    if (out.isEmpty())
    {
        return false;
    }

    if (out.at(0) != QChar::fromAscii('?') && out.at(0) != QChar::fromAscii('+'))
    {
        return false;
    }

    return true;
}

void PlatformImplementation::reloadFileManagerExtension()
{
    bool finderExtEnabled = PlatformImplementation::isFileManagerExtensionEnabled();
    if (!finderExtEnabled) // No need to reload, extension is currenctly disabled and next time user enable it, it will launch updated version
    {
        return;
    }

    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-e")
               << QString::fromUtf8("tell application \"MEGAShellExtFinder\" to quit");

    QProcess p;
    p.start(QString::fromAscii("osascript"), scriptArgs);
    if (!p.waitForFinished(2000))
    {
        return;
    }

    scriptArgs.clear();
    scriptArgs << QString::fromUtf8("-c")
               << QString::fromUtf8("pluginkit -e ignore -i mega.mac.MEGAShellExtFinder && sleep 1 && pluginkit -e use -i mega.mac.MEGAShellExtFinder");
    QProcess::startDetached(QString::fromUtf8("bash"), scriptArgs);
}

void PlatformImplementation::enableFileManagerExtension(bool value)
{
    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-e")
               << (value ? QString::fromUtf8("use") : QString::fromUtf8("ignore")) //Enable or disable extension plugin
               << QString::fromUtf8("-i")
               << kFinderSyncBundleId;

    QProcess::startDetached(QString::fromUtf8("pluginkit"), scriptArgs);
}

void PlatformImplementation::streamWithApp(const QString &app, const QString &url)
{
    QString args;
    args = QString::fromUtf8("-a ");
    args += QDir::toNativeSeparators(QString::fromUtf8("\"")+ app + QString::fromUtf8("\"")) + QString::fromUtf8(" \"%1\"").arg(url);
    QString command = QString::fromLatin1("open ") + args;
    QProcess::startDetached(command);
}

bool PlatformImplementation::showInFolder(QString pathIn)
{

    //Escape possible double quotes from osascript command to avoid syntax errors and stop parsing arguments
    pathIn.replace(QString::fromLatin1("\""), QString::fromLatin1("\\\""));

    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-e")
               << QString::fromUtf8("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(pathIn);
    QProcess::startDetached(QString::fromUtf8("osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QString::fromUtf8("-e")
               << QString::fromUtf8("tell application \"Finder\" to activate");
    return QProcess::startDetached(QString::fromAscii("osascript"), scriptArgs);
}

void PlatformImplementation::startShellDispatcher(MegaApplication *receiver)
{
    if (!systemServiceTask)
    {
        systemServiceTask = new MacXSystemServiceTask(receiver);
    }

    if (!extService)
    {
        extService = new MacXExtServerService(receiver);
    }
}

void PlatformImplementation::stopShellDispatcher()
{
    if (systemServiceTask)
    {
        delete systemServiceTask;
        systemServiceTask = nullptr;
    }

    if (extService)
    {
        delete extService;
        extService = nullptr;
    }
}

void PlatformImplementation::notifyItemChange(const QString& path, int newState)
{
    if (!path.isEmpty())
    {
        if (extService)
        {
            emit extService->itemChange(path, newState);
        }

        mShellNotifier->notify(path);
    }
}

void PlatformImplementation::notifySyncFileChange(string* localPath, int newState)
{
    notifyItemChange(QString::fromStdString(*localPath), newState);
}

void PlatformImplementation::syncFolderAdded(QString syncPath, QString syncName, QString /*syncID*/)
{
    addPathToPlaces(syncPath,syncName);
    setFolderIcon(syncPath);

    if (extService)
    {
        emit extService->syncAdd(syncPath, syncName);
    }
}

void PlatformImplementation::syncFolderRemoved(QString syncPath, QString syncName, QString /*syncID*/)
{
    removePathFromPlaces(syncPath);
    unSetFolderIcon(syncPath);

    if (extService)
    {
        emit extService->syncDel(syncPath, syncName);
    }
}

void PlatformImplementation::notifyRestartSyncFolders()
{
    notifyAllSyncFoldersRemoved();
    notifyAllSyncFoldersAdded();
}

void PlatformImplementation::notifyAllSyncFoldersAdded()
{
    if (extService)
    {
        emit extService->allClients(MacXExtServer::NOTIFY_ADD_SYNCS);
    }
}

void PlatformImplementation::notifyAllSyncFoldersRemoved()
{
    if (extService)
    {
        emit extService->allClients(MacXExtServer::NOTIFY_DEL_SYNCS);
    }
}

QString PlatformImplementation::getDefaultOpenApp(QString extension)
{
    return defaultOpenApp(extension);
}

bool PlatformImplementation::registerUpdateJob()
{
    return registerUpdateDaemon();
}

bool PlatformImplementation::shouldRunHttpServer()
{
    return runHttpServer();
}

bool PlatformImplementation::isUserActive()
{
    return userActive();
}

double PlatformImplementation::getUpTime()
{
    return uptime();
}

void PlatformImplementation::disableSignalHandler()
{
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
}

QString PlatformImplementation::getDeviceName()
{
    // First, try to read maker and model
    QString deviceName;
    QProcess proc;

    proc.start(QLatin1String("/bin/sh"), QStringList()<<QLatin1String("-c")
                                                       <<QLatin1String("system_profiler SPHardwareDataType | "
                                                                       "grep \"Model Name\" | awk -F \"Model "
                                                                       "Name: \" '{print $2}' | tr -d '\n'"));
    proc.waitForFinished();
    deviceName = QString::fromStdString(proc.readAll().toStdString());

    if (deviceName.isEmpty())
    {
        deviceName = QSysInfo::machineHostName();
        deviceName.remove(QLatin1Literal(".local"));
    }

    return deviceName;
}

void PlatformImplementation::initMenu(QMenu* m, const char *objectName, const bool applyDefaultStyling)
{
    if (m)
    {
        m->setObjectName(QString::fromUtf8(objectName));
        if (applyDefaultStyling)
        {
            m->setStyleSheet(QLatin1String("QMenu {"
                                               "background: #ffffff;"
                                               "padding-top: 5px;"
                                               "padding-bottom: 5px;"
                                               "border: 1px solid #B8B8B8;"
                                               "border-radius: 5px;"
                                           "}"
                                           "QMenu::separator {"
                                               "height: 1px;"
                                               "margin: 0px 8px 0px 8px;"
                                               "background-color: rgba(0, 0, 0, 0.1);"
                                           "}"
                                           // For vanilla QMenus (only in TransferManager and NodeSelectorTreeView (NodeSelector))
                                           "QMenu::item {"
                                               "font-size: 14px;"
                                               "margin: 6px 16px 6px 16px;"
                                               "color: #777777;"
                                               "padding-right: 16px;"
                                           "}"
                                           "QMenu::item:selected {"
                                               "color: #000000;"
                                           "}"
                                           // For menus with MenuItemActions
                                           "QLabel {"
                                               "font-family: Lato;"
                                               "font-size: 14px;"
                                               "padding: 0px;"
                                           "}"
                                           ));
            m->setAttribute(Qt::WA_TranslucentBackground);
            m->setWindowFlags(m->windowFlags() | Qt::FramelessWindowHint);
            m->ensurePolished();
        }
    }
}

void PlatformImplementation::calculateInfoDialogCoordinates(const QRect& rect, int* posx, int* posy)
{
    int xSign = 1;
    int ySign = 1;
    QPoint position;
    QRect screenGeometry;
    QSystemTrayIcon* trayIcon = MegaSyncApp->getTrayIcon();
    QPoint positionTrayIcon;
    positionTrayIcon = trayIcon->geometry().topLeft();

    position = QCursor::pos();
    QScreen* currentScreen = QGuiApplication::screenAt(position);
    if (currentScreen)
    {
        screenGeometry = currentScreen->availableGeometry();

        QString otherInfo = QString::fromUtf8("pos = [%1,%2], name = %3").arg(position.x()).arg(position.y()).arg(currentScreen->name());
        logInfoDialogCoordinates("availableGeometry", screenGeometry, otherInfo);

        if (!screenGeometry.isValid())
        {
            screenGeometry = currentScreen->geometry();
            otherInfo = QString::fromUtf8("dialog rect = %1").arg(rectToString(rect));
            logInfoDialogCoordinates("screenGeometry", screenGeometry, otherInfo);

            if (screenGeometry.isValid())
            {
                screenGeometry.setTop(28);
            }
            else
            {
                screenGeometry = rect;
                screenGeometry.setBottom(screenGeometry.bottom() + 4);
                screenGeometry.setRight(screenGeometry.right() + 4);
            }

            logInfoDialogCoordinates("screenGeometry 2", screenGeometry, otherInfo);
        }
        else
        {
            if (screenGeometry.y() < 0)
            {
                ySign = -1;
            }

            if (screenGeometry.x() < 0)
            {
                xSign = -1;
            }
        }

        MegaApi::log(MegaApi::LOG_LEVEL_DEBUG, QString::fromUtf8("Calculating Info Dialog coordinates. posTrayIcon = %1")
                           .arg(QString::fromUtf8("[%1,%2]").arg(positionTrayIcon.x()).arg(positionTrayIcon.y()))
                           .toUtf8().constData());

        if (positionTrayIcon.x() || positionTrayIcon.y())
        {
            if ((positionTrayIcon.x() + rect.width() / 2) > screenGeometry.right())
            {
                *posx = screenGeometry.right() - rect.width() - 1;
            }
            else
            {
                *posx = positionTrayIcon.x() + trayIcon->geometry().width() / 2 - rect.width() / 2 - 1;
            }
        }
        else
        {
            *posx = screenGeometry.right() - rect.width() - 1;
        }
        *posy = screenGeometry.top();

        if (*posy == 0)
        {
            *posy = 22;
        }
    }

    QString otherInfo = QString::fromUtf8("dialog rect = %1, posx = %2, posy = %3").arg(rectToString(rect)).arg(*posx).arg(*posy);
    logInfoDialogCoordinates("Final", screenGeometry, otherInfo);
}
