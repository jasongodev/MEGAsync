#include "BackupItemModel.h"

#include <QCoreApplication>
#include <QIcon>
#include <QFileInfo>

BackupItemModel::BackupItemModel(QObject *parent)
    : QAbstractItemModel(parent),
    mSyncModel (SyncModel::instance()),
    mSyncController(SyncController::instance()),
    mSortColumn(-1),
    mSortOrder(Qt::AscendingOrder)
{
    // use signal modelReset() for convenience;
    // individual updates might be better, especially to save selection indexes
    connect(mSyncModel, &SyncModel::syncStateChanged, this, &BackupItemModel::resetModel);
    connect(mSyncModel, &SyncModel::syncRemoved, this, &BackupItemModel::resetModel);
    resetModel();
}

QVariant BackupItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        switch(section)
        {
        case BackupItemColumn::ENABLED:
            if(role == Qt::SizeHintRole)
                return QSize(16, 16);
            if(role == Qt::ToolTipRole)
                return tr("Click to sort by enabled state");
            break;
        case BackupItemColumn::LNAME:
            if(role == Qt::DisplayRole)
                return tr("Local Folder");
            if(role == Qt::ToolTipRole)
                return tr("Click to sort alphabetically");
            break;
        case BackupItemColumn::MENU:
            if(role == Qt::SizeHintRole)
                return QSize(16, 16);
            if(role == Qt::ToolTipRole)
                return tr("Click to undo sorting");
            break;
        }
    }
    return QVariant();
}

void BackupItemModel::sort(int column, Qt::SortOrder order)
{
    switch (column)
    {
    case BackupItemColumn::ENABLED:
        std::sort(mOrderedList.begin(), mOrderedList.end(),
                  [order](std::shared_ptr<SyncSetting> a, std::shared_ptr<SyncSetting>b)
        {
            if(order == Qt::SortOrder::DescendingOrder)
            {
                if (!a->isEnabled())
                    return true;
                if (!b->isEnabled())
                    return false;
            }
            else if(order == Qt::SortOrder::AscendingOrder)
            {
                if (a->isEnabled())
                    return true;
                if (b->isEnabled())
                    return false;
            }
            return false;
        });
        break;
    case BackupItemColumn::LNAME:
        std::sort(mOrderedList.begin(), mOrderedList.end(),
                  [order](std::shared_ptr<SyncSetting> a, std::shared_ptr<SyncSetting>b)
        {
                if(order == Qt::SortOrder::DescendingOrder)
                    return a->getLocalFolder() < b->getLocalFolder();
                else
                    return a->getLocalFolder() > b->getLocalFolder();
        });
        break;
    default:
        // reset sort order
        mOrderedList = mList;
    }
    mSortColumn = column;
    mSortOrder = order;
    emit dataChanged(QModelIndex(), QModelIndex());
}

Qt::ItemFlags BackupItemModel::flags(const QModelIndex &index) const
{
    if (index.isValid() && (index.column() == BackupItemColumn::ENABLED))
        return QAbstractItemModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;

    return QAbstractItemModel::flags(index);
}

QModelIndex BackupItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return createIndex(row, column, nullptr);
}

QModelIndex BackupItemModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    // NOTE: Implement me for the tree view
    return QModelIndex();
}

int BackupItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return static_cast<int>(mOrderedList.size());
}

int BackupItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return BACKUP_ITEM_COLUMNS;
}

QVariant BackupItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    std::shared_ptr<SyncSetting> sync = mOrderedList.at(index.row());

    if(role == Qt::UserRole)
        return QVariant::fromValue(sync);

    switch(index.column())
    {
    case BackupItemColumn::ENABLED:
        if(role == Qt::CheckStateRole)
            return sync->isEnabled() ? Qt::Checked : Qt::Unchecked;
        else if(role == Qt::ToolTipRole)
            return sync->isEnabled() ? tr("Backup is enabled") : tr("Backup is disabled");
        break;
    case BackupItemColumn::LNAME:
        if(role == Qt::DecorationRole)
        {
            QIcon syncIcon;
            if(sync->getError())
            {
                syncIcon.addFile(QLatin1String(":/images/icons/pc/PC_warning_ico_rest.png"), QSize(16, 16), QIcon::Normal);
                syncIcon.addFile(QLatin1String(":/images/icons/pc/PC_warning_ico_hover.png"), QSize(16, 16), QIcon::Selected);
            }
            else
            {
                syncIcon.addFile(QLatin1String(":/images/icons/pc/PC_ico_rest.png"), QSize(16, 16), QIcon::Normal);
                syncIcon.addFile(QLatin1String(":/images/icons/pc/PC_ico_rest_hover.png"), QSize(16, 16), QIcon::Selected);
            }
            return syncIcon;
        }
        else if(role == Qt::DisplayRole)
        {
            return sync->name();
        }
        else if(role == Qt::ToolTipRole)
        {
            if(sync->getError())
                return QCoreApplication::translate("MegaSyncError", mega::MegaSync::getMegaSyncErrorCode(sync->getError()));
            else
                return sync->getLocalFolder();
        }
        break;
    case BackupItemColumn::MENU:
        QIcon dotsMenu;
        dotsMenu.addFile(QLatin1String(":/images/Item_options_rest.png"), QSize(16, 16), QIcon::Normal);
        dotsMenu.addFile(QLatin1String(":/images/Item_options_press.png"), QSize(16, 16), QIcon::Selected);
        dotsMenu.addFile(QLatin1String(":/images/Item_options_hover.png"), QSize(16, 16), QIcon::Active);
        if(role == Qt::DecorationRole)
            return dotsMenu;
        else if(role == Qt::SizeHintRole)
            return QSize(16, 16);
        else if(role == Qt::ToolTipRole)
            return tr("Click menu for more Backup actions");
        break;
    }
    return QVariant();
}

bool BackupItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || (index.column() != BackupItemColumn::ENABLED))
        return false;

    if ((role == Qt::CheckStateRole) || (role == Qt::EditRole))
    {
        if (value.toInt() == Qt::Checked)
            mSyncController.enableSync(mOrderedList.at(index.row()));
        else if (value.toInt() == Qt::Unchecked)
            mSyncController.disableSync(mOrderedList.at(index.row()));
        emit dataChanged(index, index, {role});
        return true;
    }

    return false;
}

bool BackupItemModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row + count - 1);
    // we are batch updating via resetModel()
    endInsertRows();
    return true;
}

bool BackupItemModel::insertColumns(int column, int count, const QModelIndex &parent)
{
    beginInsertColumns(parent, column, column + count - 1);
    // we are batch updating via resetModel()
    endInsertColumns();
    return true;
}

bool BackupItemModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    // we are batch updating via resetModel()
    endRemoveRows();
    return true;
}

bool BackupItemModel::removeColumns(int column, int count, const QModelIndex &parent)
{
    beginRemoveColumns(parent, column, column + count - 1);
    // we are batch updating via resetModel()
    endRemoveColumns();
    return true;
}

void BackupItemModel::resetModel()
{
    beginResetModel();
    mList = mOrderedList = mSyncModel->getSyncsByType(mega::MegaSync::SyncType::TYPE_BACKUP);
    sort(mSortColumn, mSortOrder);
    endResetModel();
}
