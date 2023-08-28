#include "../stdafx.h"

#include "../Network/GameServer.h"
#include "../Network/iocpHandler.h"
#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"
#include "../DataBase/LogDBClient.h"
#include "../MainProcess.h"
#include "../ioProcessChecker.h"
#include "../ServerCloseManager.h"
#include "../Shutdown.h"

#include "EventGoodsManager.h"
#include "CampManager.h"
#include "GuildNodeManager.h"
#include "TradeNodeManager.h"
#include "ServerNodeManager.h"
#include "TournamentManager.h"
#include "MgrToolNodeManager.h"

extern CLog TradeLOG;
extern CLog MonitorLOG;
ServerNodeManager *ServerNodeManager::sg_Instance = NULL;

ServerNodeManager::ServerNodeManager() : m_dwCurrentTime(0), m_dwCheckServerToServerConnectTime(0)
{
	CreateServerIndexes();
}

ServerNodeManager::~ServerNodeManager()
{
	m_vServerNode.clear();
}

ServerNodeManager &ServerNodeManager::GetInstance()
{
	if( !sg_Instance )
		sg_Instance = new ServerNodeManager;

	return *sg_Instance;
}

void ServerNodeManager::ReleaseInstance()
{
	SAFEDELETE( sg_Instance );
}

void ServerNodeManager::InitMemoryPool()
{
	ioINILoader kLoader( "ls_config_main.ini" );
	kLoader.SetTitle( "Server Session Buffer" );
	int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
	int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
	kLoader.SetTitle( "MemoryPool" );
	int iMaxServer = kLoader.LoadInt( "server_pool", 10 );

	m_MemNode.CreatePool(0, MAX_SERVERNODE, FALSE);
	for(int i = 0;i < iMaxServer;i++)
	{
		m_MemNode.Push( new ServerNode( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize ) );
	}
}

void ServerNodeManager::ReleaseMemoryPool()
{
	vServerNode_iter iter, iEnd;
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		ServerNode *pServerNode = *iter;
		pServerNode->OnDestroy();
		m_MemNode.Push( pServerNode );
	}	
	m_vServerNode.clear();
	m_MemNode.DestroyPool();

	for(sToolServerInfo::iterator iter = m_sToolServerInfo.begin(); iter != m_sToolServerInfo.end(); ++iter)
	{
		ToolServerInfo *pInfo = *iter;
		if( !pInfo )
			continue;
		SAFEDELETE( pInfo );
	}
	m_sToolServerInfo.clear();
}

void ServerNodeManager::CreateServerIndexes()
{
	for(DWORD dwServerIndex = 1 ; dwServerIndex <=  MAX_SERVERNODE ; dwServerIndex++)
	{
		m_vServerIndexes.push_back( dwServerIndex );
	}
}

ServerNode *ServerNodeManager::CreateServerNode(SOCKET s)
{
	ServerNode *newNode = m_MemNode.Remove();
	if( !newNode )
	{
		LOG.PrintTimeAndLog(0,"ServerNodeManager::CreateServerNode MemPool Zero!");
		return NULL;
	}

	newNode->SetSocket(s);
	newNode->OnCreate();

	// ������ ���� ���� �ε�
	g_DBClient.OnSelectItemBuyCnt();
	return newNode;
}

BOOL ServerNodeManager::CreateServerIndex(DWORD& dwServerIndex)
{
	dwServerIndex = 0;
	if(m_vServerIndexes.size() > 0)
	{
		dwServerIndex = m_vServerIndexes.front();
		m_vServerIndexes.pop_front();

		return TRUE;
	}
	return FALSE;
}

void ServerNodeManager::DestroyServerIndex(const DWORD dwServerIndex)
{
	if( 0 != dwServerIndex )
	{
		// ���μ��� ����۽� �ε����� �ʱ�ȭ �ǹǷ� �̹� �ִ� �ε������� Ȯ���� �߰��Ѵ�
		SERVERINDEXES::iterator it = std::find( m_vServerIndexes.begin(), m_vServerIndexes.end(),  dwServerIndex );
		if( it == m_vServerIndexes.end() )
		{
			m_vServerIndexes.push_front( dwServerIndex );
		}
	}
}

