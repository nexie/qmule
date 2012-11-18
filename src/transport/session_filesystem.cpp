#include <QDirIterator>
#include <QFileSystemModel>
#include <QTextStream>
#include <algorithm>
#ifdef Q_WS_WIN32
#include <QVarLengthArray>
#include <windows.h>
#endif

#include "session_filesystem.h"
#include "session.h"
#include "preferences.h"

#include <libed2k/file.hpp>
#include <libed2k/filesystem.hpp>


#ifdef Q_OS_WIN32
static QString qt_GetLongPathName(const QString &strShortPath)
{
    if (strShortPath.isEmpty()
        || strShortPath == QLatin1String(".") || strShortPath == QLatin1String(".."))
        return strShortPath;
    if (strShortPath.length() == 2 && strShortPath.endsWith(QLatin1Char(':')))
        return strShortPath.toUpper();
    const QString absPath = QDir(strShortPath).absolutePath();
    if (absPath.startsWith(QLatin1String("//"))
        || absPath.startsWith(QLatin1String("\\\\"))) // unc
        return QDir::fromNativeSeparators(absPath);
    if (absPath.startsWith(QLatin1Char('/')))
        return QString();
    const QString inputString = QLatin1String("\\\\?\\") + QDir::toNativeSeparators(absPath);
    QVarLengthArray<TCHAR, MAX_PATH> buffer(MAX_PATH);
    DWORD result = ::GetLongPathName((wchar_t*)inputString.utf16(),
                                     buffer.data(),
                                     buffer.size());
    if (result > DWORD(buffer.size())) {
        buffer.resize(result);
        result = ::GetLongPathName((wchar_t*)inputString.utf16(),
                                   buffer.data(),
                                   buffer.size());
    }
    if (result > 4) {
        QString longPath = QString::fromWCharArray(buffer.data() + 4); // ignoring prefix
        longPath[0] = longPath.at(0).toUpper(); // capital drive letters
        return QDir::fromNativeSeparators(longPath);
    } else {
        return QDir::fromNativeSeparators(strShortPath);
    }
}
#endif

QString translateDriveName(const QFileInfo &drive)
{
    QString driveName = drive.absoluteFilePath();
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
    if (driveName.startsWith(QLatin1Char('/'))) // UNC host
        return drive.fileName();
#endif
#if (defined(Q_OS_WIN) && !defined(Q_OS_WINCE)) || defined(Q_OS_SYMBIAN)
    if (driveName.endsWith(QLatin1Char('/')))
        driveName.chop(1);
#endif
    return driveName;
}

FileNode::FileNode(DirNode* parent, const QFileInfo& info) :
    m_parent(parent),
    m_active(false),
    m_info(info),
    m_atp(NULL)
{
    m_filename = m_info.fileName();
}

FileNode::~FileNode()
{
    delete m_atp;
}

void FileNode::create_transfer()
{
    try
    {
        m_atp->duplicate_is_error = true;
        m_hash = Session::instance()->get_ed2k_session()->addTransfer(*m_atp).hash();
        Session::instance()->registerNode(this);
        m_parent->drop_transfer_by_file();
    }
    catch(const libed2k::libed2k_exception& e)
    {
        m_error = e.error();
        m_active = false;
    }
}

void FileNode::share(bool recursive)
{
    Q_UNUSED(recursive);
    if (m_active) return;
    m_active = true;

    if (has_metadata())
    {
        create_transfer();
    }
    else
    {        
        Session::instance()->get_ed2k_session()->makeTransferParametersAsync(filepath());
    }

    Session::instance()->signal_changeNode(this);
}

void FileNode::unshare(bool recursive)
{
    Q_UNUSED(recursive);
    if (!m_active) return;
    qDebug() << indention() << "unshare file: " << filename();
    m_active = false;

    if (has_transfer())
    {
        Session::instance()->get_ed2k_session()->deleteTransfer(m_hash, false);
    }
    else
    {
        Session::instance()->get_ed2k_session()->cancelTransferParameters(filepath());
    }

    m_parent->drop_transfer_by_file();
    Session::instance()->signal_changeNode(this);
}

void FileNode::on_transfer_finished(const QString& hash)
{
    m_active = true;
    m_hash = hash;
    m_parent->drop_transfer_by_file();
    // TODO - we need extract add_transfer_parameters to node from transfer
    Session::instance()->registerNode(this);
    Session::instance()->signal_changeNode(this);
}

void FileNode::on_transfer_deleted()
{
    m_active = false;
    m_hash.clear();
    m_parent->drop_transfer_by_file();
    Session::instance()->signal_changeNode(this);
}

bool FileNode::on_metadata_completed(const libed2k::add_transfer_params& atp, const libed2k::error_code& ec)
{            
    if (!ec)
    {
        // parameters ready
        if (!m_atp) m_atp = new libed2k::add_transfer_params;

        *m_atp = atp;

        if (is_active())
        {
            create_transfer();
        }
    }
    else
    {        
        // parameters possibly ware cancelled or completed with errors
        delete m_atp;
        m_atp = NULL;
        m_active = false;
        m_error = ec;

        if (has_transfer())
        {
            Session::instance()->deleteTransfer(m_hash, false);
        }
    }

    Session::instance()->signal_changeNode(this);
    return (!m_error);
}

