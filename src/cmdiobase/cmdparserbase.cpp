#include "cmdparserbase.h"
#include "cmdhandlerbase.h"
#include <QCoreApplication>

SimpleCmdData::SimpleCmdData(QString strDisplay, CmdParamTypeIdList ListParams, int CmdID, bool bLogCmd) :
    m_strDisplay(strDisplay), m_ListParams(ListParams), m_CmdID(CmdID), m_bLogCmd(bLogCmd)
{
}

int SimpleCmdData::GetCmdID()
{
    return m_CmdID;
}

QString SimpleCmdData::GetDisplayStr()
{
    return m_strDisplay;
}

bool QSimpleCmdParserBase::m_bLogCmdGlobal = false;

QSimpleCmdParserBase::QSimpleCmdParserBase(QObject *parent) :
    QObject(parent), m_pCurrCmd(Q_NULLPTR), m_pCookie(Q_NULLPTR), m_pCmdHandler(Q_NULLPTR), m_bLogCmd(m_bLogCmdGlobal)
{
}

void QSimpleCmdParserBase::SetCmdHandler(QSimpleCmdHandlerBase* pCmdHandler)
{
    m_pCmdHandler = pCmdHandler;
    connect(m_pCmdHandler, &QSimpleCmdHandlerBase::OperationFinish, this, &QSimpleCmdParserBase::OnOperationFinish, Qt::QueuedConnection);
}

void QSimpleCmdParserBase::SetGlobalOutOfOrderMessage(QString strMessage)
{
    m_strGlobalOutOfOrderMessage = strMessage;
}

bool QSimpleCmdParserBase::GetCmdLog()
{
    return m_bLogCmdGlobal && m_bLogCmd;
}

void QSimpleCmdParserBase::SetCmdLogGlobal(bool bLogCmdGlobal)
{
    m_bLogCmdGlobal = bLogCmdGlobal;
}


/**
  @b Setup command info lists required for proper parsing
  @param strCmd [in] Command name
  @param paramList [in] list of expected parameter types
  @param iCmdID [in] ID of decoded command
  */
void QSimpleCmdParserBase::AddCmdInfo(QString strCmd, CmdParamTypeIdList paramList, int iCmdID, bool bLogCmd)
{
    QString strCmdUpp = strCmd.toUpper();
    if(!m_cmdList.contains(strCmdUpp))
    {
        m_cmdList[strCmdUpp] = new SimpleCmdData(
                    strCmd,
                    paramList,
                    iCmdID,
                    bLogCmd);
    }
    else
        qFatal("Cmd \"%s\" already inserted!", qPrintable(strCmd));
}

const QString QSimpleCmdParserBase::PlausiCheck(SimpleCmdData *pCmd, const QVariantList& params)
{
    Q_UNUSED( pCmd )
    Q_UNUSED( params )
    return QString();
}

/**
  @b Parse and start a command. If parsing succeeds our handler's StartCmd is called
  @param strCmd [in] Command received from whatever interface
  @param pCookie [in] Generic pointer wich will be part of the
  @returns (Error)Message - if empty we expended end on signal CmdFinish
  */