void ServerNodeManager::AddServerNode( ServerNode *pNewNode )
{
	m_vServerNode.push_back( pNewNode );

	// ���� ������ ���� �� �ŷ��� �������� ����ȭ ���ش�.
	g_TradeNodeManager.SendAllTradeItem();
}

void ServerNodeManager::RemoveNode( ServerNode *pServerNode )
{
	DWORD dwServerIndex = pServerNode->GetServerIndex();

	vServerNode_iter iter = std::find( m_vServerNode.begin(), m_vServerNode.end(), pServerNode );
	if( iter != m_vServerNode.end() )
	{
		ServerNode *pServerNode = *iter;
		pServerNode->OnDestroy();
		m_vServerNode.erase( iter );
		m_MemNode.Push( pServerNode );

		DestroyServerIndex( dwServerIndex );
	}	

	if( m_vServerNode.empty() )
	{
		if( g_ServerCloseMgr.IsServerClose() )
		{
			FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 �� �̻� �ɸ���α� ����
			//��� ������ ������ ����Ǿ���.
			g_MainProc.Shutdown( SHUTDOWN_QUICK );
		}
	}
}

ServerNode *ServerNodeManager::GetServerNode()
{
	if( m_vServerNode.empty() ) return NULL;

	int iSize = m_vServerNode.size();
	int iRand = rand() % iSize;

	if( COMPARE( iRand, 0, iSize ) )
	{
		return m_vServerNode[iRand];
	}

	return NULL;
}

ServerNode *ServerNodeManager::GetServerNode( DWORD dwServerIndex )
{
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		if( item->GetServerIndex() == dwServerIndex )
		{
			return item;
		}		
	}
	return NULL;
}

void ServerNodeManager::ProcessServerNode()
{
	FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 �� �̻� �ɸ���α� ����

	if( m_dwCurrentTime == 0 )
		m_dwCurrentTime = TIMEGETTIME();

	if( TIMEGETTIME() - m_dwCurrentTime < 20000 ) return;

	m_dwCurrentTime = TIMEGETTIME();
	
	// �������� �� ���� (20�� ~ 40�ʸ��� ����)
	CheckServerPing();

	// ������ ������ ����� ������ ã�Ƽ� �����Ų��. (1�� ~ 1�� 10�ʸ��� üũ)
	CheckServerToServerConnect();
}

void ServerNodeManager::ServerNode_SendBufferFlush()
{
	if( m_vServerNode.empty() == false )
	{
		vector< ServerNode* >::iterator	iter	= m_vServerNode.begin();
		vector< ServerNode* >::iterator	iterEnd	= m_vServerNode.end();

		for( iter ; iter != iterEnd ; ++iter )
		{
			ServerNode* pServerNode = (*iter);

			if( ! pServerNode->IsActive() )
				continue;
			if( pServerNode->GetSocket() == INVALID_SOCKET )
				continue;

			pServerNode->FlushSendBuffer();
		}
	}
}

DWORD ServerNodeManager::GetMaxUserCount()
{
	DWORD dwUserCount = 0;
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		dwUserCount += item->GetUserCount();	
	}
	return dwUserCount;
}

DWORD ServerNodeManager::GetMaxUserCountByChannelingType( ChannelingType eChannelingType )
{
	DWORD dwUserCount = 0;
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		dwUserCount += item->GetUserCountByChannelingType( eChannelingType );	
	}
	return dwUserCount;
}

int ServerNodeManager::GetServerConnectCount( DWORD dwServerIndex )
{
	int iReturnCount = 0;
	vServerNode_iter iter, iEnd;	
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		ServerNode *pServerNode = *iter;
		if( pServerNode->IsConnectServerIndex( dwServerIndex ) )
			iReturnCount++;
	}	
	return iReturnCount;
}

