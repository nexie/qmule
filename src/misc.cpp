/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#include "misc.h"

#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QByteArray>
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include <QNetworkInterface>
#include <QTextCodec>
#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#include <QDesktopWidget>
#endif

#ifdef Q_WS_WIN
#include <shlobj.h>
#include <windows.h>
#include <PowrProf.h>
const int UNLEN = 256;
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef Q_WS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif

#ifndef Q_WS_WIN
#if defined(Q_WS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#else
#include <winbase.h>
#endif

#include "libtorrent/bencode.hpp"

#ifdef Q_WS_WIN
// migration
#include "transport/session.h"
#endif
#include "torrentpersistentdata.h"

#ifndef DISABLE_GUI
#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
#include <QDBusInterface>
#include <QDBusMessage>
#endif
#endif // DISABLE_GUI

#ifdef Q_WS_WIN
#include <QDesktopServices>
#endif

using namespace libtorrent;

const int MAX_FILENAME_LENGTH = 255;

static struct { const char *source; const char *comment; } units[] = {
  QT_TRANSLATE_NOOP3("misc", "B", "bytes"),
  QT_TRANSLATE_NOOP3("misc", "KiB", "kibibytes (1024 bytes)"),
  QT_TRANSLATE_NOOP3("misc", "MiB", "mebibytes (1024 kibibytes)"),
  QT_TRANSLATE_NOOP3("misc", "GiB", "gibibytes (1024 mibibytes)"),
  QT_TRANSLATE_NOOP3("misc", "TiB", "tebibytes (1024 gibibytes)")
};

QString misc::QDesktopServicesDataLocation() {
#ifdef Q_WS_WIN
  LPWSTR path=new WCHAR[256];
  QString result;
#if defined Q_WS_WINCE
  if (SHGetSpecialFolderPath(0, path, CSIDL_APPDATA, FALSE))
#else
  if (SHGetSpecialFolderPath(0, path, CSIDL_LOCAL_APPDATA, FALSE))
#endif
    result = QString::fromWCharArray(path);
  if (!QCoreApplication::applicationName().isEmpty())
    result = result + QLatin1String("\\") + qApp->applicationName();
  if (!result.endsWith("\\"))
    result += "\\";
  return result;
#else
#ifdef Q_WS_MAC
  FSRef ref;
  OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &ref);
  if (err)
    return QString();
  QString path;
  QByteArray ba(2048, 0);
  if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
    path = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
  path += QLatin1Char('/') + qApp->applicationName();
  return path;
#else
  QString xdgDataHome = QLatin1String(qgetenv("XDG_DATA_HOME"));
  if (xdgDataHome.isEmpty())
    xdgDataHome = QDir::homePath() + QLatin1String("/.local/share");
  xdgDataHome += QLatin1String("/data/")
      + qApp->applicationName();
  return xdgDataHome;
#endif
#endif
}

QString misc::QDesktopServicesCacheLocation() {
#ifdef Q_WS_WIN
  return QDesktopServicesDataLocation() + QLatin1String("\\cache");
#else
#ifdef Q_WS_MAC
  // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
  FSRef ref;
  OSErr err = FSFindFolder(kUserDomain, kCachedDataFolderType, false, &ref);
  if (err)
    return QString();
  QString path;
  QByteArray ba(2048, 0);
  if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
    path = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
  path += QLatin1Char('/') + qApp->applicationName();
  return path;
#else
  QString xdgCacheHome = QLatin1String(qgetenv("XDG_CACHE_HOME"));
  if (xdgCacheHome.isEmpty())
    xdgCacheHome = QDir::homePath() + QLatin1String("/.cache");
  xdgCacheHome += QLatin1Char('/') + QCoreApplication::applicationName();
  return xdgCacheHome;
#endif
#endif
}