QString FileNode::filepath() const
{
    QStringList path;
    const DirNode* parent = m_parent;
    path << m_filename;

    while(parent && !parent->is_root())
    {
        path.prepend(parent->filename());
        parent = parent->m_parent;
    }

    QString fullPath = QDir::fromNativeSeparators(path.join(QDir::separator()));

#if !defined(Q_OS_WIN) || defined(Q_OS_WINCE)
    if ((fullPath.length() > 2) && fullPath[0] == QLatin1Char('/') && fullPath[1] == QLatin1Char('/'))
        fullPath = fullPath.mid(1);
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_SYMBIAN)
    if (fullPath.length() == 2 && fullPath.endsWith(QLatin1Char(':')))
        fullPath.append(QLatin1Char('/'));
#endif

    return fullPath;
}

int FileNode::level() const
{
    int level = 0;
    const DirNode* parent = m_parent;

    while(parent && !parent->is_root())
    {
        ++level;
        parent = parent->m_parent;
    }

    return level;
}

QString FileNode::indention() const
{
    QString indent;
    indent.fill(' ', level()*2);
    return indent;
}

QString FileNode::string() const
{
    QString res;

    if (m_atp)
    {
        res = misc::toQStringU(libed2k::emule_collection::toLink(libed2k::filename(m_atp->m_filepath), m_atp->file_size, m_atp->file_hash));
    }

    return (res);
}

DirNode::DirNode(DirNode* parent, const QFileInfo& info, bool root /*= false*/) :
    FileNode(parent, info),
    m_populated(false),
    m_root(root)
{
}

DirNode::~DirNode()
{
    foreach(FileNode* p, m_file_children.values()) { delete p; }
    foreach(DirNode* p, m_dir_children.values()) { delete p; }
}

void DirNode::share(bool recursive)
{
    if (!m_active)
    {
        m_active = true;
        Session::instance()->addDirectory(this);

        // execute without check current state
        // we can re-share files were unshared after directory was shared
        populate();

        foreach(FileNode* p, m_file_children.values())
        {
            p->share(recursive);
        }

        // update state on all children
        foreach(DirNode* p, m_dir_children.values())
        {
            p->update_state();
        }
    }

    // share all what are not shared
    foreach(DirNode* p, m_dir_children.values())
    {
        if (recursive)
        {
            p->share(recursive);
        }
    }

    Session::instance()->signal_changeNode(this);
}

void DirNode::unshare(bool recursive)
{
    qDebug() << indention() << "unshare dir: " << filename();

    if (m_active)
    {
        m_active = false;
        Session::instance()->removeDirectory(this);

        if (has_transfer())
        {
            Session::instance()->get_ed2k_session()->deleteTransfer(m_hash, true);
        }

        foreach(FileNode* p, m_file_children.values())
        {
            p->unshare(recursive);
        }

        // on non-recursive we update state because current node state was changed
        if (!recursive)
        {
            foreach(DirNode* p, m_dir_children.values())
            {
                p->update_state();
            }
        }
    }

    if (recursive)
    {
        foreach(DirNode* p, m_dir_children.values())
        {
            p->unshare(recursive);
        }
    }

    Session::instance()->signal_changeNode(this);
}

bool DirNode::contains_active_children() const
{
    bool active = m_active;

    if (!active)
    {
        foreach(const FileNode* node, m_file_children)
        {
            active = node->contains_active_children();
            if (active) break;
        }

        if (!active)
        {
            foreach(const DirNode* node, m_dir_children)
            {
                active = node->contains_active_children();
                if (active) break;
            }
        }
    }

    return active;
}

bool DirNode::all_active_children() const
{
    bool active = m_active;

    if (active)
    {
        foreach(const FileNode* node, m_file_children)
        {
            active = node->all_active_children();
            if (!active) break;
        }

        if (active)
        {
            foreach(const DirNode* node, m_dir_children)
            {
                active = (node->is_populated() && node->all_active_children());
                if (!active) break;
            }
        }
    }

    return active;
}

void DirNode::on_transfer_deleted()
{
    m_hash.clear();
}

bool DirNode::on_metadata_completed(const libed2k::add_transfer_params& atp, const libed2k::error_code& ec)
{
    Q_UNUSED(atp);
    Q_UNUSED(ec);
    return true;
    // do nothing
}

void DirNode::update_state()
{
    if (m_active && (has_transfer()))
    {
        Session::instance()->get_ed2k_session()->deleteTransfer(m_hash, true);
    }

    foreach(DirNode* node, m_dir_children)
    {
        node->update_state();
    }
}

void DirNode::drop_transfer_by_file()
{
    if (m_active && has_transfer())
    {
        Session::instance()->get_ed2k_session()->deleteTransfer(m_hash, true);
    }

    Session::instance()->signal_changeNode(this);
}

