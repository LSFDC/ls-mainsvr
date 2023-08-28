#include "../stdafx.h"

#include "../MainProcess.h"
#include "../ioProcessChecker.h"

#include "../Network/GameServer.h"
#include "../Network/ioPacketQueue.h"
#include "../DataBase/DBClient.h"

#include "TradeNodeManager.h"
#include "CampManager.h"
#include "GuildNodeManager.h"
#include "MgrToolNodeManager.h"
#include "EventGoodsManager.h"
#include "ServerNodeManager.h"
#include "ExtraItemGrowthCatalystMgr.h"
#include "TournamentManager.h"
#include "SuperGashponLimitManager.h"
#include "ServerNode.h"
#include "NodeInfoManager.h"
#include "../Define.h"
#include "SpecialShopManager.h"
#include "GuildRoomInfos.h"
#include "GuildRoomInfos.h"

extern CLog TradeLOG;
extern CLog MonitorLOG;

ServerNode::ServerNode( SOCKET s, DWORD dwSendBufSize, DWORD dwRecvBufSize ) : CConnectNode( s, dwSendBufSize, dwRecvBufSize )
{
	InitData();
}

ServerNode::~ServerNode()
{	
}

void ServerNode::InitData()
{
	m_dwServerIndex= 0;
	m_dwNormalUserCount = 0;
	m_dwMgameUserCount  = 0;
	m_dwDaumUserCount   = 0;
	m_dwBuddyUserCount  = 0;
	m_dwNaverUserCount  = 0;
	m_dwToonilandUserCount = 0;
	m_dwNexonUserCount = 0;
	m_dwSteamUserCount = 0;
	m_dwServerPingSendTime = 0;
	m_dwServerNoReactionTime = 0;
	m_serverID = 0;
	m_szPrivateIP.Clear();
	m_szPublicIP.Clear();
	m_szServerName.Clear();
	m_iServerPort  = 0;
	m_iClientPort  = 0;
	m_dwSendPingTime = 0;
	m_dwPingMS     = 0;
	m_dwRoomCount  = 0;
	m_dwPlazaCount = 0;
	m_dwBattleRoomCount = 0;
	m_iSafetySurvivalRoomUserCount = 0;
	m_iPlazaUserCount   = 0;
	m_iBattleRoomUserCount = 0;
	m_iBattleRoomPlayUserCount = 0;
	m_iLadderBattlePlayUserCount = 0;
	m_iDisconnectCheckCount = 0;
	m_vConnectServerIndex.clear();
	m_dwDBQueryMS = 0;
	m_bRelayServerState = TRUE;
	m_dwHangameUserCount	= 0;
}

void ServerNode::OnCreate()
{
	CConnectNode::OnCreate();
	InitData();
	CConnectNode::SetIPMapping( "127.0.0.1" ); // �缳IP�� ���ʿ䰡 ���� 127.0.0.1 �Է�
}

bool ServerNode::AfterCreate()
{
 
	{
		SP2Packet kPacket( MSTPK_UPDATE_CLIENT_VERSION );
		kPacket << g_MainProc.IsUseClientVersion();
		if( g_MainProc.IsUseClientVersion() )
			kPacket << g_MainProc.GetClientVersion();
		SendMessage( kPacket );
	}

	{
		SP2Packet kPacket( MSTPK_CAMP_BATTLE_INFO );
		kPacket << g_CampMgr.IsCampBattlePlay();
		kPacket << g_CampMgr.GetActiveCampDate();
		SendMessage( kPacket );
		//LOG.PrintTimeAndLog( 0, "ServerNode::AfterCreate() : Active camp Date : %d",  g_CampMgr.GetActiveCampDate() ); //������
	}	

	{	// ���� ���� ����
		g_ExtraItemGrowthCatalystMgr.SendLoadData( this );
	}

	{	// ���� �ɼ� ����
		SP2Packet kPacket( MSTPK_GAME_SERVER_OPTION );
		g_MainProc.FillGameServerOption( kPacket );
		SendMessage( kPacket );
	}

	{
		//Ư������ ����
		SP2Packet kPacket1( MSTPK_SPECIAL_SHOP_STATE_CHANGE );
		PACKET_GUARD_bool( kPacket1.Write(g_SpecialShopManager.IsOpen()) );
		SendMessage(kPacket1);

		SP2Packet kPacket2( MSTPK_SPECIAL_SHOP_GOODS_INFO );
		g_SpecialShopManager.FillCurSellGoodsInfo(kPacket2);
		SendMessage(kPacket2);
	}

	{
		// ���� ���� ���� �˸� 
		SP2Packet kPacket( MSTPK_SERVER_CONNECT );
		SendMessage( kPacket );
	}

	{
		//�ش� ���Ӽ����� ���� ������ ��û
		SP2Packet kPacket(MSTPK_REQ_ALL_GUILD_ROOM_INFO);
		SendMessage( kPacket );

		g_GuildRoomMgr.AddRequestCount();
	}

	m_dwServerPingSendTime = TIMEGETTIME();

	return CConnectNode::AfterCreate();
}

void ServerNode::OnDestroy()
{
	// ������ ������ ����Ǿ� ����͸� ���� �˸�
	SP2Packet kPacket( STPKM_SERVER_DISCONNECT );
	kPacket << m_szPrivateIP << m_iClientPort;
	g_MgrTool.SendMessageAllNode( kPacket );

	LOG.PrintTimeAndLog( 0, "Server Node : Disconnect : (%s:%d[%s])", m_szPrivateIP.c_str(), m_iClientPort, GetPublicIP() );
	CConnectNode::OnDestroy();
}

bool ServerNode::CheckNS( CPacket &rkPacket )
{

	return true;             //��Ʈ�� ���� �ʿ����.
}

int ServerNode::GetConnectType()
{
	return CONNECT_TYPE_GAMESERVER;
}

bool ServerNode::IsDisconnectCheckOver()
{
	m_iDisconnectCheckCount++;
	if( m_iDisconnectCheckCount > 2 )
		return true;
	return false;
}

void ServerNode::FillServerInfo( SP2Packet &rkPacket )
{
	rkPacket << m_szServerName << m_szPrivateIP << m_iClientPort 
			 << (WORD)GetUserCount() << (WORD)m_dwRoomCount 
			 << (WORD)m_dwPlazaCount << (WORD)m_dwBattleRoomCount 
			 << (WORD)m_dwPingMS     << (WORD)m_dwDBQueryMS
			 << GetCurDelaySec();
	rkPacket << m_bRelayServerState;
}

