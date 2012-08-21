#include <QDateTime>
#include <QNetworkInterface>

#include "session_base.h"
#include "misc.h"

const qreal SessionBase::MAX_RATIO = 9999.;

void SessionBase::pauseTransfer(const QString& hash)
{
    Transfer h = getTransfer(hash);
    if (h.is_valid() && !h.is_paused()) {
        h.pause();
    }
}

void SessionBase::resumeTransfer(const QString &hash)
{
    Transfer h = getTransfer(hash);
    if (h.is_valid() && h.is_paused()) {
        h.resume();
    }
}

void SessionBase::pauseAllTransfers()
{
    std::vector<Transfer> torrents = getTransfers();
    std::vector<Transfer>::iterator torrentIT;
    for (torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        if (torrentIT->is_valid() && !torrentIT->is_paused()) {
            torrentIT->pause();
        }
    }
}

void SessionBase::resumeAllTransfers()
{
    std::vector<Transfer> torrents = getTransfers();
    std::vector<Transfer>::iterator torrentIT;
    for (torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        if (torrentIT->is_valid() && torrentIT->is_paused()) {
            torrentIT->resume();
        }
    }
}

qreal SessionBase::getRealRatio(const QString &hash) const
{
    Transfer h = getTransfer(hash);
    if (!h.is_valid()) {
        return 0.;
    }

    libtorrent::size_type all_time_upload = h.all_time_upload();
    libtorrent::size_type all_time_download = h.all_time_download();
    if (all_time_download == 0 && h.is_seed()) {
        // Purely seeded transfer
        all_time_download = h.total_done();
    }

    if (all_time_download == 0) {
        if (all_time_upload == 0)
            return 0;
        return MAX_RATIO+1;
    }

    qreal ratio = all_time_upload / (float) all_time_download;
    Q_ASSERT(ratio >= 0.);
    if (ratio > MAX_RATIO)
        ratio = MAX_RATIO;
    return ratio;
}

bool SessionBase::hasActiveTransfers() const
{
    std::vector<Transfer> torrents = getTransfers();
    std::vector<Transfer>::iterator torrentIT;
    for (torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        const Transfer h(*torrentIT);
        if (h.is_valid() && !h.is_seed() && !h.is_paused() && !h.is_queued() )
            return true;
    }
    return false;
}

void SessionBase::addConsoleMessage(QString msg, QColor color/*=QApplication::palette().color(QPalette::WindowText)*/)
{
    if (consoleMessages.size() > MAX_LOG_MESSAGES)
    {
        consoleMessages.removeFirst();
    }

    msg = "<font color='grey'>"+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + "</font> - <font color='" + color.name() + "'><i>" + msg + "</i></font>";
    consoleMessages.append(msg);
    emit newConsoleMessage(msg);
}

bool SessionBase::isFilePreviewPossible(const QString& hash) const
{
    // See if there are supported files in the transfer
    const Transfer h = getTransfer(hash);
    if (!h.is_valid() || !h.has_metadata()) {
        return false;
    }
    const unsigned int nbFiles = h.num_files();
    for (unsigned int i=0; i<nbFiles; ++i) {
        const QString extension = misc::file_extension(h.filename_at(i));
        if (misc::isPreviewable(extension))
            return true;
    }
    return false;
}

float SessionBase::progress() const
{
    std::vector<Transfer> v = getTransfers();
    v.erase(std::remove_if(v.begin(), v.end(), std::not1(std::mem_fun_ref(&Transfer::is_valid))), v.end());    

    float min_progress = v.empty()?0:1;

    for(std::vector<Transfer>::const_iterator itr = v.begin(); itr != v.end(); ++itr)
    {
        min_progress = std::min(min_progress, itr->progress());
    }

    return (min_progress);
}