void DirNode::build_collection()
{
    // collection would hash, but hasn't transfer yet
    qDebug() << "build collection " << filename();
    bool pending = false;

    int files_count = 0;
    // check children
    foreach(const FileNode* p, m_file_children.values())
    {
        if (p->is_active())
        {
            ++files_count;

            if (!p->has_transfer())
            {
                pending = true;
                break;
            }
        }
    }

    if (!pending && (files_count > 0))
    {
        qDebug() << "collection " << filename() << " ready";
        QStringList lines;

        foreach(const FileNode* p, m_file_children.values())
        {
            if (p->is_active())
            {
                QString line = p->string();
                Q_ASSERT(!line.isEmpty());
                lines << line;
            }
        }

        int iteration = 0;
        // generate unique filename
        QDir cd(misc::ED2KCollectionLocation());
        QString collection_filepath;

        while(collection_filepath.isEmpty())
        {
            QString filename = collection_name() + (iteration?(QString("_") + QString::number(iteration)):QString()) + QString("-") + QString::number(lines.count()) + QString(".emulecollection");
            QFileInfo fi(cd.filePath(filename));

            if (fi.exists())
            {
                ++iteration;
            }
            else
            {
                collection_filepath = fi.absoluteFilePath();
            }
        }

        qDebug() << "collection filepath " << collection_filepath;

        QFile data(collection_filepath);

        if (data.open(QFile::WriteOnly | QFile::Truncate))
        {
             QTextStream out(&data);

             foreach(const QString& line, lines)
             {
                 out << line << "\n";
             }

             data.close();

             // hash file and add node
             std::pair<libed2k::add_transfer_params, libed2k::error_code> res_pair = Session::instance()->get_ed2k_session()->makeTransferParameters(collection_filepath);

             if (!res_pair.second)
             {
                Session::instance()->setDirectLink(md4toQString(res_pair.first.file_hash), this);   //!< direclty set link in dictionary
                Session::instance()->get_ed2k_session()->addTransfer(res_pair.first);
             }
        }
    }
}

QString DirNode::collection_name() const
{
    QString res;

    // check active by user wish, do not check files
    if (m_active)
    {
        QStringList name_list;
        const DirNode* parent = this;

        while(parent && !parent->is_root())
        {
            if (parent->m_active)
            {
                name_list.prepend(parent->filename());
            }

            parent = parent->m_parent;
        }

        res = name_list.join(QString("-"));
    }

    return res;
}

FileNode* DirNode::child(const QString& filename)
{
    return m_file_children.value(filename);
}

void DirNode::add_node(FileNode* node)
{
    if (m_populated) Session::instance()->signal_beginInsertNode(node);

    if (node->is_dir())
    {
        m_dir_children.insert(node->filename(), static_cast<DirNode*>(node));
        m_dir_vector.push_back(static_cast<DirNode*>(node));
    }
    else
    {
        m_file_children.insert(node->filename(), node);        
        m_file_vector.push_back(node);
    }

    if (m_populated) Session::instance()->signal_endInsertNode();
}

void DirNode::delete_node(const FileNode* node)
{
    if (m_populated) Session::instance()->signal_beginRemoveNode(node);

    if (node->is_dir())
    {
        m_dir_vector.erase(std::remove(m_dir_vector.begin(), m_dir_vector.end(), node), m_dir_vector.end());
        Q_ASSERT(m_dir_children.take(node->filename()));
    }
    else
    {
        m_file_vector.erase(std::remove(m_file_vector.begin(), m_file_vector.end(), node), m_file_vector.end());
        Q_ASSERT(m_file_children.take(node->filename()));
    }

    delete node;

    if (m_populated) Session::instance()->signal_endRemoveNode();
}

QStringList DirNode::exclude_files() const
{
    QStringList res;

    if (is_active())
    {
        foreach(const FileNode* p, m_file_children)
        {
            if (!p->is_active())
            {
                res << p->filename();
            }
        }
    }

    return res;
}

void DirNode::populate()
{
    if (m_populated) return;

    QString path = filepath();

    if (path.isEmpty())
    {
        foreach(const QFileInfo& fi, QDir::drives())
        {
            if (!m_dir_children.contains(translateDriveName(fi)))
            {
                DirNode* p = new DirNode(this, fi);
                p->m_filename = translateDriveName(fi);
                add_node(p);
            }
        }
    }
    else
    {
        QString itPath = QDir::fromNativeSeparators(path);
        QDirIterator dirIt(itPath, QDir::NoDotAndDotDot| QDir::AllEntries | QDir::System | QDir::Hidden);

        while(dirIt.hasNext())
        {
            dirIt.next();
            QFileInfo fileInfo = dirIt.fileInfo();

            if (fileInfo.isDir() && !m_dir_children.contains(fileInfo.fileName()))
            {
                add_node(new DirNode(this, fileInfo));
                continue;
            }

            if (fileInfo.isFile() && !m_file_children.contains(fileInfo.fileName()))
            {
                add_node(new FileNode(this, fileInfo));
                continue;
            }
        }
    }

    m_populated = true;
}