bool ServerNode::IsConnectServerIndex( DWORD dwServerIndex )
{
	int i = 0;
	for(i = 0;i < (int)m_vConnectServerIndex.size();i++)
	{
		if( dwServerIndex == m_vConnectServerIndex[i] )
			return true;
	}
	return false;
}

void ServerNode::ApplyConnectServerIndex( SP2Packet &rkPacket )
{
	m_vConnectServerIndex.clear();
	m_vConnectServerIndex.push_back( GetServerIndex() ); 

	// 
	int i, iMaxIndexList;
	rkPacket >> iMaxIndexList;
	for(i = 0;i < iMaxIndexList;i++)
	{
		DWORD dwServerIndex;
		rkPacket >> dwServerIndex;
		m_vConnectServerIndex.push_back( dwServerIndex );
	}	
}

void ServerNode::SendPingCheck()
{
	if( m_dwServerPingSendTime == 0 )
		m_dwServerPingSendTime = TIMEGETTIME();

	if( TIMEGETTIME() - m_dwServerPingSendTime < PING_CHECK_TIME )
		return;

	m_dwServerPingSendTime = TIMEGETTIME();

	SP2Packet kPacket( MSTPK_SERVER_PING_CHECK );
	SendMessage( kPacket );
	m_dwSendPingTime = TIMEGETTIME();
}

void ServerNode::SessionClose( BOOL safely )
{
	if(IsActive())
	{
		CPacket packet(ITPK_CLOSE_SESSION);
		ReceivePacket( packet );
	}
}

bool ServerNode::SendMessage( CPacket &rkPacket )
{
	g_ProcessChecker.ServerSendMessage( rkPacket.GetBufferSize() );
	return CConnectNode::SendMessage( rkPacket );
}

void ServerNode::ReceivePacket( CPacket &packet )
{
	g_RecvQueue.InsertQueue( (DWORD)this, packet, PK_QUEUE_SESSION );
}

