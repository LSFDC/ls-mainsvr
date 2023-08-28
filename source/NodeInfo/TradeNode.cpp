#include "../stdafx.h"
//#include "../Window.h"

#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"

#include "../EtcHelpFunc.h"

#include "TradeNodeManager.h"
#include "TradeNode.h"

TradeNode::TradeNode()
{
	InitData();
}

TradeNode::~TradeNode()
{	
}

void TradeNode::InitData()
{
	m_dwTradeIndex = 0;
	m_dwRegisterUserIndex = 0;

	m_dwItemType = 0;
	m_dwItemMagicCode = 0;
	m_dwItemValue = 0;
	m_dwItemMaleCustom = 0;
	m_dwItemFemaleCustom = 0;
	m_iItemPrice = 0;

	m_dwRegisterDate1 = 0;
	m_dwRegisterDate2 = 0;
	m_dwRegisterPeriod = 0;

	m_iPriorityOrder = 0;

	m_szRegisterUserNick.Clear();
	m_szRegisterIP.Clear();
}

void TradeNode::CreateTradeItem( SP2Packet &rkPacket )
{
	rkPacket >> m_dwTradeIndex >> m_dwRegisterUserIndex >> m_szRegisterUserNick;
	rkPacket >> m_dwItemType >> m_dwItemMagicCode >> m_dwItemValue >> m_dwItemMaleCustom >> m_dwItemFemaleCustom >> m_iItemPrice;
	rkPacket >> m_dwRegisterPeriod >> m_dwRegisterDate1 >> m_dwRegisterDate2;
}

void TradeNode::CreateTradeItem( CQueryResultData *pQueryData )
{
	// �⺻ ����
	if(!pQueryData->GetValue( m_dwTradeIndex ))				return;	// �ŷ�ǰ �ε���
	if(!pQueryData->GetValue( m_dwRegisterUserIndex ))		return;	// ����� �ε���
	if(!pQueryData->GetValue( m_szRegisterUserNick, ID_NUM_PLUS_ONE ))		return;	// ����� �г���
	if(!pQueryData->GetValue( m_dwItemType ))				return;	// �ŷ�ǰ Ÿ��
	if(!pQueryData->GetValue( m_dwItemMagicCode ))			return;	// �ŷ�ǰ ������
	if(!pQueryData->GetValue( m_dwItemValue ))				return;	// �ŷ�ǰ value
	if(!pQueryData->GetValue( m_iItemPrice ))				return;	// �ŷ� ����
	if(!pQueryData->GetValue( m_dwItemMaleCustom ))			return;	// �ŷ�ǰ ���� ġ��
	if(!pQueryData->GetValue( m_dwItemFemaleCustom ))		return;	// �ŷ�ǰ ���� ġ��

	DBTIMESTAMP dts;
	if(!pQueryData->GetValue( m_dwRegisterPeriod ))			return; // ��� �Ⱓ
	if(!pQueryData->GetValue( (char*)&dts, sizeof(DBTIMESTAMP) ))			return;	// ��� ��¥


	CTime kRegisterDate(Help::GetSafeValueForCTimeConstructor(dts.year,dts.month,dts.day, dts.hour,dts.minute,dts.second));
	SetRegisterDate( kRegisterDate.GetYear(),
					 kRegisterDate.GetMonth(),
					 kRegisterDate.GetDay(),
					 kRegisterDate.GetHour(),
					 kRegisterDate.GetMinute(),
					 kRegisterDate.GetSecond() );
}

void TradeNode::SetRegisterDate( int iYear , int iMonth, int iDay, int iHour, int iMinute, int iSec )
{
	m_dwRegisterDate1 = ( iYear * 10000 ) + ( iMonth * 100 ) + iDay;
	m_dwRegisterDate2 = ( iHour * 10000 ) + ( iMinute * 100 ) + iSec;
}

SHORT TradeNode::GetYear()
{
	return (SHORT)(m_dwRegisterDate1/10000);
}

SHORT TradeNode::GetMonth()
{
	return ( m_dwRegisterDate1/100 ) % 100;
}

SHORT TradeNode::GetDay()
{
	return m_dwRegisterDate1 % 100;
}

SHORT TradeNode::GetHour()
{
	return (SHORT)(m_dwRegisterDate2 / 10000);
}

SHORT TradeNode::GetMinute()
{
	return ( m_dwRegisterDate2/100 ) % 100;
}

SHORT TradeNode::GetSec()
{
	return m_dwRegisterDate2 % 100;
}