QString misc::QDesktopServicesDownloadLocation() {
#ifdef Q_WS_WIN
  // TODO: Use IKnownFolderManager to get path of FOLDERID_Downloads
  // instead of hardcoding "Downloads"
  // Unfortunately, this would break compatibility with WinXP
  return QDir(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).absoluteFilePath(tr("Downloads"));
#endif

#ifdef Q_WS_X11
  QString save_path;
  // Default save path on Linux
  QString config_path = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME").constData());
  if (config_path.isEmpty())
    config_path = QDir::home().absoluteFilePath(".config");

  QString user_dirs_file = config_path + "/user-dirs.dirs";
  if (QFile::exists(user_dirs_file)) {
    QSettings settings(user_dirs_file, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    QString xdg_download_dir = settings.value("XDG_DOWNLOAD_DIR").toString();
    if (!xdg_download_dir.isEmpty()) {
      // Resolve $HOME environment variables
      xdg_download_dir.replace("$HOME", QDir::homePath());
      save_path = xdg_download_dir;
      save_path += QDir::separator() + "mule";
      qDebug() << Q_FUNC_INFO << "SUCCESS: Using XDG path for downloads: " << save_path;
    }
  }

  // Fallback
  if (!save_path.isEmpty() && !QFile::exists(save_path))
  {
      QDir().mkpath(save_path);
  }

  if (save_path.isEmpty() || !QFile::exists(save_path))
  {
    save_path = QDir::home().absoluteFilePath(tr("Downloads"));
    save_path += QDir::separator() + "mule";
    qDebug() << Q_FUNC_INFO << "using" << save_path << "as fallback since the XDG detection did not work";
  }

  return save_path;
#endif

#ifdef Q_WS_MAC
  // TODO: How to support this on Mac OS X?
#endif

  // Fallback
  qDebug() << Q_FUNC_INFO << " use downloads";
  return QDir::home().absoluteFilePath(tr("Downloads"));
}

long long misc::freeDiskSpaceOnPath(QString path) {
  if (path.isEmpty()) return -1;
  path.replace("\\", "/");
  QDir dir_path(path);
  if (!dir_path.exists()) {
    QStringList parts = path.split("/");
    while (parts.size() > 1 && !QDir(parts.join("/")).exists()) {
      parts.removeLast();
    }
    dir_path = QDir(parts.join("/"));
    if (!dir_path.exists()) return -1;
  }
  Q_ASSERT(dir_path.exists());

#ifndef Q_WS_WIN
  unsigned long long available;
  struct statfs stats;
  const QString statfs_path = dir_path.path()+"/.";
  const int ret = statfs (qPrintable(statfs_path), &stats) ;
  if (ret == 0) {
    available = ((unsigned long long)stats.f_bavail) *
        ((unsigned long long)stats.f_bsize) ;
    return available;
  } else {
    return -1;
  }
#else
  typedef BOOL (WINAPI *GetDiskFreeSpaceEx_t)(LPCTSTR,
                                              PULARGE_INTEGER,
                                              PULARGE_INTEGER,
                                              PULARGE_INTEGER);
  GetDiskFreeSpaceEx_t
      pGetDiskFreeSpaceEx = (GetDiskFreeSpaceEx_t)::GetProcAddress
      (
        ::GetModuleHandle(TEXT("kernel32.dll")),
        "GetDiskFreeSpaceExW"
        );
  if ( pGetDiskFreeSpaceEx )
  {
    ULARGE_INTEGER bytesFree, bytesTotal;
    unsigned long long *ret;
    if (pGetDiskFreeSpaceEx((LPCTSTR)(QDir::toNativeSeparators(dir_path.path())).utf16(), &bytesFree, &bytesTotal, NULL)) {
      ret = (unsigned long long*)&bytesFree;
      return *ret;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
#endif
}

#ifndef DISABLE_GUI
void misc::shutdownComputer(bool sleep) {
#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
  // Use dbus to power off / suspend the system
  if (sleep) {
    // Recent systems use UPower
    QDBusInterface upowerIface("org.freedesktop.UPower", "/org/freedesktop/UPower",
                               "org.freedesktop.UPower", QDBusConnection::systemBus());
    if (upowerIface.isValid()) {
      upowerIface.call("Suspend");
      return;
    }
    // HAL (older systems)
    QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement",
                            QDBusConnection::systemBus());
    halIface.call("Suspend", 5);
  } else {
    // Recent systems use ConsoleKit
    QDBusInterface consolekitIface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                                   "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());
    if (consolekitIface.isValid()) {
      consolekitIface.call("Stop");
      return;
    }
    // HAL (older systems)
    QDBusInterface halIface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement",
                            QDBusConnection::systemBus());
    halIface.call("Shutdown");
  }
#endif
#ifdef Q_WS_MAC
  AEEventID EventToSend;
  if (sleep)
    EventToSend = kAESleep;
  else
    EventToSend = kAEShutDown;
  AEAddressDesc targetDesc;
  static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
  AppleEvent eventReply = {typeNull, NULL};
  AppleEvent appleEventToSend = {typeNull, NULL};

  OSStatus error = noErr;

  error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                       sizeof(kPSNOfSystemProcess), &targetDesc);

  if (error != noErr)
  {
    return;
  }

  error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                             kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

  AEDisposeDesc(&targetDesc);
  if (error != noErr)
  {
    return;
  }

  error = AESend(&appleEventToSend, &eventReply, kAENoReply,
                 kAENormalPriority, kAEDefaultTimeout, NULL, NULL);

  AEDisposeDesc(&appleEventToSend);
  if (error != noErr)
  {
    return;
  }

  AEDisposeDesc(&eventReply);
#endif
#ifdef Q_WS_WIN
  HANDLE hToken;              // handle to process token
  TOKEN_PRIVILEGES tkp;       // pointer to token structure
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    return;
  // Get the LUID for shutdown privilege.
  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                       &tkp.Privileges[0].Luid);

  tkp.PrivilegeCount = 1;  // one privilege to set
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  // Get shutdown privilege for this process.

  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);

  // Cannot test the return value of AdjustTokenPrivileges.

  if (GetLastError() != ERROR_SUCCESS)
    return;

  if (sleep)
    SetSuspendState(false, false, false);
  else
    InitiateSystemShutdownA(0, tr("qMule will shutdown the computer now because all downloads are complete.").toLocal8Bit().data(), 10, true, false);

  // Disable shutdown privilege.
  tkp.Privileges[0].Attributes = 0;
  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES) NULL, 0);
#endif
}
#endif // DISABLE_GUI