void ServerNode::PacketParsing( CPacket &packet )
{
	SP2Packet &kPacket = (SP2Packet&)packet;

	FUNCTION_TIME_CHECKER( 100000.0f, kPacket.GetPacketID() );          // 0.1 �� �̻� �ɸ���α� ����
	
	switch( kPacket.GetPacketID() )
	{
	case ITPK_CLOSE_SESSION:
		OnClose( kPacket );
		break;
	case MSTPK_SERVER_INFO:
		OnServerInfo( kPacket );
		break;
	case MSTPK_SERVER_UPDATE:
		OnServerUpdate( kPacket );
		break;
	case MSTPK_GUILD_CREATE_REG:
		OnCreateGuildReg( kPacket );
		break;
	case MSTPK_GUILD_RANK_LIST:
		OnGuildRankList( kPacket );
		break;
	case MSTPK_GUILD_INFO:
		OnGuildInfo( kPacket );
		break;
	case MSTPK_GUILD_JOINER_CHANGE:
		OnGuildChangeJoiner( kPacket );
		break;
	case MSTPK_GUILD_ENTRY_AGREE:
		OnGuildEntryAgree( kPacket );
		break;
	case MSTPK_GUILD_LEAVE:
		OnGuildLeave( kPacket );
		break;
	case MSTPK_GUILD_TITLE_CHANGE:
		OnGuildTitleChange( kPacket );
		break;
	case MSTPK_GUILD_SIMPLE_INFO:
		OnGuildSimpleInfo( kPacket );
		break;
	case MSTPK_GUILD_JOIN_USER:
		OnGuildJoinUser( kPacket );
		break;
	case MSTPK_GUILD_MARK_CHANGE:
		OnGuildMarkChange( kPacket );
		break;
	case MSTPK_GUILD_EXIST:
		OnGuildExist( kPacket );
		break;
	case MSTPK_GUILD_MARK_BLOCK_INFO:
		OnGuildMarkBlockInfo( kPacket );
		break;
	case MSTPK_LADDER_MODE_RESULT_UPDATE:
		OnLadderModeResultUpdate( kPacket );
		break;
	case MSTPK_GUILD_TITLE_SYNC:		
		OnGuildTitleSync( kPacket );		
		break;
	case MSTPK_ADD_LADDER_POINT:
		OnGuildAddLadderPoint( kPacket );
		break;
	case MSTPK_CAMP_ROOM_BATTLE_INFO:
		OnCampRoomBattleInfo( kPacket );
		break;
	case MSTPK_CAMP_DATA_SYNC:
		OnCampDataSync( kPacket );
		break;
	case MSTPK_CAMP_ENTRY_CHANGE:
		OnCampEntryChange( kPacket );
		break;
	case MSTPK_GUILD_NAME_CHANGE:
		OnGuildNameChange( kPacket );
		break;
	case MSTPK_LOAD_CS3_FILE_RESULT:
		OnResultLoadCS3File( kPacket );
		break;
	case MSTPK_CS3_FILE_VERSION_RESULT:
		OnResultCS3FileVersion( kPacket );
		break;
	case MSTPK_TRADE_CREATE_REG:
		OnCreateTradeReg( kPacket );
		break;
	case MSTPK_TRADE_LIST:
		OnTradeList( kPacket );
		break;
	case MSTPK_TRADE_ITEM_TRADE:
		OnTradeItem( kPacket );
		break;
	case MSTPK_TRADE_ITEM_CANCEL:
		OnTradeCancel( kPacket );
		break;
	case MSTPK_EVENT_SHOP_GOODS_LIST:
		OnEventShopGoodsList( kPacket );
		break;
	case MSTPK_EVENT_SHOP_GOODS_BUY:
		OnEventShopGoodsBuy( kPacket );
		break;
	case MSTPK_EVENT_SHOP_GOODS_BUY_RESULT:
		OnEventShopGoodsBuyResult( kPacket );
		break;
	case MSTPK_EVENT_SHOP_STATE:
		OnEventShopState( kPacket );
		break;
	case MSTPK_EVENT_SHOP_BUY_USER_CLEAR:
		OnEventShopBuyUserClear( kPacket );
		break;
	case MSTPK_EXTRAITEM_GROWTH_MORTMAIN_CHECK:
		OnExtraItemGrowthMortmainCheck( kPacket );
		break;
	case MSTPK_SERVER_PING_CHECK:
		OnServerPingCheck( kPacket );
		break;
	case MSTPK_EXTRAITEM_GROWTH_CATALYST_INFO:
		OnExtraItemGrowthMortmainInfo( kPacket );
		break;
	case MSTPK_SERVER_INFO_ACK:
		{
			OnServerInfoAck( kPacket );
		}
		break;
	case MSTPK_TOURNAMENT_REGULAR_INFO:
		OnTournamentRegularInfo( kPacket );
		break;
	case MSTPK_TOURNAMENT_MAIN_INFO:
		OnTournamentMainInfo( kPacket );
		break;
	case MSTPK_TOURNAMENT_LIST_REQUEST:
		OnTournamentListRequest( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_CREATE:
		OnTournamentTeamCreate( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_INFO:
		OnTournamentTeamInfo( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_DELETE:
		OnTournamentTeamDelete( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_LADDER_POINT_ADD:
		OnTournamentTeamLadderPointAdd( kPacket );
		break;
	case MSTPK_TOURNAMENT_SCHEDULE_INFO:
		OnTournamentScheduleInfo( kPacket );
		break;
	case MSTPK_TOURNAMENT_ROUND_TEAM_DATA:
		OnTournamentRoundTeamData( kPacket );
		break;
	case MSTPK_TOURNAMENT_ROUND_CREATE_BATTLEROOM:
		OnTournamentRoundCreateBattleRoom( kPacket );
		break;
	case MSTPK_TOURNAMENT_BATTLE_RESULT:
		OnTournamentBattleResult( kPacket );
		break;
	case MSTPK_TOURNAMENT_BATTLE_TEAM_CHANGE:
		OnTournamentBattleTeamChange( kPacket );
		break;
	case MSTPK_TOURNAMENT_CUSTOM_CREATE:
		OnTournamentCustomCreate( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_ALLOCATE_LIST:
		OnTournamentTeamAllocateList( kPacket );
		break;
	case MSTPK_TOURNAMENT_TEAM_ALLOCATE_DATA:
		OnTournamentTeamAllocateData( kPacket );
		break;
	case MSTPK_TOURNAMENT_JOIN_CONFIRM_CHECK:
		OnTournamentJoinConfirmCheck( kPacket );  
		break;
	case MSTPK_TOURNAMENT_CONFIRM_REG:
		OnTournamentJoinConfirmReg( kPacket );
		break;
	case MSTPK_TOURNAMENT_ANNOUNCE_CHANGE:
		OnTournamentAnnounceChange( kPacket );
		break;
	case MSTPK_TOURNAMENT_TOTAL_TEAM_LIST:
		OnTournamentTotalTeamList( kPacket );
		break;
	case MSTPK_TOURNAMENT_CUSTOM_STATE_START:
		OnTournamentCustomStateStart( kPacket );
		break;
	case MSTPK_TOURNAMENT_CUSTOM_REWARD_LIST:
		OnTournamentCustomRewardList( kPacket );
		break;
	case MSTPK_TOURNAMENT_CUSTOM_REWARD_REG_CHECK:
		OnTournamentCustomRewardRegCheck( kPacket );
		break;
	case MSTPK_TOURNAMENT_CUSTOM_REWARD_REG_UPDATE:
		OnTournamentCustomRewardRegUpdate( kPacket );
		break;
	case MSTPK_NODEINFO_REQUEST:
		OnNodeInfo(kPacket);
		break;
	case MSTPK_EVENT_NPC_CLOSE:
		OnEventNpcClose( kPacket );
		break;
	case MSTPK_SUPER_GASHPON_LIMIT_CHECK:
		OnSuperGashponLimitCheck( kPacket );
		break;
	case MSTPK_SUPER_GASHPON_LIMIT_DECREASE:
		OnSuperGashponLimitDecrease( kPacket );
		break;
	case MSTPK_SUPER_GASHPON_LIMIT_INFO:
		OnSuperGashponLimitInfo( kPacket );
		break;
	case MSTPK_SUPER_GASHPON_LIMIT_RESET:
		OnSuperGashponLimitReset( kPacket );
		break;
	case MSTPK_TOURNAMENT_CHEER_DECISION:
		OnTournamentCheerDecision( kPacket );
		break;
	case MSTPK_SPECIAL_SHOP_GOODS_BUY:
		OnSpecialShopGoodsBuy( kPacket );
		break;
	case MSTPK_SPECIAL_SHOP_GOODS_BUY_RESULT:
		OnSpecialShopGoodsBuyResult( kPacket );
		break;
	case MSTPK_GUILD_ENTRY_FAIL:
		OnDecreaseGuildUserCount( kPacket );
		break;
	case MSTPK_REQ_GUILD_ROOM_INDEX:
		OnReqGuildRoomIndex( kPacket );
		break;
	case MSTPK_UPDATE_GUILD_ROOM_INDEX:
		OnUpdateGuildRoomIndex( kPacket );
		break;
	case MSTPK_DELETE_GUILD_ROOM_INFO:
		OnDeleteGuildRoomInfo( kPacket );
		break;
	case MSTPK_REQ_ALL_GUILD_ROOM_INFO:
		OnGuildRoomsInfo(kPacket);
		break;
	case MSTPK_TOURNAMENT_MACRO:
		OnTournamentMacro(kPacket);
		break;
// �ؿܿ� ���� ���̼��� üũ
#if defined( SRC_OVERSEAS )
	case MSTPK_LICENSE_ALERT_CHECK:
		OnSendLicenseAlert( kPacket );	//���Ӽ��� ���̼��� ����
		break;
#endif

	default:
		LOG.PrintTimeAndLog( 0, "ServerNode::PacketParsing �˼����� ��Ŷ : 0x%x", kPacket.GetPacketID() );
		break;
	}
}

void ServerNode::OnClose( SP2Packet &kPacket )
{
	g_ServerNodeManager.RemoveNode( this );

	Information( "-Gameserver Closed\n" );
}

void ServerNode::OnServerInfo( SP2Packet &rkPacket )
{
	rkPacket >> m_dwServerIndex >> m_serverID >> m_szServerName >> m_szPublicIP >> m_szPrivateIP >> m_iServerPort >> m_iClientPort;	

	if( m_dwServerIndex == 0 )	// ù ���� ����
	{	
		// DB�� ���� ���
		g_DBClient.OnInsertGameServerInfo( m_serverID, m_szPublicIP, m_iServerPort, m_szServerName, m_iClientPort );

		// Index ����
		if(g_ServerNodeManager.CreateServerIndex(m_dwServerIndex))
		{
			SP2Packet kPacket( MSTPK_SERVER_INDEX );
			kPacket << m_dwServerIndex;
			SendMessage( kPacket );

			// ���� ����Ʈ ����
			g_ServerNodeManager.SendServerList( this );
			LOG.PrintTimeAndLog( 0, "ServerNode::OnServerInfo Create Index : %d - %s:%d - %d", GetServerIndex(), GetPrivateIP().c_str(), GetServerPort(), g_ServerNodeManager.GetNodeSize() );
		}
		else
		{
			// �ε����� ���� ������ ���̻� ������ �� ����(512���� ���Ӽ����� �ʰ���)
			SP2Packet kPacket( MSTPK_SERVER_INDEX );
			kPacket << m_dwServerIndex;
			SendMessage( kPacket );
		}
	}
	else
	{
		// �翬��� ����
		LOG.PrintTimeAndLog( 0, "ServerNode::OnServerInfo Reconnect : %d - %s:%d - %d", GetServerIndex(), GetPrivateIP().c_str(), GetServerPort(), g_ServerNodeManager.GetNodeSize() );
	}

	SP2Packet kPacket( MSTPK_TOTAL_REG_USER_CNT );
	kPacket << g_MainProc.GetTotalRegUser();
	SendMessage( kPacket );

	// tool server info
	ToolServerInfo *pInfo = new ToolServerInfo;
	if( pInfo )
	{
		pInfo->m_szPublicIP   = m_szPublicIP;
		pInfo->m_szPrivateIP  = m_szPrivateIP;
		pInfo->m_iClientPort  = m_iClientPort;
	}

	if( !g_ServerNodeManager.InsertToolServerInfo( pInfo ) )
		SAFEDELETE( pInfo );

	// ��ȸ ���� ����
	g_TournamentManager.SendTournamentListServerSync( this );
}

void ServerNode::OnServerUpdate( SP2Packet &rkPacket )
{
	//��� ����
	rkPacket >> m_dwNormalUserCount >> m_dwMgameUserCount >> m_dwDaumUserCount >> m_dwBuddyUserCount >> m_dwNaverUserCount >> m_dwToonilandUserCount >> m_dwNexonUserCount >> m_dwHangameUserCount >> m_dwRoomCount >> m_dwPlazaCount >> m_dwBattleRoomCount;
#ifdef SRC_NA  //�Ϲ��� ��� ���� CCU �� ������
	rkPacket >> m_dwSteamUserCount;
#endif
	rkPacket >> m_dwRoomCount >> m_dwPlazaCount >> m_dwBattleRoomCount;
	//��ġ ����
	rkPacket >> m_iSafetySurvivalRoomUserCount >> m_iPlazaUserCount >> m_iBattleRoomUserCount >> m_iBattleRoomPlayUserCount >> m_iLadderBattlePlayUserCount;

	// ��Ÿ
	rkPacket >> m_dwDBQueryMS;
	//�����̼��� ������Ʈ �߰� 
	rkPacket >> m_bRelayServerState;


	SP2Packet kPacket( MSTPK_SERVER_UPDATE );
	g_ServerNodeManager.FillTotalServerUserPos( kPacket );
	
	SendMessage( kPacket );

	//����� ���� �ε�����.
	ApplyConnectServerIndex( rkPacket );	
	MonitorLOG.PrintTimeAndLog( 0, "(%s : %d) : G:%dms - D:%dms - %d�� - %d�� - %d�� - %d�� - %d�� - %d�� - %d�� - %d��", m_szPrivateIP.c_str(), m_iClientPort, 
								m_dwPingMS, m_dwDBQueryMS, m_dwNormalUserCount, m_dwMgameUserCount, m_dwDaumUserCount, m_dwBuddyUserCount, m_dwNaverUserCount, m_dwToonilandUserCount, m_dwNexonUserCount, m_dwHangameUserCount );
	m_dwServerNoReactionTime = TIMEGETTIME();
}

void ServerNode::OnCreateGuildReg( SP2Packet &rkPacket )
{
	g_GuildNodeManager.CreateGuildNode( rkPacket );
}

void ServerNode::OnGuildRankList( SP2Packet &rkPacket )
{
	g_GuildNodeManager.SendCurGuildList( this, rkPacket );
}

void ServerNode::OnGuildInfo( SP2Packet &rkPacket )
{
	g_GuildNodeManager.SendCurGuildInfo( this, rkPacket );
}

void ServerNode::OnGuildChangeJoiner( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwGuildIndex, dwGuildMaxUser;
	rkPacket >> dwUserIndex >> dwGuildIndex >> dwGuildMaxUser;
	
	dwGuildMaxUser = g_GuildNodeManager.ChangeGuildMaxUser( dwGuildIndex, dwGuildMaxUser );

	SP2Packet kPacket( MSTPK_GUILD_JOINER_CHANGE );
	kPacket << dwUserIndex << dwGuildIndex << dwGuildMaxUser;
	SendMessage( kPacket );
}

void ServerNode::OnGuildEntryAgree( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex	= 0 , dwMasterIndex = 0, dwEntryUserIndex = 0 ;
	PACKET_GUARD_VOID(rkPacket.Read(dwGuildIndex));
	PACKET_GUARD_VOID(rkPacket.Read(dwMasterIndex));
	PACKET_GUARD_VOID(rkPacket.Read(dwEntryUserIndex));
	
	SP2Packet kPacket( MSTPK_GUILD_ENTRY_AGREE );

	if( g_GuildNodeManager.GuildEntryUserAgree(dwGuildIndex) )
	{
		PACKET_GUARD_VOID(kPacket.Write(GUILD_ENTRY_POSSIBLE));
	}
	else
	{
		PACKET_GUARD_VOID(kPacket.Write(GUILD_ENTRY_IMPOSSIBEL));
	}

	PACKET_GUARD_VOID(kPacket.Write(dwGuildIndex));
	PACKET_GUARD_VOID(kPacket.Write(dwMasterIndex));
	PACKET_GUARD_VOID(kPacket.Write(dwEntryUserIndex));
	SendMessage(kPacket);
}

void ServerNode::OnDecreaseGuildUserCount( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex = 0;
	PACKET_GUARD_VOID(rkPacket.Read(dwGuildIndex));
	g_GuildNodeManager.GuildDecreaseUserCount(dwGuildIndex);
}

void ServerNode::OnGuildLeave( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex;
	rkPacket >> dwGuildIndex;
	g_GuildNodeManager.GuildLeaveUser( dwGuildIndex );
}

void ServerNode::OnGuildTitleChange( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex;
	ioHashString szGuildTitle;
	rkPacket >> dwGuildIndex >> szGuildTitle;
	g_GuildNodeManager.GuildTitleChange( dwGuildIndex, szGuildTitle );
}

void ServerNode::OnGuildSimpleInfo( SP2Packet &rkPacket )
{
	g_GuildNodeManager.SendCurGuildSimpleInfo( this, rkPacket );	
}

void ServerNode::OnGuildJoinUser( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex, dwGuildJoinUser;
	rkPacket >> dwGuildIndex >> dwGuildJoinUser;
	g_GuildNodeManager.GuildJoinUser( dwGuildIndex, dwGuildJoinUser );
}

void ServerNode::OnGuildMarkChange( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex, dwGuildMark;
	rkPacket >> dwGuildIndex >> dwGuildMark; 
	g_GuildNodeManager.GuildMarkChange( dwGuildIndex, dwGuildMark );
}

void ServerNode::OnGuildExist( SP2Packet &rkPacket )
{
	DWORD dwUserIndex = 0;
	ioHashString szGuildName;
	rkPacket >> dwUserIndex >> szGuildName;

	bool  bExist = false;
	DWORD dwGuildIndex = 0;
	GuildNode *pNode = g_GuildNodeManager.GetGuildNodeLowercaseExist( szGuildName );
	if( pNode )
	{
		bExist       = true;
		dwGuildIndex = pNode->GetGuildIndex();
	}

	SP2Packet kPacket( MSTPK_GUILD_EXIST );
	kPacket << dwUserIndex << bExist;
	if( bExist )
		kPacket << dwGuildIndex;
	SendMessage( kPacket );
}

void ServerNode::OnGuildMarkBlockInfo( SP2Packet &rkPacket )
{
	g_GuildNodeManager.SendGuildMarkBlockInfo( this, rkPacket );
}

void ServerNode::OnLadderModeResultUpdate( SP2Packet &rkPacket )
{
	g_CampMgr.OnLadderModeResultUpdate( rkPacket );

	enum { MAX_GUILD_SIZE = 2, };
	for(int i = 0;i < MAX_GUILD_SIZE;i++)
	{
		DWORD dwGuildIndex;
		rkPacket >> dwGuildIndex;
		if( dwGuildIndex == 0 ) continue;
		g_GuildNodeManager.OnLadderModeResult( dwGuildIndex, rkPacket );
	}	
}

void ServerNode::OnGuildTitleSync( SP2Packet &rkPacket )
{	
	g_GuildNodeManager.SendGuildTitleSync( this, rkPacket );
}

void ServerNode::OnGuildAddLadderPoint( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex;
	int   iLadderPoint;
	rkPacket >> dwGuildIndex >> iLadderPoint;
	g_GuildNodeManager.GuildAddLadderPoint( dwGuildIndex, iLadderPoint );
}

void ServerNode::OnCampRoomBattleInfo( SP2Packet &rkPacket )
{	
	g_CampMgr.SendCampRoomBattleInfo( this, rkPacket );
}

void ServerNode::OnCampDataSync( SP2Packet &rkPacket )
{
	g_CampMgr.SendCampDataSync( this, rkPacket );
}

void ServerNode::OnCampEntryChange( SP2Packet &rkPacket )
{
	g_CampMgr.ChangeCampEntryCount( rkPacket );
}

void ServerNode::OnGuildNameChange( SP2Packet &rkPacket )
{
	g_GuildNodeManager.SendGuildNameChange( this, rkPacket );
}

void ServerNode::OnResultLoadCS3File( SP2Packet &rkPacket )
{
	int iResultType     = 0;
	int iVersion     = 0;
	ioHashString     szGUID;

	rkPacket >> iResultType;
	rkPacket >> szGUID;
	rkPacket >> iVersion;
	
	MgrToolNode *pNode =  g_MgrTool.GetNode( szGUID );
	if( !pNode )
	{
		LOG.PrintTimeAndLog( 0, "ServerNode::OnResultLoadCS3File : pNode == NULL :%s", szGUID.c_str() );
		return;
	}

	SP2Packet kPacket( STPKM_LOAD_CS3_FILE );
	kPacket << iResultType;
	kPacket << m_szPrivateIP;
	kPacket << m_iClientPort;
	kPacket << iVersion;
	pNode->SendMessage( kPacket );
	LOG.PrintTimeAndLog( 0, "ServerNode::OnResultLoadCS3File : Success :%s:%d:%d", szGUID.c_str() , iResultType, iVersion );
}

void ServerNode::OnResultCS3FileVersion( SP2Packet &rkPacket )
{
	ioHashString szGUID;
	rkPacket >>  szGUID;

	MgrToolNode *pNode =  g_MgrTool.GetNode( szGUID );
	if( !pNode )
	{
		LOG.PrintTimeAndLog( 0, "ServerNode::OnResultCS3FileVersion : pNode == NULL :%s", szGUID.c_str() );
		return;
	}
	
	int iMoveSize = szGUID.Length() + 1; // +1�� string�� NULL
	SP2Packet kPacket( STPKM_CS3_FILE_VERSION );
	kPacket << m_szPrivateIP;
	kPacket << m_iClientPort;
	kPacket.SetDataAdd( (char*)rkPacket.GetData()+iMoveSize, rkPacket.GetDataSize()-iMoveSize ); // version copy
	
	pNode->SendMessage( kPacket );
	LOG.PrintTimeAndLog( 0, "ServerNode::OnResultCS3FileVersion : Success :%s", szGUID.c_str() );
}

DWORD ServerNode::GetUserCountByChannelingType( ChannelingType eChannelingType )
{
	if( eChannelingType == CNT_WEMADEBUY )
		return m_dwNormalUserCount;
	else if( eChannelingType == CNT_MGAME )
		return m_dwMgameUserCount;
	else if( eChannelingType == CNT_DAUM )
		return m_dwDaumUserCount;
	else if( eChannelingType == CNT_BUDDY )
		return m_dwBuddyUserCount;
	else if( eChannelingType == CNT_NAVER )
		return m_dwNaverUserCount;
	else if( eChannelingType == CNT_TOONILAND )
		return m_dwToonilandUserCount;
	else if( eChannelingType == CNT_NEXON )
		return m_dwNexonUserCount;
	else if( eChannelingType == CNT_HANGAME )
		return m_dwHangameUserCount;
	else if( eChannelingType == CNT_STEAM )
		return m_dwSteamUserCount;	return 0;
}

WORD ServerNode::GetCurDelaySec()
{
	DWORD dwCurDelayMs = TIMEGETTIME() - m_dwServerNoReactionTime;
	return static_cast< WORD >( ( dwCurDelayMs / 1000 ) ); // ms->sec
}

void ServerNode::OnCreateTradeReg( SP2Packet &rkPacket )
{
	g_TradeNodeManager.CreateTradeNode( rkPacket );
}

void ServerNode::OnTradeList( SP2Packet &rkPacket )
{
	g_TradeNodeManager.SendCurList( this, rkPacket );
}

void ServerNode::OnTradeItem( SP2Packet &rkPacket )
{
	int iType;
	rkPacket >> iType;

	if( iType == TRADE_ITEM_GET_INFO )
		g_TradeNodeManager.OnGetTradeItemInfo( this, rkPacket, m_dwServerIndex );
	else if( iType == TRADE_ITEM_TRADE_FAIL )
		g_TradeNodeManager.OnTradeItemFail( this, rkPacket, m_dwServerIndex );
	else if( iType == TRADE_ITEM_DEL )
		g_TradeNodeManager.OnTradeItemDel( this, rkPacket, m_dwServerIndex );
}

void ServerNode::OnTradeCancel( SP2Packet &rkPacket )
{
	int iType;
	rkPacket >> iType;

	if( iType == TRADE_ITEM_CANCEL_GET_INFO )
		g_TradeNodeManager.OnGetTradeCancelInfo( this, rkPacket, m_dwServerIndex );
	else if( iType == TRADE_ITEM_CANCEL_FAIL )
		g_TradeNodeManager.OnTradeItemFail( this, rkPacket, m_dwServerIndex );
	else if( iType == TRADE_ITEM_CANCEL_DEL )
		g_TradeNodeManager.OnTradeItemDel( this, rkPacket, m_dwServerIndex );
}

void ServerNode::OnEventShopGoodsList( SP2Packet &rkPacket )
{
	g_EventGoodsMgr.OnEventShopGoodsList( this, rkPacket );
}

void ServerNode::OnEventShopGoodsBuy( SP2Packet &rkPacket )
{
	g_EventGoodsMgr.OnEventShopGoodsBuy( this, rkPacket );
}

void ServerNode::OnEventShopGoodsBuyResult( SP2Packet &rkPacket )
{
	g_EventGoodsMgr.OnEventShopGoodsBuyResult( this, rkPacket );
}

void ServerNode::OnEventShopState( SP2Packet &rkPacket )
{
	int iShopState;
	rkPacket >> iShopState;
	g_EventGoodsMgr.SetEventShopState( iShopState );
}

void ServerNode::OnEventNpcClose( SP2Packet &rkPacket )
{
	int nNpc;
	rkPacket >> nNpc;

	SP2Packet kPacket( MSTPK_EVENT_NPC_CLOSE_REQUEST );
	kPacket << nNpc;

	g_ServerNodeManager.SendMessageAllNode( kPacket );
}

void ServerNode::OnEventShopBuyUserClear( SP2Packet &rkPacket )
{
	g_EventGoodsMgr.ClearUserBuyCount();
}

void ServerNode::OnExtraItemGrowthMortmainCheck( SP2Packet &rkPacket )
{
	int iType;
	rkPacket >> iType;

	switch( iType )
	{
	case EXTRAITEM_GROWTH_MORTMAIN_CHECK:
		{
			DWORD dwUserIndex;
			int iEtcItemType, iTargetSlot, iItemCode, iReinforce;
			rkPacket >> dwUserIndex >> iEtcItemType >> iTargetSlot >> iItemCode >> iReinforce;

			if( g_ExtraItemGrowthCatalystMgr.IsExtraItemMortmainCheck( iItemCode ) )
			{
				// ���� ����
				SP2Packet kPacket( MSTPK_EXTRAITEM_GROWTH_MORTMAIN_CHECK );
				kPacket << EXTRAITEM_GROWTH_MORTMAIN_CHECK_RESULT_OK;
				kPacket << dwUserIndex << iEtcItemType << iTargetSlot << iItemCode << iReinforce;
				SendMessage( kPacket );
				LOG.PrintTimeAndLog( 0, "EXTRAITEM_GROWTH_MORTMAIN_CHECK OK : %d - %d - %d", dwUserIndex, iItemCode, iReinforce );
			}
			else
			{
				// ���� ����
				SP2Packet kPacket( MSTPK_EXTRAITEM_GROWTH_MORTMAIN_CHECK );
				kPacket << EXTRAITEM_GROWTH_MORTMAIN_CHECK_RESULT_FAIL;
				kPacket << dwUserIndex << iEtcItemType << iTargetSlot << iItemCode << iReinforce;
				SendMessage( kPacket );
				LOG.PrintTimeAndLog( 0, "EXTRAITEM_GROWTH_MORTMAIN_CHECK FAIL : %d - %d - %d", dwUserIndex, iItemCode, iReinforce );
			}
		}
		break;
	case EXTRAITEM_GROWTH_MORTMAIN_COUNT_MINUS:
		{
			DWORD dwUserIndex;
			int iItemCode;
			rkPacket >> dwUserIndex >> iItemCode;
			g_ExtraItemGrowthCatalystMgr.MinusExtraItemMortmainCount( iItemCode );
			LOG.PrintTimeAndLog( 0, "EXTRAITEM_GROWTH_MORTMAIN_COUNT_MINUSK : %d - %d", dwUserIndex, iItemCode );
		}
		break;
	}
}

void ServerNode::OnServerPingCheck( SP2Packet &rkPacket )
{
	m_dwPingMS = (DWORD)((float)( TIMEGETTIME() - m_dwSendPingTime ) / 2);
}

void ServerNode::OnExtraItemGrowthMortmainInfo( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwItemCode, dwItemCount, dwItemDate;
	rkPacket >> dwUserIndex >> dwItemCode;

	g_ExtraItemGrowthCatalystMgr.GetExtraItemMortmainInfo( dwItemCode, dwItemCount, dwItemDate );

	SP2Packet kPacket( MSTPK_EXTRAITEM_GROWTH_CATALYST_INFO );
	kPacket << dwUserIndex << dwItemCode << dwItemCount << dwItemDate;
	SendMessage( kPacket );
}

void ServerNode::OnServerInfoAck( SP2Packet& rkPacket )
{
	ioHashString szGUID;
	rkPacket >> szGUID;

	GAMESERVERINFO	info;
	rkPacket >> info;

	//relay data 
	int relayCount = 0;
	rkPacket >> relayCount;
	
	std::vector<SendRelayInfo_> relayInfos;
	for(int i=0; i< relayCount; ++i)
	{
		SendRelayInfo_ tmpData;
		rkPacket >> tmpData;
		relayInfos.push_back(tmpData);
	}

	// Declare)
	SP2Packet	kPacket( STPKM_SERVER_INFO_ACK );
	
	// Set Data
	kPacket << (int)eServerType_GameServer;
	kPacket << info;
	// Relay Data 
	kPacket << relayCount;
	for(int i=0; i< relayInfos.size(); ++i)
	{
		kPacket << relayInfos[i];
	}

	// Send
	g_MgrTool.SendMessageGUIDNode( szGUID, kPacket );
}

void ServerNode::OnTournamentRegularInfo( SP2Packet &rkPacket )
{
	DWORD dwUserIndex;
	rkPacket >> dwUserIndex;

	TournamentNode *pTournament = g_TournamentManager.GetRegularTournament();
	if( pTournament )
	{
		SP2Packet kPacket( MSTPK_TOURNAMENT_REGULAR_INFO );
		kPacket << dwUserIndex << pTournament->GetIndex() << pTournament->GetState() << pTournament->GetRegularResourceType() << pTournament->IsDisableTournament();
		SendMessage( kPacket );
	}
	else
	{
		LOG.PrintTimeAndLog( 0, "OnTournamentRegularInfo : None Regular Tournament!!!" );
	}
}

void ServerNode::OnTournamentMainInfo( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwIndex;
	rkPacket >> dwUserIndex >> dwIndex;

	TournamentNode *pTournament = g_TournamentManager.GetTournament( dwIndex );
	if( pTournament )
	{
		SP2Packet kPacket( MSTPK_TOURNAMENT_MAIN_INFO );
		kPacket << dwUserIndex;
		pTournament->FillMainInfo( kPacket );

		SendMessage( kPacket );
	}
	else
	{
		// ����Ǿ��� �� �ִ�!! ����� ���׿� ���� ��Ŷ ����
	}
}

void ServerNode::OnTournamentListRequest( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentList( this, rkPacket );
}

void ServerNode::OnTournamentTeamCreate( SP2Packet &rkPacket )
{
	g_TournamentManager.CreateTournamentTeam( this, rkPacket );
}

void ServerNode::OnTournamentTeamInfo( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentTeamInfo( this, rkPacket );
}

void ServerNode::OnTournamentTeamDelete( SP2Packet &rkPacket )
{
	g_TournamentManager.DeleteTournamentTeam( rkPacket );
}

void ServerNode::OnTournamentTeamLadderPointAdd( SP2Packet &rkPacket )
{
	int iLadderPoint;
	DWORD dwTourIndex, dwTeamIndex;
	rkPacket >> dwTourIndex >> dwTeamIndex >> iLadderPoint;

	g_TournamentManager.SetTournamentTeamLadderPointAdd( dwTourIndex, dwTeamIndex, iLadderPoint );
}

void ServerNode::OnTournamentScheduleInfo( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentScheduleInfo( this, rkPacket );
}

void ServerNode::OnTournamentRoundTeamData( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentRoundTeamData( this, rkPacket );
}

void ServerNode::OnTournamentRoundCreateBattleRoom( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentRoundCreateBattleRoom( this, rkPacket );
}

void ServerNode::OnTournamentBattleResult( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentBattleResult( this, rkPacket );
}

void ServerNode::OnTournamentBattleTeamChange( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentRoundTeamChange( rkPacket );
}

void ServerNode::OnTournamentCustomCreate( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwTourIndex;
	rkPacket >> dwUserIndex >> dwTourIndex;

	g_DBClient.OnSelectTournamentCustomData( dwTourIndex, dwUserIndex, GetServerIndex() );
}

void ServerNode::OnTournamentTeamAllocateList( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentTeamAllocateList( this, rkPacket );
}

void ServerNode::OnTournamentTeamAllocateData( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentTeamAllocateData( this, rkPacket );
}

void ServerNode::OnTournamentJoinConfirmCheck( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentJoinConfirmCheck( this, rkPacket );
}

void ServerNode::OnTournamentJoinConfirmReg( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentJoinConfirmReg( rkPacket );
}

void ServerNode::OnTournamentAnnounceChange( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentAnnounceChange( this, rkPacket );
}

void ServerNode::OnTournamentTotalTeamList( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentTotalTeamList( this, rkPacket );
}

void ServerNode::OnTournamentCustomStateStart( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentCustomStateStart( this, rkPacket );
}

void ServerNode::OnTournamentCustomRewardList( SP2Packet &rkPacket )
{
	g_TournamentManager.SendTournamentCustomRewardList( this, rkPacket );
}

void ServerNode::OnTournamentCustomRewardRegCheck( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentCustomRewardRegCheck( this, rkPacket );
}

void ServerNode::OnTournamentCustomRewardRegUpdate( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentCustomRewardRegUpdate( rkPacket );
}

void ServerNode::OnTournamentCheerDecision( SP2Packet &rkPacket )
{
	g_TournamentManager.ApplyTournamentCheerDecision( this, rkPacket );
}

void ServerNode::OnNodeInfo( CPacket & packet ) //kyg �߰� 
{
	SP2Packet rpacket(MSTPK_NODEINFO_REQUEST);
	g_NodeInfoManager.MakeInfoPacket(rpacket);
	SendMessage(rpacket);
}

void ServerNode::OnSuperGashponLimitCheck( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwEtcItemType, dwPackageIndex, dwLimitMax;
	int iUseType;
	rkPacket >> dwUserIndex >> dwEtcItemType >> dwPackageIndex >> dwLimitMax >> iUseType;

	if( g_SuperGashponLimitManager.IncraseLimit( dwEtcItemType, dwLimitMax ) )
	{		
		SP2Packet rpacket( MSTPK_SUPER_GASHPON_LIMIT_CHECK );
		rpacket << SUPER_GASHPON_LIMIT_REMAIN;
		rpacket << dwUserIndex << dwEtcItemType << dwPackageIndex << iUseType;
		SendMessage( rpacket );
	}
	else
	{
		SP2Packet rpacket( MSTPK_SUPER_GASHPON_LIMIT_CHECK );
		rpacket << SUPER_GASHPON_LIMIT_FULL;
		rpacket << dwUserIndex << dwEtcItemType << iUseType;
		SendMessage( rpacket );
	}
}

void ServerNode::OnSuperGashponLimitDecrease( SP2Packet &rkPacket )
{
	DWORD dwEtcItemType;
	rkPacket >> dwEtcItemType;
	g_SuperGashponLimitManager.DecraseLimit( dwEtcItemType );
}

void ServerNode::OnSuperGashponLimitInfo( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwEtcItemType;
	rkPacket >> dwUserIndex >> dwEtcItemType;

	DWORD dwLimit = g_SuperGashponLimitManager.GetLimit( dwEtcItemType );

	SP2Packet rpacket( MSTPK_SUPER_GASHPON_LIMIT_INFO );
	rpacket << dwUserIndex << dwEtcItemType << dwLimit;
	SendMessage( rpacket );
}

void ServerNode::OnSuperGashponLimitReset( SP2Packet &rkPacket )
{
	DWORD dwUserIndex, dwEtcItemType;
	rkPacket >> dwUserIndex >> dwEtcItemType;

	g_SuperGashponLimitManager.LimitReset( dwEtcItemType );
}

void ServerNode::OnSpecialShopGoodsBuy( SP2Packet &rkPacket )
{
	g_SpecialShopManager.BuySpecialShopGoods(this, rkPacket);
}

void ServerNode::OnSpecialShopGoodsBuyResult( SP2Packet &rkPacket )
{
	g_SpecialShopManager.BuyResultSpecialShopGoods(this, rkPacket);
}

void ServerNode::OnReqGuildRoomIndex( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex	= 0;
	DWORD dwUserIndex	= 0;
	DWORD dwMapIndex	= 0;

	PACKET_GUARD_VOID( rkPacket.Read(dwUserIndex) );
	PACKET_GUARD_VOID( rkPacket.Read(dwGuildIndex) );
	PACKET_GUARD_VOID( rkPacket.Read(dwMapIndex) );

	int iResult = g_GuildRoomMgr.GetRoomResultByGuildIndex(dwGuildIndex);
	
	SP2Packet kPacket(MSTPK_REQ_GUILD_ROOM_INDEX);
	PACKET_GUARD_VOID( kPacket.Write(dwUserIndex) );
	PACKET_GUARD_VOID( kPacket.Write(dwGuildIndex) );
	PACKET_GUARD_VOID( kPacket.Write(dwMapIndex) );
	PACKET_GUARD_VOID( kPacket.Write(iResult) );
	
	if( GUILD_ROOM_INFO == iResult )
	{
		DWORD dwRoomIndex	= g_GuildRoomMgr.GetGuildRoomIndex(dwGuildIndex);
		PACKET_GUARD_VOID( kPacket.Write(dwRoomIndex) );
	}
	SendMessage(kPacket);
}

void ServerNode::OnUpdateGuildRoomIndex( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex	= 0;
	DWORD dwRoomIndex	= 0;

	PACKET_GUARD_VOID( rkPacket.Read(dwGuildIndex) );
	PACKET_GUARD_VOID( rkPacket.Read(dwRoomIndex) );

	g_GuildRoomMgr.UpdateGuildRoomIndex(dwGuildIndex, dwRoomIndex);
}

void ServerNode::OnDeleteGuildRoomInfo( SP2Packet &rkPacket )
{
	DWORD dwGuildIndex	= 0;
	DWORD dwRoomIndex	= 0;

	PACKET_GUARD_VOID( rkPacket.Read(dwGuildIndex) );
	PACKET_GUARD_VOID( rkPacket.Read(dwRoomIndex) );

	g_GuildRoomMgr.DeleteGuildRoomInfo(dwGuildIndex, dwRoomIndex);
}

void ServerNode::OnGuildRoomsInfo( SP2Packet &rkPacket )
{
	int iSize			= 0;
	DWORD dwGuildIndex	= 0;
	DWORD dwRoomIndex	= 0;

	PACKET_GUARD_VOID( rkPacket.Read(iSize) );

	for( int i = 0; i < iSize; i++ )
	{
		PACKET_GUARD_VOID( rkPacket.Read(dwGuildIndex) );
		PACKET_GUARD_VOID( rkPacket.Read(dwRoomIndex) );

		if( 0 == dwGuildIndex )
			continue;

		if( !g_GuildRoomMgr.AddGuildRoomInfo(dwGuildIndex) )
			continue;

		g_GuildRoomMgr.UpdateGuildRoomIndex(dwGuildIndex, dwRoomIndex);
	}

	g_GuildRoomMgr.DecreaseRequestCount();
}

void ServerNode::OnTournamentMacro(SP2Packet &rkPacket)
{
	int iState = 0;

	PACKET_GUARD_VOID( rkPacket.Read(iState) );

	if( iState >= TM_END )
		return;

	switch( iState )
	{
	case TM_NEXT_STATE:
		TestTournamentNextStep();
		break;

	case TM_RESULT:
		TestTournamentEnd();
		break;
	}
}

void ServerNode::TestTournamentNextStep()
{
	TournamentNode *pTournament = g_TournamentManager.GetRegularTournament();
	if( pTournament )
	{
		if( pTournament->GetState() >= TournamentNode::STATE_TOURNAMENT )
			pTournament->TestRoundStartProcess();

		pTournament->TestSetState(pTournament->GetState() + 1);

		if( pTournament->GetState() > pTournament->GetStateDateSize() )
			pTournament->TestSetState(TournamentNode::STATE_WAITING);
	}
}

void ServerNode::TestTournamentEnd()
{
	g_TournamentManager.InsertRegularTournamentReward(); 
	LOG.PrintTimeAndLog( 0, "[info][test]Call tournament macro");
}

// �ؿܿ� ���� ���̼��� üũ
#if defined( SRC_OVERSEAS )
//���Ӽ��� ���̼��� 30��üũ
void ServerNode::OnSendLicenseAlert( SP2Packet &rkPacket )
{
	//��Ŷ � ���� �����ؼ� 
	//���⼭ ����͸� ���� ���� -> ����͸� �� �������� �� Ȯ��

	DWORD dwResultDay, dwLicenseDate, dwLocalDate;
	ioHashString szLocalName;
	szLocalName.Clear();

	rkPacket >> dwResultDay >> dwLicenseDate >> dwLocalDate >> szLocalName;

	SP2Packet rpacket( STPKM_LICENSE_ALERT_CHECK );
	rpacket << dwResultDay << dwLicenseDate << dwLocalDate << szLocalName;
	g_MgrTool.SendMessageAllNode( rpacket );
}
#endif