void ServerNodeManager::CheckServerToServerConnect()
{
	if( m_dwCheckServerToServerConnectTime == 0 )
		m_dwCheckServerToServerConnectTime = TIMEGETTIME();

	if( TIMEGETTIME() - m_dwCheckServerToServerConnectTime < 60000 )
		return;

	m_dwCheckServerToServerConnectTime = TIMEGETTIME();

	vSortServerNode vSortNode;
	vServerNode_iter iter, iEnd;	
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		SortServerNode kNode;
		kNode.m_pNode = *iter;
		if( kNode.m_pNode == NULL ) continue;

		kNode.m_iSortPoint = GetServerConnectCount( kNode.m_pNode->GetServerIndex() );
		vSortNode.push_back( kNode );
	}	

	if( vSortNode.empty() )
	{
		LOG.PrintTimeAndLog( 0, "There are no connected server" );
		return;
	}

	std::sort( vSortNode.begin(), vSortNode.end(), ServerNodeSort() );
	
	SortServerNode kLowConnectNode = vSortNode[0];
	if( kLowConnectNode.m_iSortPoint >= (int)m_vServerNode.size() )
	{
		int iSize = vSortNode.size();
		for(int i = 0;i < iSize;i++)
		{
			ServerNode *pNode = vSortNode[i].m_pNode;
			if( pNode )
				pNode->InitDisconnectCheckCount();
		}
		LOG.PrintTimeAndLog( 0, "%d servers are connected", kLowConnectNode.m_iSortPoint );
	}
	else
	{
		ServerNode *pDisconnectNode = kLowConnectNode.m_pNode;
		if( pDisconnectNode )
		{
			if( pDisconnectNode->IsDisconnectCheckOver() )
			{
				LOG.PrintTimeAndLog( 0, "������ ������ ����� ���� ���� ��ȣ ����(%d) : %s:%d", kLowConnectNode.m_iSortPoint, pDisconnectNode->GetPrivateIP().c_str(), pDisconnectNode->GetClientPort() );

				SP2Packet kPacket( MSTPK_LOW_CONNECT_SERVER_EXIT );
				kPacket << kLowConnectNode.m_iSortPoint;
				pDisconnectNode->SendMessage( kPacket );
			}		
		}
		else
		{
			LOG.PrintTimeAndLog( 0, "CheckServerToServerConnect NULL ServerNode" );
		}
	}
}

void ServerNodeManager::CheckServerPing()
{
	vServerNode_iter iter, iEnd;	
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		ServerNode *pServer = *iter;
		if( pServer == NULL ) continue;

		pServer->SendPingCheck();
	}	
}

void ServerNodeManager::FillServerInfo( SP2Packet &rkPacket )
{
	rkPacket << (int)m_vServerNode.size() + 1;
	vServerNode_iter iter, iEnd;	
	iEnd = m_vServerNode.end();
	for(iter = m_vServerNode.begin();iter != iEnd;++iter)
	{
		ServerNode *pServerNode = *iter;
		pServerNode->FillServerInfo( rkPacket );
	}

	// �߰�. (MainServer)
	FillMainServerInfo( rkPacket );
}

void ServerNodeManager::FillMainServerInfo( SP2Packet& rkPacket )
{
	// Main���� ����
	rkPacket << g_MainProc.GetPublicIP() << g_MainProc.GetPort()
			<< g_ServerNodeManager.GetNodeSize()
			<< 0 << 0 << 0 << 0 << 0 << 0;
}