QString misc::fixFileNames(QString path) {
  //qDebug() << Q_FUNC_INFO << path;
  path.replace("\\", "/");
  QStringList parts = path.split("/", QString::SkipEmptyParts);
  if (parts.isEmpty()) return path;
  QString last_part = parts.takeLast();
  QList<QString>::iterator it;
  for (it = parts.begin(); it != parts.end(); it++) {
    QByteArray raw_filename = it->toLocal8Bit();
    // Make sure the filename is not too long
    if (raw_filename.size() > MAX_FILENAME_LENGTH) {
      qDebug() << "Folder" << *it << "was cut because it was too long";
      raw_filename.resize(MAX_FILENAME_LENGTH);
      *it = QString::fromLocal8Bit(raw_filename.constData());
      qDebug() << "New folder name is" << *it;
      Q_ASSERT(it->length() == MAX_FILENAME_LENGTH);
    }
  }
  // Fix the last part (file name)
  QByteArray raw_lastPart = last_part.toLocal8Bit();
  qDebug() << "Last part length:" << raw_lastPart.length();
  if (raw_lastPart.length() > MAX_FILENAME_LENGTH) {
    qDebug() << "Filename" << last_part << "was cut because it was too long";
    // Shorten the name, keep the file extension
    int point_index = raw_lastPart.lastIndexOf(".");
    QByteArray extension = "";
    if (point_index >= 0) {
      extension = raw_lastPart.mid(point_index);
      raw_lastPart = raw_lastPart.left(point_index);
    }
    raw_lastPart = raw_lastPart.left(MAX_FILENAME_LENGTH-extension.length()) + extension;
    Q_ASSERT(raw_lastPart.length() == MAX_FILENAME_LENGTH);
    last_part = QString::fromLocal8Bit(raw_lastPart.constData());
    qDebug() << "New file name is" << last_part;
  }
  parts << last_part;
  return parts.join("/");
}

QString misc::truncateRootFolder(boost::intrusive_ptr<torrent_info> t) {
  if (t->num_files() == 1) {
    // Single file torrent
#if LIBTORRENT_VERSION_MINOR > 15
    QString path = QString::fromUtf8(t->file_at(0).path.c_str());
#else
    QString path = QString::fromUtf8(t->file_at(0).path.string().c_str());
#endif
    // Remove possible subfolders
    path = fixFileNames(fileName(path));
    t->rename_file(0, path.toUtf8().data());
    return QString();
  }
  QString root_folder;
  for (int i=0; i<t->num_files(); ++i) {
#if LIBTORRENT_VERSION_MINOR > 15
    QString path = QString::fromUtf8(t->file_at(i).path.c_str());
#else
    QString path = QString::fromUtf8(t->file_at(i).path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    if (path_parts.size() > 1) {
      root_folder = path_parts.takeFirst();
    }
    path = fixFileNames(path_parts.join("/"));
    t->rename_file(i, path.toUtf8().data());
  }
  return root_folder;
}

QString misc::truncateRootFolder(libtorrent::torrent_handle h) {
  torrent_info t = h.get_torrent_info();
  if (t.num_files() == 1) {
    // Single file torrent
    // Remove possible subfolders
#if LIBTORRENT_VERSION_MINOR > 15
    QString path = QString::fromUtf8(t.file_at(0).path.c_str());
#else
    QString path = QString::fromUtf8(t.file_at(0).path.string().c_str());
#endif
    path = fixFileNames(fileName(path));
    t.rename_file(0, path.toUtf8().data());
    return QString();
  }
  QString root_folder;
  for (int i=0; i<t.num_files(); ++i) {
#if LIBTORRENT_VERSION_MINOR > 15
    QString path = QString::fromUtf8(t.file_at(i).path.c_str());
#else
    QString path = QString::fromUtf8(t.file_at(i).path.string().c_str());
#endif
    QStringList path_parts = path.split("/", QString::SkipEmptyParts);
    if (path_parts.size() > 1) {
      root_folder = path_parts.takeFirst();
    }
    path = fixFileNames(path_parts.join("/"));
    h.rename_file(i, path.toUtf8().data());
  }
  return root_folder;
}

bool misc::sameFiles(const QString &path1, const QString &path2) {
  QFile f1(path1), f2(path2);
  if (!f1.exists() || !f2.exists()) return false;
  if (f1.size() != f2.size()) return false;
  if (!f1.open(QIODevice::ReadOnly)) return false;
  if (!f2.open(QIODevice::ReadOnly)) {
    f1.close();
    return false;
  }
  bool same = true;
  while(!f1.atEnd() && !f2.atEnd()) {
    if (f1.read(5) != f2.read(5)) {
      same = false;
      break;
    }
  }
  f1.close(); f2.close();
  return same;
}

QString misc::updateLabelInSavePath(QString defaultSavePath, QString save_path, const QString &old_label, const QString &new_label) {
  if (old_label == new_label) return save_path;
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
  defaultSavePath.replace("\\", "/");
  save_path.replace("\\", "/");
#endif
  qDebug("UpdateLabelInSavePath(%s, %s, %s)", qPrintable(save_path), qPrintable(old_label), qPrintable(new_label));
  if (!save_path.startsWith(defaultSavePath)) return save_path;
  QString new_save_path = save_path;
  new_save_path.replace(defaultSavePath, "");
  QStringList path_parts = new_save_path.split("/", QString::SkipEmptyParts);
  if (path_parts.empty()) {
    if (!new_label.isEmpty())
      path_parts << new_label;
  } else {
    if (old_label.isEmpty() || path_parts.first() != old_label) {
      if (path_parts.first() != new_label)
        path_parts.prepend(new_label);
    } else {
      if (new_label.isEmpty()) {
        path_parts.removeAt(0);
      } else {
        if (path_parts.first() != new_label)
          path_parts.replace(0, new_label);
      }
    }
  }
  new_save_path = defaultSavePath;
  if (!new_save_path.endsWith(QDir::separator())) new_save_path += QDir::separator();
  new_save_path += path_parts.join(QDir::separator());
  qDebug("New save path is %s", qPrintable(new_save_path));
  return new_save_path;
}

QString misc::toValidFileSystemName(QString filename) {
  qDebug("toValidFSName: %s", qPrintable(filename));
  const QRegExp regex("[\\\\/:?\"*<>|]");
  filename.replace(regex, " ");
  qDebug("toValidFSName, result: %s", qPrintable(filename));
  return filename.trimmed();
}

bool misc::isValidFileSystemName(const QString& filename) {
  if (filename.isEmpty()) return false;
  const QRegExp regex("[\\\\/:?\"*<>|]");
  return !filename.contains(regex);
}

#ifndef DISABLE_GUI
// Get screen center
QPoint misc::screenCenter(QWidget *win) {
  int scrn = 0;
  const QWidget *w = win->window();

  if (w)
    scrn = QApplication::desktop()->screenNumber(w);
  else if (QApplication::desktop()->isVirtualDesktop())
    scrn = QApplication::desktop()->screenNumber(QCursor::pos());
  else
    scrn = QApplication::desktop()->screenNumber(win);

  QRect desk(QApplication::desktop()->availableGeometry(scrn));
  return QPoint((desk.width() - win->frameGeometry().width()) / 2, (desk.height() - win->frameGeometry().height()) / 2);
}
#endif

/**
 * Detects the version of python by calling
 * "python --version" and parsing the output.
 */
int misc::pythonVersion() {
  static int version = -1;
  if (version < 0) {
    QProcess python_proc;
    python_proc.start("python", QStringList() << "--version", QIODevice::ReadOnly);
    if (!python_proc.waitForFinished()) return -1;
    if (python_proc.exitCode() < 0) return -1;
    QByteArray output = python_proc.readAllStandardOutput();
    if (output.isEmpty())
      output = python_proc.readAllStandardError();
    const QByteArray version_str = output.split(' ').last();
    qDebug() << "Python version is:" << version_str.trimmed();
    if (version_str.startsWith("3."))
      version = 3;
    else
      version = 2;
  }
  return version;
}

QString misc::searchEngineLocation() {
  QString folder = "nova";
  if (pythonVersion() >= 3)
    folder = "nova3";
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + folder);
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString misc::BTBackupLocation() {
  const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                           + QDir::separator() + "BT_backup");
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

QString misc::ED2KBackupLocation()
{
    const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                               + QDir::separator() + "ED2K_backup");
    QDir locationDir(location);
    if (!locationDir.exists()) locationDir.mkpath(locationDir.absolutePath());
    return location;
}

