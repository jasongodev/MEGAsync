#include <QDir>
#include <QDebug>

#include "ChooseFolder.h"
#include <Platform.h>
#include <MegaApplication.h>
#include "gui/node_selector/gui/NodeSelectorSpecializations.h"
#include <DialogOpener.h>
#include "mega/types.h"
#include "megaapi.h"
#include "Syncs.h"

ChooseLocalFolder::ChooseLocalFolder(QObject* parent)
    : QObject(parent)
{

}

void ChooseLocalFolder::openFolderSelector(const QString& folderPath)
{
    auto openFromFolder = QDir::toNativeSeparators(Utilities::getDefaultBasePath());

    if (!folderPath.isEmpty())
    {
        openFromFolder = QDir::toNativeSeparators(folderPath);
        QDir openFromFolderDir(openFromFolder);
        if (openFromFolderDir.cdUp())
        {
            openFromFolder = openFromFolderDir.path();
        }
        else
        {
            openFromFolder = QDir::toNativeSeparators(Utilities::getDefaultBasePath());
        }
    }

    SelectorInfo info;
    info.title = tr("Select local folder");
    info.defaultDir = openFromFolder;
    info.canCreateDirectoreis = true;
    info.func = [this](QStringList selection){
        if(!selection.isEmpty())
        {
            QString fPath = selection.first();
            auto folder = QDir::toNativeSeparators(QDir(fPath).canonicalPath());
            if(!folder.isNull() && !folder.isEmpty())
            {
                emit folderChoosen(folder);
            }
        }
    };

    Platform::getInstance()->folderSelector(info);
}

bool ChooseLocalFolder::createFolder(const QString& folderPath)
{
    if (folderPath.isEmpty())
    {
        return false;
    }

    auto folder = QDir::toNativeSeparators(folderPath);

    QDir defaultFolder(folder);
    if (!defaultFolder.exists() &&  (folder != getDefaultFolder(Syncs::DEFAULT_MEGA_FOLDER) || !defaultFolder.mkpath(QString::fromUtf8("."))))
    {
        mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                           QString::fromUtf8("ChooseFolder: %1 cannot be created.").arg(folderPath).toUtf8().constData());

        return false;
    }

    return true;
}

QString ChooseLocalFolder::getDefaultFolder(const QString& folderName)
{
    auto folder = Utilities::getDefaultBasePath();
    if (!folderName.isEmpty())
    {
        folder.append(QString::fromLatin1("/"));
        folder.append(folderName);
    }

    QDir dir(folder);
    folder = dir.canonicalPath(); //in case there is a folder with the same name MEGA but with different case: Mega

    return QDir::toNativeSeparators(folder);
}

QString ChooseRemoteFolder::DEFAULT_FOLDER_PATH = QString::fromLatin1("/");

ChooseRemoteFolder::ChooseRemoteFolder(QObject *parent)
    : QObject(parent)
    , mFolderHandle(mega::INVALID_HANDLE)
    , mFolderName(ChooseRemoteFolder::DEFAULT_FOLDER_PATH)
{
}

void ChooseRemoteFolder::openFolderSelector()
{
    QPointer<SyncNodeSelector> nodeSelector = new SyncNodeSelector();

    if(mFolderHandle != mega::INVALID_HANDLE)
    {
        std::shared_ptr<mega::MegaNode> node(MegaSyncApp->getMegaApi()->getNodeByHandle(mFolderHandle));
        nodeSelector->setSelectedNodeHandle(node);
    }

    DialogOpener::showDialog<SyncNodeSelector>(nodeSelector, [nodeSelector, this]()
    {
        if (nodeSelector->result() == QDialog::Accepted)
        {
            mFolderHandle = nodeSelector->getSelectedNodeHandle();
            auto node = MegaSyncApp->getMegaApi()->getNodeByHandle(mFolderHandle);
            if(node)
            {
                mFolderName = QString::fromUtf8(MegaSyncApp->getMegaApi()->getNodePath(node));
                if(!mFolderName.isNull() && !mFolderName.isEmpty())
                {
                    emit folderChoosen(mFolderName);
                    emit folderNameChanged();
                }
            }
        }
    });
}

mega::MegaHandle ChooseRemoteFolder::getHandle()
{
    return mFolderHandle;
}

void ChooseRemoteFolder::reset()
{
    mFolderHandle = mega::INVALID_HANDLE;
    mFolderName = ChooseRemoteFolder::DEFAULT_FOLDER_PATH;
    emit folderNameChanged();
}

QString ChooseRemoteFolder::getFolderName()
{
    return mFolderName;
}