void ServerNodeManager::FillTotalServerUserPos( SP2Packet &rkPacket )
{
	DWORD dwHeadquartersUserCount = 0;		//���� ����
	DWORD dwSafetySurvivalRoomUserCount = 0;//�ʺ������̹� �Ʒ� ����
	DWORD dwPlazaUserCount = 0;				//���� ����
	DWORD dwBattleRoomUserCount = 0;		//���� ����
	DWORD dwBattleRoomPlayUserCount = 0;	//���� �÷��� ����
	DWORD dwLadderBattlePlayUserCount= 0;    //����� ����
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		dwHeadquartersUserCount += item->GetUserCount();	
		dwSafetySurvivalRoomUserCount += item->GetSafetySurvivalRoomUserCount();
		dwPlazaUserCount += item->GetPlazaUserCount();
		dwBattleRoomUserCount += item->GetBattleRoomUserCount();
		dwBattleRoomPlayUserCount += item->GetBattleRoomPlayUserCount();
		dwLadderBattlePlayUserCount += item->GetLadderBattlePlayUserCount();
	}
	// �ִ� ���� �ο� - (������+��������+�����÷�������)
	if( dwHeadquartersUserCount >= (dwPlazaUserCount + dwBattleRoomPlayUserCount) )
		dwHeadquartersUserCount = dwHeadquartersUserCount - (dwPlazaUserCount + dwBattleRoomPlayUserCount);
	else
		dwHeadquartersUserCount = 0;
	rkPacket << dwHeadquartersUserCount << dwSafetySurvivalRoomUserCount << dwPlazaUserCount << dwBattleRoomUserCount << dwLadderBattlePlayUserCount;
}

bool ServerNodeManager::InsertToolServerInfo( ToolServerInfo *pInfo )
{
	sToolServerInfo::iterator iter = m_sToolServerInfo.find( pInfo );
	if( iter != m_sToolServerInfo.end() )
		return false;

	m_sToolServerInfo.insert( pInfo );

	return true;
}

void ServerNodeManager::FillToolServerInfo( SP2Packet &rkPacket )
{
	rkPacket << (int)m_sToolServerInfo.size();
	for(sToolServerInfo::iterator iter = m_sToolServerInfo.begin(); iter != m_sToolServerInfo.end(); ++iter)
	{
		ToolServerInfo *pInfo = *iter;
		if( !pInfo )
			continue;
		rkPacket << pInfo->m_szPublicIP;
		rkPacket << pInfo->m_szPrivateIP;
		rkPacket << pInfo->m_iClientPort;
	}

	// main ����
	rkPacket << g_MainProc.GetPublicIP() << g_MainProc.GetPort();
}

void ServerNodeManager::SendMessageAllNode( SP2Packet &rkPacket, const DWORD dwServerIndex )
{
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		if( item->GetServerIndex() != dwServerIndex)
			item->SendMessage( rkPacket );
	}
}

bool ServerNodeManager::SendMessageNode( int iServerIndex, SP2Packet &rkPacket )
{
	ServerNode *pNode = GetServerNode( iServerIndex );
	if( pNode )
		return pNode->SendMessage( rkPacket );
	return false;
}

bool ServerNodeManager::SendMessageArray( int iServerArray, SP2Packet &rkPacket )
{
	if( !COMPARE( iServerArray, 0, (int)m_vServerNode.size() ) )
		return false;

    vServerNode_iter iter = m_vServerNode.begin() + iServerArray;
	ServerNode *pNode = *iter;
	if( !pNode )
		return false;
	
	return pNode->SendMessage( rkPacket );
}

bool ServerNodeManager::SendMessageIP( ioHashString &rszIP, SP2Packet &rkPacket )
{
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;
	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		if( item->GetPrivateIP() == rszIP )
		{
			return item->SendMessage( rkPacket );
		}		
	}
	return false;
}

bool ServerNodeManager::SendMessageIPnPort( ioHashString& rszIP, int port, SP2Packet& rkPacket )
{
	vServerNode_iter iter = m_vServerNode.begin();
	vServerNode_iter iter_Prev;

	while( iter != m_vServerNode.end() )
	{
		iter_Prev = iter++;
		ServerNode *item = *iter_Prev;
		if( item->GetPrivateIP() == rszIP && item->GetClientPort() == port )
		{
			return item->SendMessage( rkPacket );
		}		
	}
	return false;
}