QString misc::XCatalogCacheLocation()
{
    const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                               + QDir::separator() + "XCatalog");
    QDir locationDir(location);
    if (!locationDir.exists()) locationDir.mkpath(locationDir.absolutePath());
    return location;
}

QString misc::ED2KMetaLocation(const QString& filename)
{
    const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                               + QDir::separator() + "ED2K_meta");

    QDir locationDir(location);
    if (!locationDir.exists()) locationDir.mkpath(locationDir.absolutePath());
    return (locationDir.filePath(filename));
}

QString misc::ED2KCollectionLocation()
{
    const QString location = QDir::cleanPath(QDesktopServicesDataLocation()
                                               + QDir::separator() + "ED2K_collections");
    QDir locationDir(location);
    if (!locationDir.exists()) locationDir.mkpath(locationDir.absolutePath());
    return location;
}

QString misc::cacheLocation() {
  QString location = QDir::cleanPath(QDesktopServicesCacheLocation());
  QDir locationDir(location);
  if (!locationDir.exists())
    locationDir.mkpath(locationDir.absolutePath());
  return location;
}

// return best userfriendly storage unit (B, KiB, MiB, GiB, TiB)
// use Binary prefix standards from IEC 60027-2
// see http://en.wikipedia.org/wiki/Kilobyte
// value must be given in bytes
QString misc::friendlyUnit(qreal val, SizeType type) {
  if (val < 0)
    return tr("Unknown", "Unknown (size)");
  if (type == ST_DEFAULT)
  {
      int i = 0;
      while(val >= 1024. && i++<6)
        val /= 1024.;
      if (i == 0)
        return QString::number((long)val) + " " + tr(units[0].source, units[0].comment);
      return QString::number(val, 'f', 2) + " " + tr(units[i].source, units[i].comment);
  }
  else if (type == ST_KiB)
  {
      val /= 1024.;
      return QString::number(val, 'f', 2) + " " + tr(units[1].source, units[1].comment);
  }
  else
  {
      val /= 1048576.;
      return QString::number(val, 'f', 2) + " " + tr(units[2].source, units[2].comment);
  }
}

