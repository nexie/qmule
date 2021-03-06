#include <QDateTime>
#include <libed2k/util.hpp>
#include "status_widget.h"

status_widget::status_widget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    m_nFiles = 0;
    m_nUsers = 0;

    setDisconnectedInfo();

    QString htmlClub = "<a href='http://city.is74.ru/forum/forumdisplay.php?f=134'>" + tr("Visit oslovedy club") + "</a> <br><br>";
    editServerInfo->insertHtml(htmlClub);
}

status_widget::~status_widget()
{

}

void status_widget::addLogMessage(QString log_message)
{
    QDateTime date_time = QDateTime::currentDateTime();
    QString msg = date_time.toString("dd.MM.yyyy hh:mm:ss") + ": " + log_message;
    editJournal->appendHtml(msg);
}

void status_widget::addHtmlLogMessage(const QString& msg)
{
    editJournal->appendHtml(msg);
}

void status_widget::setDisconnectedInfo()
{
    editInfo->clear();
    QTextCharFormat charFormat = editInfo->currentCharFormat();
    QTextCharFormat charFormatBold = charFormat;
    charFormatBold.setFontWeight(QFont::Bold);
    editInfo->setCurrentCharFormat(charFormatBold);
    editInfo->appendPlainText(tr("eD2K Network"));
    editInfo->setCurrentCharFormat(charFormat);
    editInfo->appendPlainText(tr("Status:") + "\t" + tr("disconnected"));    
}

void status_widget::updateConnectedInfo()
{
    editInfo->clear();
    QTextCharFormat charFormat = editInfo->currentCharFormat();
    QTextCharFormat charFormatBold = charFormat;
    charFormatBold.setFontWeight(QFont::Bold);

    editInfo->setCurrentCharFormat(charFormatBold);
    editInfo->appendPlainText(tr("eD2K Network"));
    editInfo->setCurrentCharFormat(charFormat);
    editInfo->appendPlainText(tr("Status:") + "\t" + tr("connected"));
    editInfo->appendPlainText(tr("IP:Port:") + "\t");
    QString num;
    num.setNum(m_nClientId);
    editInfo->appendPlainText("ID:\t" + num);
    editInfo->appendPlainText(libed2k::isLowId(m_nClientId)?"\tLow ID":"\tHigh ID");

    editInfo->setCurrentCharFormat(charFormatBold);
    editInfo->appendPlainText("\n" + tr("eD2K Server"));
    editInfo->setCurrentCharFormat(charFormat);
    editInfo->appendPlainText(tr("Name:") + "\temule.is74.ru");
    editInfo->appendPlainText(tr("Description:"));
    editInfo->appendPlainText(tr("IP:Port:") + "\t" + m_strServer);
    num.setNum(m_nUsers);
    editInfo->appendPlainText(tr("Clients:") + "\t" + num);
    num.setNum(m_nFiles);
    editInfo->appendPlainText(tr("Files:") + "\t" + num);
}

void status_widget::serverAddress(QString strServer)
{
    m_strServer = strServer;
    updateConnectedInfo();

    QTextCharFormat charFormat = editServerInfo->currentCharFormat();
    QTextCharFormat charFormatBlue = charFormat;
    charFormatBlue.setForeground(QBrush(QColor(Qt::blue)));
    editServerInfo->setCurrentCharFormat(charFormatBlue);
    QDateTime date_time = QDateTime::currentDateTime();
    QString msg = date_time.toString("dd.MM.yyyy hh:mm:ss") + ": " + tr("Connection is established with: ") + "emule.is74.ru (" + strServer + ")";
    editServerInfo->insertPlainText("\n");
    editServerInfo->insertPlainText(msg);
    editServerInfo->setCurrentCharFormat(charFormat);
}

void status_widget::clientID(unsigned int nClientId)
{
    m_nClientId = nClientId;
    updateConnectedInfo();
}

void status_widget::serverInfo(QString strInfo)
{
    editServerInfo->insertPlainText("\n");
    editServerInfo->insertPlainText(strInfo);
}

void status_widget::serverStatus(int nFiles, int nUsers)
{
    m_nFiles = nFiles;
    m_nUsers = nUsers;
    updateConnectedInfo();
}
