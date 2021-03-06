#include "base_model.h"
#include "misc.h"
#include "session.h"

BaseModel::BaseModel(DirNode* root, QObject *parent/* = 0*/) :
    QAbstractItemModel(parent), m_rootItem(root), m_row_count_changed(false)
{
    // integrate model into session
    connect(Session::instance(), SIGNAL(changeNode(const FileNode*)), this, SLOT(changeNode(const FileNode*)));
    connect(Session::instance(), SIGNAL(beginRemoveNode(const FileNode*)), this, SLOT(beginRemoveNode(const FileNode*)));
    connect(Session::instance(), SIGNAL(endRemoveNode()), this, SLOT(endRemoveNode()));
    connect(Session::instance(), SIGNAL(beginInsertNode(const FileNode*)), this, SLOT(beginInsertNode(const FileNode*)));
    connect(Session::instance(), SIGNAL(endInsertNode()), this, SLOT(endInsertNode()));
}

BaseModel::~BaseModel()
{
}

QModelIndex BaseModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
            return QModelIndex();

    FileNode* childItem = static_cast<FileNode*>(index.internalPointer());
    DirNode* parentItem = childItem->m_parent;

    if (parentItem == m_rootItem)
    {
        return QModelIndex();
    }

    bool found = false;
    int row = 0;

    if (parentItem->m_parent)
    {
        foreach(const DirNode* p, parentItem->m_parent->m_dir_vector) // search parent in grand parent!
        {
            if (p == parentItem)
            {
                found = true;
                break;
            }

            ++row;
        }

        if (found)
        {
            return createIndex(row, 0, parentItem);
        }
    }

    return QModelIndex();

}



int BaseModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    return colcount();
}

QModelIndex BaseModel::index(const FileNode* node)
{
    return node2index(node);
}

QModelIndex BaseModel::index( int row, int column, const QModelIndex & parent /*= QModelIndex()*/) const
{
    return QModelIndex();
}

void BaseModel::setRootNode(const QModelIndex& index)
{
    if (index.isValid())
    {
        qDebug() << "set index to " << static_cast<DirNode*>(index.internalPointer())->filepath();
        m_rootItem = static_cast<DirNode*>(index.internalPointer());
        reset();
    }
}

void BaseModel::setRootNode(DirNode* node)
{
    if (node)
    {
        qDebug() << "set index to " << node->filepath();
        m_rootItem = node;
        reset();
    }
}


// helper functions
QString BaseModel::hash(const QModelIndex& index) const
{
    if (!index.isValid()) return QString();
    return node(index)->hash();
}

bool BaseModel::contains_active_children(const QModelIndex& index) const
{
    if (!index.isValid()) return 0;
    return node(index)->contains_active_children();
}

bool BaseModel::active(const QModelIndex& index) const
{
    if (!index.isValid()) return false;
    return node(index)->is_active();
}

qint64 BaseModel::size(const QModelIndex &index) const
{
    if (!index.isValid()) return 0;
    return node(index)->size_on_disk();
}

QString BaseModel::type(const QModelIndex &index) const
{
    if (!index.isValid()) return QString();
    FileNode* p = node(index);

    if (p->m_displayType.isEmpty())
    {
        p->m_displayType = m_iconProvider.type(p->m_info);
    }

    return p->m_displayType;
}

QDateTime BaseModel::lastModified(const QModelIndex &index) const
{
    if (!index.isValid()) return QDateTime();
    return node(index)->m_info.lastModified();
}

QIcon BaseModel::icon(const QModelIndex& index) const
{
    if (!index.isValid()) return QIcon();
    FileNode* p = node(index);

    if (p->m_icon.isNull())
    {
        p->m_icon = m_iconProvider.icon(p->m_info);
    }

    return p->m_icon;
}

QString BaseModel::displayName(const QModelIndex &index) const
{
    return name(index);
}

QString BaseModel::name(const QModelIndex& index) const
{
    if (!index.isValid()) return QString();
    return node(index)->filename();
}

QString BaseModel::filepath(const QModelIndex& index) const
{
    if (!index.isValid()) return QString();
    return node(index)->filepath();
}