void ServerNodeManager::SendServerList( ServerNode *pServerNode )
{
	if( pServerNode == NULL ) return;

	const int iSendListSize = 40;
	int iMaxServerCount = (int)m_vServerNode.size();
	for(int iStartArray = 0;iStartArray < iMaxServerCount;)
	{
		int iLoop = iStartArray;
		int iSendSize = min( iMaxServerCount - iStartArray, iSendListSize );
		SP2Packet kPacket( MSTPK_ALL_SERVER_LIST );
		kPacket << iSendSize;	
		for(;iLoop < iStartArray + iSendSize;iLoop++)
		{
			ServerNode *pServer = m_vServerNode[iLoop];
			kPacket << pServer->GetServerIndex();
			kPacket << pServer->GetPrivateIP();
			kPacket << pServer->GetServerPort();
		}		
		// ��Ŷ ����
		kPacket << (bool)(iLoop >= iMaxServerCount);
		pServerNode->SendMessage( kPacket );		

		iStartArray = iLoop;
	}	
}

bool ServerNodeManager::GlobalQueryParse( SP2Packet &rkPacket )
{
	CQueryResultData query_data;
	rkPacket >> query_data;

	g_PacketChecker.QueryPacket( query_data.GetMsgType() );	
	
	FUNCTION_TIME_CHECKER( 100000.0f, query_data.GetMsgType() );          // 0.1 �� �̻� �ɸ���α� ����

	switch( query_data.GetMsgType() )
	{
	case DBAGENT_GAME_PINGPONG:
		OnResultPingPong( &query_data );
		return true;
	case DBAGENT_LOG_PINGPONG:
		return true;
	case DBAGENT_SERVER_INFO_SET:
		OnResultInsertGameServerInfo( &query_data );
		return true;
	case DBAGENT_ITEM_BUYCNT_SET:
		OnResultSelectItemBuyCnt( &query_data );
		return true;
	case DBAGENT_TOTAL_REG_USER_SET:
		OnResultSelectTotalRegUser( &query_data );
		return true;
	case DBAGENT_GUILD_INFO_GET:
		OnResultSelectGuildInfoList( &query_data );
		return true;
	case DBAGENT_CAMP_DATA_GET:
		OnResultSelectCampData( &query_data );
		return true;
	case DBAGENT_CAMP_SPECIAL_USER_COUNT_GET:
		OnResultSelectCampSpecialUserCount( &query_data );
		return true;
	case DBAGENT_TRADE_INFO_GET:
		OnResultSelectTradeInfoList( &query_data );
		return true;
	case DBAGENT_TRADE_DELETE:
		OnResultTradeItemDelete( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_DATA_GET:
		OnResultSelectTournamentData( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_CUSTOM_DATA_GET:
		OnResultSelectTournamentCustomData( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_TEAM_LIST_GET:
		OnResultSelectTournamentTeamList( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_WINNER_HISTORY_SET:
		OnResultInsertTournamentWinnerHistory( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_CUSTOM_INFO_GET:
		OnResultSelectTournamentCustomInfo( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_CUSTOM_ROUND_GET:
		OnResultSelectTournamentCustomRound( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_CONFIRM_USER_LIST:
		OnResultSelectTournamentConfirmUserList( &query_data );
		return true;
	case DBAGENT_TOURNAMENT_PREV_CAMP_INFO_GET:
		OnResultSelectTournamentPrevChampInfo( &query_data );
		return true;
	case DBAGENT_USERBLOCK_SET:
		OnResultInsertUserBlock( &query_data );
		return true;
	case DBAGENT_EVENTSHOP_BUYCOUNT_GET:
		OnResultSelectGoodsBuyCount( &query_data );
		return true;
	case DBAGENT_EVENTSHOP_BUYCOUNT_SET:
		OnResultUpdateGoodsBuyCount( &query_data );
		return true;
	case DBAGENT_EVENTSHOP_BUYCOUNT_DEL:
		return true;
	}

	return false;
}

void ServerNodeManager::OnResultPingPong( CQueryResultData *query_data )
{
	DWORD dwLastPing;
	if(!query_data->GetValue( dwLastPing ))	return;

	DWORD dwElapse = (TIMEGETTIME() - dwLastPing) / 2;
	Debug("DB Ping : %lu ms\n", dwElapse );
}

void ServerNodeManager::OnResultInsertGameServerInfo( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultInsertGameServerInfo Result FAILED! :%d",query_data->GetResultType());
		return;
	}	
}

void ServerNodeManager::OnResultSelectItemBuyCnt( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectItemBuyCnt Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	SP2Packet kPacket( MSTPK_CLASS_PRICE_INFO );
	kPacket << *query_data;
	SendMessageAllNode( kPacket );
}

void ServerNodeManager::OnResultSelectTotalRegUser( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTotalRegUser Result FAILED! :%d",query_data->GetResultType());
		return;
	}
	
	bool bServerDown;
	if(!query_data->GetValue( bServerDown ))	return;

	int iTotalRegUser;
	if(!query_data->GetValue( iTotalRegUser ))	return;

	g_MainProc.SetTotalRegUser( iTotalRegUser );
	LOG.PrintTimeAndLog( 0, "OnResultSelectTotalRegUser : %d - ServerDown(%d)", iTotalRegUser, (int)bServerDown );

	if( !bServerDown )
	{
		SP2Packet kPacket( MSTPK_TOTAL_REG_USER_CNT );
		kPacket << iTotalRegUser;
		SendMessageAllNode( kPacket );
	}	
}

void ServerNodeManager::OnResultSelectGuildInfoList( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectGuildInfoList Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	int   iCount = 0;
	DWORD dwLastIndex = GUILD_LOAD_START_INDEX;
	while( query_data->IsExist() )
	{
		GuildNode *pGuildNode = g_GuildNodeManager.CreateGuildNode( query_data );
		if( pGuildNode )
		{
			dwLastIndex = pGuildNode->GetGuildIndex();
		}
		iCount++;
	}

	if( iCount >= GUILD_MAX_LOAD_LIST )
	{
		g_DBClient.OnSelectGuildInfoList( dwLastIndex, GUILD_MAX_LOAD_LIST );
	}	
	else
	{
		g_GuildNodeManager.SortGuildRankAll();
	}
}

void ServerNodeManager::OnResultSelectCampData( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectCampData Result FAILED! :%d",query_data->GetResultType());
		return;
	}
	
	int iBlueCampPoint = 0 ,iBlueCampTodayPoint = 0, iBlueCampBonusPoint = 0;
	int iRedCampPoint = 0 ,iRedCampTodayPoint = 0, iRedCampBonusPoint = 0;
	
	if( query_data->GetResultCount() > 0 )
	{
		if(!query_data->GetValue( iBlueCampPoint ))			return;
		if(!query_data->GetValue( iBlueCampTodayPoint ))	return;
		if(!query_data->GetValue( iBlueCampBonusPoint ))	return;
		if(!query_data->GetValue( iRedCampPoint ))			return;
		if(!query_data->GetValue( iRedCampTodayPoint ))		return;
		if(!query_data->GetValue( iRedCampBonusPoint ))		return;
	}

	g_CampMgr.DBToCampData( iBlueCampPoint, iBlueCampTodayPoint, iBlueCampBonusPoint,
							iRedCampPoint, iRedCampTodayPoint, iRedCampBonusPoint );
}

void ServerNodeManager::OnResultSelectCampSpecialUserCount( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectCampSpecialUserCount Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	int iCampType = 0, iCampSpecialEntry = 0;
	if(!query_data->GetValue( iCampType ))			return;
	if(!query_data->GetValue( iCampSpecialEntry ))	return;

	g_CampMgr.DBToCampSpecialUserCount( iCampType, iCampSpecialEntry );
}

void ServerNodeManager::OnResultSelectTradeInfoList( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTradeInfoList Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	int   iCount = 0;
	DWORD dwLastIndex = TRADE_START_LAST_INDEX;
	while( query_data->IsExist() )
	{
		TradeNode *pTradeNode = g_TradeNodeManager.CreateTradeNode( query_data );
		if( pTradeNode )
		{
			dwLastIndex = pTradeNode->GetTradeIndex();
		}
		iCount++;
	}

	if( iCount >= TRADE_MAX_LOAD_LIST )
	{
		g_DBClient.OnSelectTradeItemInfo( dwLastIndex, TRADE_MAX_LOAD_LIST );
	}	

	// DB ���� ó�� �а� ���� ����ȭ�� �����ش�.
	g_TradeNodeManager.SendAllTradeItem();
}

void ServerNodeManager::OnResultTradeItemDelete( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultTradeItemDelete Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	DWORD dwTradeIndex=0, dwServerIndex=0;
	if(!query_data->GetValue( dwTradeIndex ))	return;
	if(!query_data->GetValue( dwServerIndex ))	return;

	g_TradeNodeManager.SendTimeOutItemInfo( dwTradeIndex, dwServerIndex );
}

void ServerNodeManager::OnResultSelectTournamentData( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentData Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	int   iCount = 0;
	DWORD dwLastIndex = TOURNAMENT_LOAD_START_INDEX;
	while( query_data->IsExist() )
	{
		TournamentNode *pNode = g_TournamentManager.CreateTournamentNode( query_data );
		if( pNode )
		{
			dwLastIndex = pNode->GetIndex();			
		}
		iCount++;
	}

	if( iCount >= TOURNAMENT_MAX_LOAD_LIST )
	{
		g_DBClient.OnSelectTournamentData( dwLastIndex, TOURNAMENT_MAX_LOAD_LIST );
	}	
	else
	{
		g_TournamentManager.CreateCompleteSort();

		// ��ʸ�Ʈ �� ����
		g_DBClient.OnSelectTournamentTeamList( TOURNAMENT_LOAD_START_INDEX, TOURNAMENT_TEAM_MAX_LOAD_LIST );
	}
}

void ServerNodeManager::OnResultSelectTournamentCustomData( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentCustomData Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	DWORD dwTourIndex=0, dwUserIndex=0, dwServerIndex=0;
	if(!query_data->GetValue( dwTourIndex ))		return;
	if(!query_data->GetValue( dwUserIndex ))		return;
	if(!query_data->GetValue( dwServerIndex ))		return;

	TournamentNode *pNode = g_TournamentManager.CreateTournamentNode( query_data );
	if( pNode )
	{
		if( pNode->GetIndex() == dwTourIndex )
		{
			//
			SP2Packet kPacket( MSTPK_TOURNAMENT_CUSTOM_CREATE );
			kPacket << dwUserIndex << dwTourIndex;
			if( g_ServerNodeManager.SendMessageNode( dwServerIndex, kPacket ) == false )
			{
				LOG.PrintTimeAndLog( 0, "OnResultSelectTournamentCustomData : None ServerNode : %d", dwServerIndex );
			}
		}
		else
		{
			LOG.PrintTimeAndLog( 0, "OnResultSelectTournamentCustomData : MissMatch Data : %d - %d", dwTourIndex, pNode->GetIndex() );
		}
	}
	else
	{
		LOG.PrintTimeAndLog( 0, "OnResultSelectTournamentCustomData : None Data" );
	}
}

void ServerNodeManager::OnResultSelectTournamentTeamList( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentTeamList Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	int   iCount = 0;
	DWORD dwLastIndex = TOURNAMENT_LOAD_START_INDEX;
	while( query_data->IsExist() )
	{
		TournamentTeamNode *pTeam = g_TournamentManager.CreateTournamentTeamNode( query_data );
		if( pTeam )
		{
			dwLastIndex = pTeam->GetTeamIndex();
		}
		iCount++;
	}

	if( iCount >= TOURNAMENT_TEAM_MAX_LOAD_LIST )
	{
		g_DBClient.OnSelectTournamentTeamList( dwLastIndex, TOURNAMENT_TEAM_MAX_LOAD_LIST );
	}	
	else
	{
		g_TournamentManager.CreateTeamCompleteRound();
	}
}

void ServerNodeManager::OnResultInsertTournamentWinnerHistory( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultInsertTournamentWinnerHistory Result FAILED! :%d",query_data->GetResultType());
		return;
	}	
}

void ServerNodeManager::OnResultSelectTournamentCustomInfo( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentCustomInfo Result FAILED! :%d",query_data->GetResultType());
		return;
	}	

	g_TournamentManager.ApplyTournamentInfoDB( query_data );
}

void ServerNodeManager::OnResultSelectTournamentCustomRound( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentCustomRound Result FAILED! :%d",query_data->GetResultType());
		return;
	}	

	g_TournamentManager.ApplyTournamentRoundDB( query_data );
}

void ServerNodeManager::OnResultSelectTournamentConfirmUserList( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentConfirmUserList Result FAILED! :%d",query_data->GetResultType());
		return;
	}	

	g_TournamentManager.ApplyTournamentConfirmUserListDB( query_data );
}