bool misc::isPreviewable(QString extension) {
  if (extension.isEmpty()) return false;
  extension = extension.toUpper();
  if (extension == "AVI") return true;
  if (extension == "MP3") return true;
  if (extension == "OGG") return true;
  if (extension == "OGV") return true;
  if (extension == "OGM") return true;
  if (extension == "WMV") return true;
  if (extension == "WMA") return true;
  if (extension == "MPEG") return true;
  if (extension == "MPG") return true;
  if (extension == "ASF") return true;
  if (extension == "QT") return true;
  if (extension == "RM") return true;
  if (extension == "RMVB") return true;
  if (extension == "RMV") return true;
  if (extension == "SWF") return true;
  if (extension == "FLV") return true;
  if (extension == "WAV") return true;
  if (extension == "MOV") return true;
  if (extension == "VOB") return true;
  if (extension == "MID") return true;
  if (extension == "AC3") return true;
  if (extension == "MP4") return true;
  if (extension == "MP2") return true;
  if (extension == "AVI") return true;
  if (extension == "FLAC") return true;
  if (extension == "AU") return true;
  if (extension == "MPE") return true;
  if (extension == "MOV") return true;
  if (extension == "MKV") return true;
  if (extension == "M4V") return true;
  if (extension == "AIF") return true;
  if (extension == "AIFF") return true;
  if (extension == "AIFC") return true;
  if (extension == "RA") return true;
  if (extension == "RAM") return true;
  if (extension == "M4P") return true;
  if (extension == "M4A") return true;
  if (extension == "3GP") return true;
  if (extension == "AAC") return true;
  if (extension == "SWA") return true;
  if (extension == "MPC") return true;
  if (extension == "MPP") return true;
  if (extension == "M3U") return true;
  return false;
}

QString misc::bcLinkToMagnet(QString bc_link) {
  QByteArray raw_bc = bc_link.toUtf8();
  raw_bc = raw_bc.mid(8); // skip bc://bt/
  raw_bc = QByteArray::fromBase64(raw_bc); // Decode base64
  // Format is now AA/url_encoded_filename/size_bytes/info_hash/ZZ
  QStringList parts = QString(raw_bc).split("/");
  if (parts.size() != 5) return QString::null;
  QString filename = parts.at(1);
  QString hash = parts.at(3);
  QString magnet = "magnet:?xt=urn:btih:" + hash;
  magnet += "&dn=" + filename;
  return magnet;
}

QString misc::magnetUriToName(QString magnet_uri) {
  QString name = "";
  QRegExp regHex("dn=([^&]+)");
  const int pos = regHex.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regHex.cap(1);
    // URL decode
    name = QUrl::fromPercentEncoding(found.toLocal8Bit()).replace("+", " ");
  }
  return name;
}

QString misc::magnetUriToHash(QString magnet_uri) {
  QString hash = "";
  QRegExp regHex("urn:btih:([0-9A-Za-z]+)");
  // Hex
  int pos = regHex.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regHex.cap(1);
    qDebug() << Q_FUNC_INFO << "regex found: " << found;
    if (found.length() == 40) {
      const sha1_hash sha1(QByteArray::fromHex(found.toAscii()).constData());
      qDebug("magnetUriToHash (Hex): hash: %s", qPrintable(misc::toQString(sha1)));
      return misc::toQString(sha1);
    }
  }
  // Base 32
  QRegExp regBase32("urn:btih:([A-Za-z2-7=]+)");
  pos = regBase32.indexIn(magnet_uri);
  if (pos > -1) {
    const QString found = regBase32.cap(1);
    if (found.length() > 20 && (found.length()*5)%40 == 0) {
      const sha1_hash sha1(base32decode(regBase32.cap(1).toStdString()));
      hash = misc::toQString(sha1);
    }
  }
  qDebug("magnetUriToHash (base32): hash: %s", qPrintable(hash));
  return hash;
}

// Replace ~ in path
QString misc::expandPath(QString path) {
  path = path.trimmed();
  if (path.isEmpty()) return path;
  if (path.length() == 1) {
    if (path[0] == '~' ) return QDir::homePath();
  }
  if (path[0] == '~' && path[1] == QDir::separator()) {
    path.replace(0, 1, QDir::homePath());
  } else {
    if (QDir::isAbsolutePath(path)) {
      path = QDir(path).absolutePath();
    }
  }
  return QDir::cleanPath(path);
}

// Take a number of seconds and return an user-friendly
// time duration like "1d 2h 10m".
QString misc::userFriendlyDuration(qlonglong seconds, int type) {
    if (!type)
    {
        if (seconds < 0 || seconds >= MAX_ETA) {
            return QString::fromUtf8("∞");
        }
        if (seconds == 0) {
            return "0";
        }
        if (seconds < 60) {
            return tr("< 1m", "< 1 minute");
        }
        int minutes = seconds / 60;
        if (minutes < 60) {
            return tr("%1m","e.g: 10minutes").arg(QString::number(minutes));
        }
        int hours = minutes / 60;
        minutes = minutes - hours*60;
        if (hours < 24) {
            return tr("%1h %2m", "e.g: 3hours 5minutes").arg(QString::number(hours)).arg(QString::number(minutes));
        }
        int days = hours / 24;
        hours = hours - days * 24;
        if (days < 100) {
            return tr("%1d %2h", "e.g: 2days 10hours").arg(QString::number(days)).arg(QString::number(hours));
        }
        return QString::fromUtf8("∞");
    }
    else
    {
        if (!seconds)
            return QString::fromUtf8("");
        int minutes = seconds / 60;
        seconds -= minutes * 60;
        int hours = minutes / 60;
        minutes -= hours * 60;
        QString result = QString::number(hours) + ":";
        if (minutes < 10)
            result += "0";
        result += QString::number(minutes) + ":";
        if (seconds < 10)
            result += "0";
        result += QString::number(seconds);

        return result;
    }
}