QString BaseModel::time(const QModelIndex& index) const
{
    if (!index.isValid()) return QString();
#ifndef QT_NO_DATESTRING
    return node(index)->m_info.lastModified().toString(Qt::SystemLocaleDate);
#else
    Q_UNUSED(index);
    return QString();
#endif
}

QDateTime BaseModel::dt(const QModelIndex& index) const
{
    if (!index.isValid()) return QDateTime();
    return node(index)->m_info.lastModified();
}

QFile::Permissions BaseModel::permissions(const QModelIndex &index) const
{
    if (!index.isValid()) return QFile::Permissions();
    return node(index)->m_info.permissions();
}

QString BaseModel::error(const QModelIndex& index) const
{
    QString res;

    if (index.isValid())
    {
        FileNode* p = node(index);
        Q_ASSERT(p);

        if (p->m_error)
        {
            res = BaseModel::tr(p->m_error.message().c_str());
        }
    }

    return res;
}

int BaseModel::has_error(const QModelIndex& index) const
{
    int res = 0;

    if (index.isValid())
    {
        FileNode* p = node(index);
        Q_ASSERT(p);
        if (p->m_error) res = 1;
    }

    return res;
}

QString BaseModel::size(qint64 bytes) const
{
    // According to the Si standard KB is 1000 bytes, KiB is 1024
    // but on windows sizes are calculated by dividing by 1024 so we do what they do.
    const qint64 kb = 1024;
    const qint64 mb = 1024 * kb;
    const qint64 gb = 1024 * mb;
    const qint64 tb = 1024 * gb;

    if (bytes >= tb)
        return misc::tr("%1 TB").arg(QLocale().toString(qreal(bytes) / tb, 'f', 3));
    if (bytes >= gb)
        return misc::tr("%1 GB").arg(QLocale().toString(qreal(bytes) / gb, 'f', 2));
    if (bytes >= mb)
        return misc::tr("%1 MB").arg(QLocale().toString(qreal(bytes) / mb, 'f', 1));
    if (bytes >= kb)
        return tr("%1 KB").arg(QLocale().toString(bytes / kb));
    return misc::tr("%1 bytes").arg(QLocale().toString(bytes));
}

Qt::CheckState BaseModel::state(const QModelIndex& index) const
{
    Qt::CheckState st = Qt::Unchecked;

    if (active(index))
    {
        if (!hash(index).isEmpty())
        {
            st = Qt::Checked;
        }
        else
        {
            st = Qt::PartiallyChecked;
        }
    }

    return st;
}

FileNode* BaseModel::node(const QModelIndex& index) const
{
    FileNode* p = static_cast<FileNode*>(index.internalPointer());
    Q_ASSERT(p);
    return (p);
}


// slots
void BaseModel::changeNode(const FileNode* node)
{
    QModelIndex indx = index(node);        

    if (indx.isValid())
    {
        emitChangeSignal(indx);
    }
}

void BaseModel::beginRemoveNode(const FileNode* node)
{
    QModelIndex indx = index(node);
    m_row_count_changed = false;

    if (indx.isValid())
    {
        qDebug() << "beginRemoveNode row: " << indx.row();
        beginRemoveRows(parent(indx), indx.row(), indx.row());
        m_row_count_changed = true;
    }
}

void BaseModel::endRemoveNode()
{
    if (m_row_count_changed)
    {
        endRemoveRows();
        m_row_count_changed = false;
    }
}

void BaseModel::beginInsertNode(const FileNode* node)
{
    QModelIndex indx = index(node);
    m_row_count_changed = false;

    if (indx.isValid())
    {
        int row = 0;
        FileNode* node = static_cast<FileNode*>(indx.internalPointer());
        DirNode* parent_node = node->m_parent;

        if (node->is_dir())
        {
            row = parent_node->m_dir_vector.size();
        }
        else
        {
            row = parent_node->m_file_vector.size();
        }

        qDebug() << "beginInsertNode row: " << row;
        beginInsertRows(index(parent_node), row, row);
        m_row_count_changed = true;
    }
}

void BaseModel::endInsertNode()
{
    if (m_row_count_changed)
    {
        endInsertRows();
        m_row_count_changed = false;
    }
}