void ServerNodeManager::OnResultSelectTournamentPrevChampInfo( CQueryResultData *query_data )
{
	if(FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectTournamentPrevChampInfo Result FAILED! :%d",query_data->GetResultType());
		return;
	}	

	g_TournamentManager.ApplyTournamentPrevChampInfoDB( query_data );
}

void ServerNodeManager::OnResultInsertUserBlock( CQueryResultData *query_data )
{
	if( FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultInsertUserBlock Result FAILED! :%d",query_data->GetResultType());
		return;
	}

	g_MgrTool.ApplyUserBlockDB( query_data );
}

void ServerNodeManager::OnResultSelectGoodsBuyCount( CQueryResultData *query_data )
{
	if( FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultSelectGoodsBuyCount Result FAILED! :%d", query_data->GetResultType());
		return;
	}

	DWORD dwUserIndex=0, dwGoodsIndex=0;
	BYTE byType=0, byCount=0;

	// ���ο� ���������� �߰�
	int iCount = 0;
	while( query_data->IsExist() )
	{
		if(!query_data->GetValue( dwUserIndex ))		return;
		if(!query_data->GetValue( byType ))				return;
		if(!query_data->GetValue( dwGoodsIndex ))		return;
		if(!query_data->GetValue( byCount ))			return;

		g_EventGoodsMgr.ApplyUserBuyData(dwUserIndex, dwGoodsIndex, byCount);

		LOG.PrintTimeAndLog(0, "Loading eventGoods : type[%d] userindex[%d] goods[%d] count[%d]", byType, dwUserIndex, dwGoodsIndex, byCount);
		++iCount;
	}

	if(iCount == g_EventGoodsMgr.m_PagingSize )
	{
		g_DBClient.OnSelectGoodsBuyCount(g_EventGoodsMgr.m_PagingSize, ++g_EventGoodsMgr.m_Page);
	}
	else
	{
		LOG.PrintTimeAndLog(0, "Loading eventGoods completed : [%d]", g_EventGoodsMgr.m_Page);
	}
}

void ServerNodeManager::OnResultUpdateGoodsBuyCount( CQueryResultData *query_data )
{
	if( FAILED(query_data->GetResultType()))
	{
		LOG.PrintTimeAndLog(0,"DB OnResultUpdateGoodsBuyCount Result FAILED! :%d", query_data->GetResultType());
		return;
	}

}