QString misc::getUserIDString() {
  QString uid = "0";
#ifdef Q_WS_WIN
  char buffer[UNLEN+1] = {0};
  DWORD buffer_len = UNLEN + 1;  
  if (GetUserNameA(buffer, &buffer_len))
  {
        uid = QString::fromLocal8Bit(buffer,  buffer_len);
  }
#else
  uid = QString::number(getuid());
#endif
  return uid;
}

QString misc::getUserName()
{
#ifdef Q_WS_WIN
    return getUserIDString();
#else
    QString res;
    const char* pchUserName = getenv("USER");
    if (pchUserName)
    {
        res = QString::fromLocal8Bit(pchUserName);
    }

    return res;
#endif
}

QStringList misc::toStringList(const QList<bool> &l) {
  QStringList ret;
  foreach (const bool &b, l) {
    ret << (b ? "1" : "0");
  }
  return ret;
}

QList<int> misc::intListfromStringList(const QStringList &l) {
  QList<int> ret;
  foreach (const QString &s, l) {
    ret << s.toInt();
  }
  return ret;
}

QList<bool> misc::boolListfromStringList(const QStringList &l) {
  QList<bool> ret;
  foreach (const QString &s, l) {
    ret << (s=="1");
  }
  return ret;
}

quint64 misc::computePathSize(QString path)
{
  // Check if it is a file
  QFileInfo fi(path);
  if (!fi.exists()) return 0;
  if (fi.isFile()) return fi.size();
  // Compute folder size
  quint64 size = 0;
  foreach (const QFileInfo &subfi, QDir(path).entryInfoList(QDir::Dirs|QDir::Files)) {
    if (subfi.fileName().startsWith(".")) continue;
    if (subfi.isDir())
      size += misc::computePathSize(subfi.absoluteFilePath());
    else
      size += subfi.size();
  }
  return size;
}

bool misc::isValidTorrentFile(const QString &torrent_path) {
  try {
    boost::intrusive_ptr<libtorrent::torrent_info> t = new torrent_info(torrent_path.toUtf8().constData());
    if (!t->is_valid() || t->num_files() == 0)
      throw std::exception();
  } catch(std::exception&) {
    return false;
  }
  return true;
}

QSet<QString> misc::torrentRoots(const QTorrentHandle& h)
{
    QSet<QString> roots;

    if (h.has_metadata())
    {
        QDir save_path(h.save_path());
        if (save_path.dirName() == h.name())
        {
            roots << save_path.absolutePath();
        }
        else
        {
            int num_files = h.num_files();
            for (int i = 0; i < num_files; ++i)
                roots << save_path.filePath(h.filepath_at(i).split(QDir::separator()).first());
        }
    }

    return roots;
}

QDir misc::uniquePath(const QDir& path, const QList<QDir>& paths)
{
    QDir result = path;
    for (int i = 1; paths.contains(result); ++i)
    {
        QFileInfo info(path.absolutePath());
        result = info.dir().absoluteFilePath(
            info.baseName() + QString("(%1).").arg(i) + info.completeSuffix());
    }

    return result;
}

/**
 * Returns a path constructed from all the elements of file_path except the last.
 * A typical use is to obtain the parent path for a path supplied by the user.
 */
QString misc::branchPath(QString file_path, bool uses_slashes)
{
  if (!uses_slashes)
    file_path.replace("\\", "/");
  Q_ASSERT(!file_path.contains("\\"));
  if (file_path.endsWith("/"))
    file_path.chop(1); // Remove trailing slash
  qDebug() << Q_FUNC_INFO << "before:" << file_path;
  if (file_path.contains("/"))
    return file_path.left(file_path.lastIndexOf('/'));
  return "";
}

bool misc::isUrl(const QString &s)
{
  const QString scheme = QUrl(s).scheme();
  QRegExp is_url("http[s]?|ftp", Qt::CaseInsensitive);
  return is_url.exactMatch(scheme);
}

QString misc::fileName(QString file_path)
{
  file_path.replace("\\", "/");
  const int slash_index = file_path.lastIndexOf('/');
  if (slash_index == -1)
    return file_path;
  return file_path.mid(slash_index+1);
}

QString misc::parseHtmlLinks(const QString &raw_text)
{
  QString result = raw_text;
  QRegExp reURL("(\\s|^)"                                     //start with whitespace or beginning of line
                "("
                "("                                      //case 1 -- URL with scheme
                "(http(s?))\\://"                    //start with scheme
                "([a-zA-Z0-9_-]+\\.)+"               //  domainpart.  at least one of these must exist
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"      //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
                ")"
                "|"
                "("                                     //case 2a -- no scheme, contains common TLD  example.com
                "([a-zA-Z0-9_-]+\\.)+"              //  domainpart.  at least one of these must exist
                "(?="                               //  must be followed by TLD
                "AERO|aero|"                  //N.B. assertions are non-capturing
                "ARPA|arpa|"
                "ASIA|asia|"
                "BIZ|biz|"
                "CAT|cat|"
                "COM|com|"
                "COOP|coop|"
                "EDU|edu|"
                "GOV|gov|"
                "INFO|info|"
                "INT|int|"
                "JOBS|jobs|"
                "MIL|mil|"
                "MOBI|mobi|"
                "MUSEUM|museum|"
                "NAME|name|"
                "NET|net|"
                "ORG|org|"
                "PRO|pro|"
                "RO|ro|"
                "RU|ru|"
                "TEL|tel|"
                "TRAVEL|travel"
                ")"
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]+)"     //  everything to 1st non-URI char, must be at least one char after the previous dot (cannot use ".*" because it can be too greedy)
                ")"
                "|"
                "("                                     // case 2b no scheme, no TLD, must have at least 2 aphanum strings plus uncommon TLD string  --> del.icio.us
                "([a-zA-Z0-9_-]+\\.) {2,}"           //2 or more domainpart.   --> del.icio.
                "[a-zA-Z]{2,}"                      //one ab  (2 char or longer) --> us
                "([a-zA-Z0-9\\?%=&/_\\.:#;-]*)"     // everything to 1st non-URI char, maybe nothing  in case of del.icio.us/path
                ")"
                ")"
                );


  // Capture links
  result.replace(reURL, "\\1<a href=\"\\2\">\\2</a>");

  // Capture links without scheme
  QRegExp reNoScheme("<a\\s+href=\"(?!http(s?))([a-zA-Z0-9\\?%=&/_\\.-:#]+)\\s*\">");
  result.replace(reNoScheme, "<a href=\"http://\\1\">");

  return result;
}