const QString QSimpleCmdParserBase::ParseAndStartCmd(QString strCmd, QIODevice *pCookie)
{
    QString strRet;
    QStringList strCmdAndParamList = strCmd.split(QLatin1String(","));
    m_bLogCmd = true;
    if(strCmdAndParamList.count() > 0)
    {
        QString strCmdUpp = strCmdAndParamList[0].toUpper().trimmed();
        if(m_cmdList.contains(strCmdUpp))
        {
            SimpleCmdData *cmdData = m_cmdList[strCmdUpp];
            // Check parameters
            if(strCmdAndParamList.count() == cmdData->m_ListParams.count() + 1)
            {
                // First trim numeric
                for(int iParam=0; iParam<cmdData->m_ListParams.count();iParam++)
                {
                    if( cmdData->m_ListParams[iParam] == PARAM_TYPE_BOOL ||
                        cmdData->m_ListParams[iParam] == PARAM_TYPE_INT ||
                        cmdData->m_ListParams[iParam] == PARAM_TYPE_FLOAT )
                        strCmdAndParamList[iParam+1] = strCmdAndParamList[iParam+1].trimmed();
                }
                QVariantList params;
                bool bParamFormatOK = true;
                for(int iParam=0; iParam<cmdData->m_ListParams.count();iParam++)
                {
                    QVariant param;
                    QString strParam = strCmdAndParamList[iParam+1];
                    switch(cmdData->m_ListParams[iParam])
                    {
                    case PARAM_TYPE_BOOL:
                        if(strParam == QLatin1String("0"))
                            param = QVariant(false);
                        else if(strParam == QLatin1String("1"))
                            param = QVariant(true);
                        else
                            bParamFormatOK = false;
                        break;
                    case PARAM_TYPE_INT:
                    {
                        int iVal = strParam.toInt(&bParamFormatOK);
                        param = QVariant(iVal);
                        break;
                    }
                    case PARAM_TYPE_FLOAT:
                    {
                        double dblVal = strParam.toDouble(&bParamFormatOK);
                        param = QVariant(dblVal);
                        break;
                    }
                    case PARAM_TYPE_STRING:
                        param = QVariant(strParam);
                        break;
                    }
                    params.append(param);
                }

                if(bParamFormatOK)
                {
                    // Are we busy?
                    if(m_pCurrCmd != Q_NULLPTR)
                        strRet = FormatErrorMsg(cmdData->m_strDisplay, "Cannot start while command \"" + m_pCurrCmd->m_strDisplay +"\"is busy!");
                    else
                    {
                        strRet = PlausiCheck(cmdData, params);
                        if(strRet.isEmpty())
                        {
                            // Command itself is OK - is there a global bad (e.g device not open)?
                            if(m_strGlobalOutOfOrderMessage.isEmpty())
                            {
                                m_pCurrCmd = cmdData;
                                m_bLogCmd = m_pCurrCmd->m_bLogCmd;
                                m_pCookie = pCookie;
                                m_pCmdHandler->StartCmd(cmdData, params);
                            }
                            else
                                strRet = FormatErrorMsg(cmdData->m_strDisplay, m_strGlobalOutOfOrderMessage);
                        }
                    }
                }
                else
                    strRet = FormatErrorMsg(cmdData->m_strDisplay , QLatin1String("Wrong parameter format"));
            }
            else
                strRet = FormatErrorMsg(cmdData->m_strDisplay , QLatin1String("Wrong number of parameters"));
        }
        // command not found
        else
            strRet = FormatErrorMsg(strCmdUpp, QLatin1String("Unknown command: \"") + strCmd +QLatin1String("\""));
    }
    return strRet;
}

/**
  @b Fire final success notification here
  @param strReturn [in] In case the transaction reads data it is set here as ','-separated else error message
  */
void QSimpleCmdParserBase::OnOperationFinish(bool bError, QString strReturn)
{
    QString strEmit;
    if(m_pCurrCmd)
        strEmit += m_pCurrCmd->m_strDisplay;
    if(!bError)
        strEmit += QString(QLatin1String(",OK"));
    else
        strEmit += QString(QLatin1String(",ERROR"));
    if(!strReturn.isEmpty())
        strEmit += QString(QLatin1String(",")) + strReturn;

    QIODevice* pCookie = m_pCookie;
    m_pCurrCmd = Q_NULLPTR;
    m_pCookie = Q_NULLPTR;
    emit CmdFinish(strEmit, pCookie);
}

/**
  @b Build error message
  @param strCmd [in] Name of command
  @param strErrDescription [in] Human readable error description
  */
const QString QSimpleCmdParserBase::FormatErrorMsg(QString strCmd, QString strErrDescription)
{
    return strCmd + QLatin1String(",ERROR: ") + strErrDescription;
}


/**
  @b Check valid Hex addrress
  @param strParam [in] Hex address value
  @param iMaxLen [in] maximum length of address
  @returns true if valid
  */
bool QSimpleCmdParserBase::isValidHexValue(QString strParam, int iMaxLen)
{
    bool bValidAddress = true;
    // length
    if((strParam.length() % 2) || strParam.length() > iMaxLen || strParam.length() <= 0)
        bValidAddress = false;
    // correct hex
    else
    {
        for(int iByte=0; iByte<strParam.length()/2 && bValidAddress; iByte++)
            strParam.mid(iByte*2, 2).toInt(&bValidAddress, 16);
    }
    return bValidAddress;
}

QSimpleCmdParserSocketBase::QSimpleCmdParserSocketBase(QObject *parent) :
    QSimpleCmdParserBase(parent), m_ui16IPPort(0)
{
}


void QSimpleCmdParserSocketBase::SetListenPort(quint16 ui16IPPort)
{
    m_ui16IPPort = ui16IPPort;
}

quint16 QSimpleCmdParserSocketBase::GetListenPort()
{
    return m_ui16IPPort;
}