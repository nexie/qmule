#include <fstream>
#include <libed2k/error_code.hpp>
#include "servers_table_model.h"
#include "misc.h"

#include <QDebug>

servers_table_model::servers_table_model(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int servers_table_model::rowCount(const QModelIndex& parent) const
{
    return server_met.m_servers.m_collection.size();
}

int servers_table_model::columnCount(const QModelIndex& parent) const
{
    return DC_DESCR + 1;
}

QVariant servers_table_model::data(const QModelIndex& index, int role) const
{
    QVariant res;

    if (!index.isValid())
        return res;

    switch(role)
    {
    case Qt::DisplayRole:
        {
            switch(index.column())
            {
            case DC_NAME:
                res = name(index.row());
                break;
            case DC_IP:
                res = ip(index.row());
                break;
            case DC_PORT:
                res = port(index.row());
                break;
            case DC_FILES:
                res = files(index.row());
                break;
            case DC_USERS:
                res = users(index.row());
                break;
            case DC_MAX_USERS:
                res = max_users(index.row());
                break;
            case DC_LOWID_USERS:
                res = lowid_users(index.row());
                break;
            case DC_DESCR:
                res = description(index.row());
                break;
            default:
                break;
            }
            break;
        }
    default:
        break;
    }

    return res;
}

QVariant servers_table_model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch(section)
    {
    case DC_NAME:
        return tr("Name");
    case DC_IP:
        return tr("IP");
    case DC_PORT:
        return tr("Port");
    case DC_FILES:
        return tr("Files");
    case DC_USERS:
        return tr("Users");
    case DC_MAX_USERS:
        return tr("Max users");
    case DC_LOWID_USERS:
        return tr("Low ID users");
    case DC_DESCR:
        return tr("Description");
    default:
        break;
    }

    return QVariant();
}

void servers_table_model::save()
{
    try
    {
        std::ofstream fs(misc::ED2KMetaLocation("server.met").toLocal8Bit(), std::ios_base::out | std::ios_base::binary);

        if (fs)
        {
            libed2k::archive::ed2k_oarchive oa(fs);
            oa << server_met;
        }
    }
    catch(libed2k::libed2k_exception& e)
    {
        qDebug() << "error on store servers model " << e.what();
    }
}

void servers_table_model::load()
{
    try
    {
        std::ifstream fs(misc::ED2KMetaLocation("server.met").toLocal8Bit(), std::ios_base::out | std::ios_base::binary);

        if (fs)
        {
            libed2k::archive::ed2k_iarchive ia(fs);
            ia >> server_met;
            qDebug() << "loaded " << server_met.m_servers.m_collection.size() << " servers";
        }
    }
    catch(libed2k::libed2k_exception& e)
    {
        qDebug() << "error on load servers model " << e.what();
    }
}

QString servers_table_model::ip(const QModelIndex& indx) const
{
    return ip(indx.row());
}

qint16 servers_table_model::port(const QModelIndex& indx) const
{
    return port(indx.row());
}

void servers_table_model::removeServer(const QModelIndex& index)
{
    if (index.isValid())
    {
        emit beginRemoveRows(QModelIndex(), index.row(), index.row());
        server_met.m_servers.m_collection.erase(server_met.m_servers.m_collection.begin() + index.row());
        server_met.m_servers.m_size = server_met.m_servers.m_collection.size();
        emit endRemoveRows();
    }
}

void servers_table_model::addServer(const QString& name, const libed2k::net_identifier& point)
{
    libed2k::server_met_entry sme;
    sme.m_network_point = point;

    if (std::find(server_met.m_servers.m_collection.begin(), server_met.m_servers.m_collection.end(), sme) ==
            server_met.m_servers.m_collection.end())
    {
        beginInsertRows(QModelIndex(), server_met.m_servers.m_collection.size(), server_met.m_servers.m_collection.size());
        sme.m_list.add_tag(boost::shared_ptr<libed2k::base_tag>(new libed2k::string_tag(name.toUtf8().constData(), libed2k::FT_FILENAME, true)));
        server_met.m_servers.m_collection.push_back(sme);
        endInsertRows();
    }
}

void servers_table_model::clear()
{
    if (!server_met.m_servers.m_collection.empty())
    {
        emit beginRemoveRows(QModelIndex(), 0, server_met.m_servers.m_collection.size()-1);
        server_met.m_servers.m_collection.clear();
        emit endRemoveRows();
    }
}

QString servers_table_model::ip(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return QString::fromStdString(libed2k::int2ipstr(server_met.m_servers.m_collection.at(row).m_network_point.m_nIP));
}

qint16  servers_table_model::port(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_network_point.m_nPort;
}

QString servers_table_model::name(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return QString::fromStdString(server_met.m_servers.m_collection.at(row).m_list.getStringTagByNameId(libed2k::FT_FILENAME));
}

QString servers_table_model::description(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return QString::fromStdString(server_met.m_servers.m_collection.at(row).m_list.getStringTagByNameId(libed2k::ST_DESCRIPTION));
}

quint64 servers_table_model::users(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByName("users");
}

quint64 servers_table_model::files(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByName("files");
}

quint64 servers_table_model::soft_files(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByNameId(libed2k::ST_SOFTFILES);
}

quint64 servers_table_model::hard_files(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByNameId(libed2k::ST_HARDFILES);
}

quint64 servers_table_model::max_users(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByNameId(libed2k::ST_MAXUSERS);
}

quint64 servers_table_model::lowid_users(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_list.getIntTagByNameId(libed2k::ST_LOWIDUSERS);
}

libed2k::net_identifier servers_table_model::identifier(int row) const
{
    Q_ASSERT(static_cast<unsigned int>(row) < server_met.m_servers.m_collection.size());
    return server_met.m_servers.m_collection.at(row).m_network_point;
}