QString misc::ifaceFromHumanName(const QString& strHumanIface)
{
    QString strRes;

    if (strHumanIface.isEmpty()) return strRes;

    foreach (const QNetworkInterface& iface, QNetworkInterface::allInterfaces())
    {
        if ((iface.flags() & QNetworkInterface::IsLoopBack) ||
            (!iface.isValid()) ||
            ((iface.flags() & QNetworkInterface::IsUp) == 0))
                continue;

        if (strHumanIface == iface.humanReadableName())
        {
            strRes = iface.name();
        }
    }

    return strRes;
}

QStringList misc::getFileLines(const QString& filename, const char* codec /*= NULL*/)
{
    QStringList slist;
    QFile textFile(filename);

    if (!textFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return slist;
    }

    QTextStream textStream(&textFile);
    if (codec) textStream.setCodec(codec);
    textStream.setAutoDetectUnicode(true);

    while (true)
    {
        QString line = textStream.readLine();
        if (line.isNull()) break;
        slist.append(line);
    }

    return slist;
}

QStringList misc::cmd2list(const QString& text)
{
    QStringList res;
    qDebug() << "cmd2list: " << text;
    QRegExp rx("((?:[^\\s\"]+)|(?:\"(?:\\\\\"|[^\"])*\"))");
    int pos = 0;

    while ((pos = rx.indexIn(text, pos)) != -1)
    {
        QString s = rx.cap(1);
        s.replace(QRegExp("^\""), "");
        s.replace(QRegExp("\"$"), "");
        res << s;
        pos += rx.matchedLength();
    }

    return res;
}


#ifdef Q_WS_WIN32

QString ShellGetFolderPath(int iCSIDL)
{
    QString str;
    TCHAR szPath[MAX_PATH];

    if ( SHGetFolderPath(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, szPath) == S_OK )
        str = QString::fromWCharArray(szPath);

    return str;
}

QString misc::emuleConfig(const QString& filename)
{
    qDebug() << "emule config for " << filename;
    QString res;
    static QList<QDir> dl = QList<QDir>()
            << QDir(ShellGetFolderPath(CSIDL_LOCAL_APPDATA)).filePath("eMule IS Mod\\config")
            << QDir(ShellGetFolderPath(CSIDL_APPDATA)).filePath("eMule IS Mod\\config")
            << QDir(ShellGetFolderPath(CSIDL_PERSONAL)).filePath("eMule IS Mod\\config");

    QList<QDir>::iterator itr = std::find_if(dl.begin(), dl.end(), std::mem_fun_ref(static_cast<bool (QDir::*)() const>(&QDir::exists)));

    if (itr != dl.end())
    {
        res = (*itr).filePath(filename);
    }

    qDebug() << "emule config " << res;
    return res;
}

QString misc::emuleConfigFilename()
{
    return QString("preferences.ini");
}

QStringList misc::emuleSharedFiles()
{
    return getFileLines(emuleConfig("sharedfiles.dat"),  "UTF-16");
}

QStringList misc::emuleSharedDirs()
{
    return getFileLines(emuleConfig("shareddir.dat"), "UTF-16");
}

QString misc::emuleKeyFile()
{
    QString filename = emuleConfig(getUserName() + QString(".rnd"));

    if (!QFile::exists(filename))
    {
        filename.clear();
    }

    return (filename);
}

QString misc::migrationAuthLogin()
{
    QSettings qs(QString("HKEY_CURRENT_USER\\Software\\eMule IS Mod"), QSettings::NativeFormat);
    return qs.value("AuthLogin", QString("")).toString();
}

shared_map misc::migrationShareds()
{
    shared_map se;
    QStringList minus_f =
            emuleSharedFiles().filter(QRegExp("^-")).replaceInStrings(QRegExp("^-"), "");
    QDir dir;

    QStringList dirs = emuleSharedDirs();
    qDebug() << dirs;
    foreach(dir, dirs)
    {
        shared_map::iterator itr = se.insert(dir.path(), QList<QString>());
        QFileInfo fi;

        foreach(fi, minus_f)
        {
            qDebug() << "dir: " << dir.path() << " : " << fi.dir().path();
            if (fi.exists() && (dir == fi.dir()))
            {
                itr.value().append(fi.fileName());
            }
        }
    }

    return se;
}

QStringList misc::migrationSharedFiles()
{
    return emuleSharedFiles().filter(QRegExp("^[^-]"));
}

