#ifndef CMDHANDLERFILE_H
#define CMDHANDLERFILE_H

#include "cmdhandlerbase.h"
#include <QList>
#include <QMap>
#include <QTcpSocket>

class CmdHandlerFile : public QSimpleCmdHandlerBase
{
    Q_OBJECT
public:
    explicit CmdHandlerFile(QObject *parent = nullptr);
    virtual void StartCmd(SimpleCmdData *pCmd, QVariantList params);
    void SendRemoteCmd(QString Cmd);
    void SetCurrIterLine(QStringList::iterator iterCurrLine);
    QStringList::iterator GetNextIterLine();
signals:
    void cmdFinish();
    void kill(int exitCode);

public slots:

private:
    void SelectSocket(QTcpSocket *pSocket);
    void OnReceive();
    void OnDisconnect [[noreturn]] ();

    QList<QTcpSocket*> m_listSockets;
    QTcpSocket *m_pCurrSocket;
    QStringList::iterator m_iterCurrLine;
    QStringList::iterator m_iterNextLine;
    QMap<QString, QStringList::iterator> m_MapTags;
    QMap<QStringList::iterator, int /*loop count*/> m_MapActiveLoops;
    bool m_bStopOnExternalError;
    QString m_strLastReceivedExternal;
};

#endif // CMDHANDLERFILE_H