void misc::migrateTorrents()
{
    qDebug() << Q_FUNC_INFO;

    // generate locations list
    QString res;
    QList<QFileInfo> dl;
    dl << QFileInfo(QDir(ShellGetFolderPath(CSIDL_LOCAL_APPDATA)).filePath("uTorrent\\resume.dat"))
    << QFileInfo(QDir(ShellGetFolderPath(CSIDL_APPDATA)).filePath("uTorrent\\resume.dat"))
    << QFileInfo(QDir(ShellGetFolderPath(CSIDL_LOCAL_APPDATA)).filePath("BitTorrent\\resume.dat"))
    << QFileInfo(QDir(ShellGetFolderPath(CSIDL_APPDATA)).filePath("BitTorrent\\resume.dat"));

    dl.erase(std::remove_if(dl.begin(), dl.end(),
        std::not1(std::mem_fun_ref(static_cast<bool (QFileInfo::*)() const>(&QFileInfo::exists)))), dl.end());

    foreach(QFileInfo fi, dl)
    {
        qDebug() << "process " << fi.filePath();
        std::vector<char> buf;
        error_code ec;
        int ret = load_file(fi.filePath().toUtf8().constData(), buf, ec);

        if (ret != 0)
        {
            qDebug() << "error on load_file";
            continue;
        }

        lazy_entry e;

        if (buf.size() == 0 || lazy_bdecode(&buf[0], &buf[0] + buf.size(), e) != 0)
        {
            qDebug() << "error on decode";
            continue;
        }

        if (e.type() != lazy_entry::dict_t)
        {
            qDebug() << "type error";
            continue;
        }

        for (int n = 0; n < e.dict_size(); ++n)
        {
            std::pair<std::string, lazy_entry const*> dic_elem = e.dict_at(n);

            if (dic_elem.second->type() != lazy_entry::dict_t)
            {
                qDebug() << "type error on dict";
                continue;
            }

            lazy_entry const* dict = dic_elem.second;
            int completed = dict->dict_find_int_value("completed_on");

            /**
              * torrentSavePath = save path for torrent when torrent contains more than one file
              * torrentSavePath = file name for torrent when torrent contains only one file
             */
            QString torrentSavePath = QString::fromUtf8(dict->dict_find_string_value("path").c_str());
            QString torrentFileName = QString::fromUtf8(dic_elem.first.c_str());
            qDebug() << "Torrent save path " << torrentSavePath
                     << " file path " << torrentFileName;

            // generate torrent file name
            if (!QFileInfo(torrentFileName).exists())
            {
                torrentFileName = fi.dir().filePath(torrentFileName);

                if (!QFileInfo(torrentFileName).exists())
                {
                    torrentFileName.clear();
                }
            }

            if (torrentFileName.isEmpty())
            {
                qDebug() << "torrent file name is empty";
                continue;
            }

            // load torrent file
            QFile ti_file(torrentFileName);
            if (!ti_file.open(QIODevice::ReadOnly)) continue;
            const QByteArray content = ti_file.readAll();
            ti_file.close();
            error_code ec;
            torrent_info ti(content.constData(), content.size(), ec);

            if (ec || !ti.is_valid())
            {
                qDebug() << "error on load temp torrent file";
                continue;
            }

            // store save path to migration map
            QString hash = misc::toQString(ti.info_hash());
            // for one file extract directory because it is filename
            if (ti.num_files() == 1)
            {
                torrentSavePath = QFileInfo(torrentSavePath).dir().path();
            }

            qDebug() << "save path is : " << torrentSavePath;
            TorrentTempData::setSavePath(hash, torrentSavePath);
            TorrentTempData::setSeedingMode(hash, completed > 0);
			
            std::string prio = dict->dict_find_string_value("prio");
            std::vector<char> priorities(prio.begin(), prio.end());
            std::vector<int> file_priorities;
            for (std::vector<char>::const_iterator iter = priorities.begin(); iter != priorities.end(); ++iter)
            {
                if (*iter == 128 || *iter == 0)
                    file_priorities.push_back(0);
                else
                    file_priorities.push_back(1);
            }
			TorrentTempData::setFilesPriority(hash, file_priorities);

            qDebug() << "add torrent to session " <<  torrentFileName;
            Session::instance()->addTransferFromFile(torrentFileName);
        }
    }
}

#else

QString misc::emuleConfigFilename()
{
    return QString("amule.conf");
}

QString misc::emuleConfig(const QString& filename)
{
    return QDir(QDir::home().filePath(".aMule")).filePath(filename);
}

QString misc::migrationAuthLogin()
{
    return QString();
}

shared_map misc::migrationShareds()
{
    return shared_map();
}

 QStringList misc::migrationSharedFiles()
 {
     return QStringList();
 }

 void misc::migrateTorrents()
 {

 }

#endif

 int misc::migrationPort(int port)
 {
     QSettings qs(emuleConfig(emuleConfigFilename()), QSettings::IniFormat);
     return qs.value("eMule/Port", port).toInt();
 }

 QString misc::migrateValue(const QString& value, const QString& def, const char* codepage /* = NULL*/)
 {
     QString res = def;
     QStringList sl = getFileLines(emuleConfig(emuleConfigFilename()), codepage).filter(QRegExp(QString("^") + value + "="));

     foreach(const QString& s, sl)
     {
         QStringList sres = s.split(QRegExp("="));

         if (sres.size() > 1)
         {
             res = sres[1];
             break;
         }
     }

     return res;
 }
