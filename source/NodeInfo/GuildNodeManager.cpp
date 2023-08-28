#include "../stdafx.h"

#include "../Network/GameServer.h"
#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"
//#include "../Window.h"
#include "../ioProcessChecker.h"
#include "ServerNode.h"
#include "GuildNodeManager.h"


GuildNodeManager *GuildNodeManager::sg_Instance = NULL;
GuildNodeManager::GuildNodeManager() : m_dwCurUpdateTime( 0 ), m_dwUpdateTime( 0 ), m_dwDefaultGuildPoint( 0 ), m_dwDefaultGuildMaxEntry( 8 ), m_dwGuildLevelUPEntryGap( 4 ),
									   m_bLevelCheckReverse( false ), m_bFastUpdateGuildNode( false )
{	
}

GuildNodeManager::~GuildNodeManager()
{
	vGuildNode_iter iter, iEnd;	
	iEnd = m_vGuildNode.end();	
	for( iter=m_vGuildNode.begin() ; iter!=iEnd ; ++iter )
	{
		SAFEDELETE( *iter );
	}
	m_vGuildNode.clear();
}

GuildNodeManager &GuildNodeManager::GetInstance()
{
	if( !sg_Instance )
		sg_Instance = new GuildNodeManager;

	return *sg_Instance;
}

void GuildNodeManager::ReleaseInstance()
{
	SAFEDELETE( sg_Instance );
}

void GuildNodeManager::InitNodeINI()
{
	m_dwCurUpdateTime = TIMEGETTIME();

	ioINILoader kLoader( "config/sp2_guild.ini" );
	kLoader.SetTitle( "info" );
	m_dwUpdateTime          = kLoader.LoadInt( "UpdateTime", 0 );
	m_dwDefaultGuildPoint   = kLoader.LoadInt( "DefaultGuildPoint", 1000 );

	m_GuildLevelList.clear();
	kLoader.SetTitle( "level" );
	m_bLevelCheckReverse        = kLoader.LoadBool( "LevelCheckReverse", false );
	m_dwDefaultGuildMaxEntry    = kLoader.LoadInt( "DefaultGuildMaxEntry", 8 );	
	m_dwGuildLevelUPEntryGap    = kLoader.LoadInt( "GuildLevelUPEntryGap", 4 );	
	int iMaxLevel = kLoader.LoadInt( "MaxLevel", 0 );
	for(int i = 0;i < iMaxLevel;i++)
	{
		char szKey[MAX_PATH];
		GuildLevel kGuildLevel;
		sprintf_s( szKey, "%d_Level", i + 1 );
		kGuildLevel.m_dwLevel    = kLoader.LoadInt( szKey, 0 );
		sprintf_s( szKey, "%d_Level_Per", i + 1 );
		kGuildLevel.m_fLevelPer  = kLoader.LoadFloat( szKey, 0.0f );
		sprintf_s( szKey, "%d_Level_MaxUser", i + 1 );
		kGuildLevel.m_dwMaxEntry = kLoader.LoadInt( szKey, 8 );
		sprintf_s( szKey, "%d_Level_Bonus", i + 1 );
		kGuildLevel.m_fLevelBonus= kLoader.LoadFloat( szKey, 0.0f );
		m_GuildLevelList.push_back( kGuildLevel );

		if( kGuildLevel.m_dwLevel == 0 )
			m_GuildLevel_Default = kGuildLevel;
	}
}

GuildNode *GuildNodeManager::CreateGuildNode( SP2Packet &rkPacket )
{
	GuildNode *pGuildNode = new GuildNode;
	pGuildNode->CreateGuild( rkPacket, m_GuildLevel_Default.m_dwLevel );
    	
	// ��� ����
	ChangeGuildPointSort( pGuildNode );
	LOG.PrintTimeAndLog( 0, "CreateGuildNode : %d - %s - %s - %d - %d - %d - %d - %d - %d", pGuildNode->GetGuildIndex(), pGuildNode->GetGuildName().c_str(),
							pGuildNode->GetGuildTitle().c_str(), pGuildNode->GetGuildMark(), pGuildNode->GetGuildMaxEntry(), pGuildNode->GetGuildRegDate(),
							pGuildNode->GetGuildRank(), pGuildNode->GetGuildJoinUser(), pGuildNode->GetGuildPoint() );
	return pGuildNode;
}

GuildNode *GuildNodeManager::CreateGuildNode( CQueryResultData *pQueryData )
{
	GuildNode *pGuildNode = new GuildNode;
	if(!pGuildNode->CreateGuild( pQueryData ))
	{
		delete pGuildNode;
		return NULL;
	}
	
	// �߰�
	m_vGuildNode.push_back( pGuildNode );	
	LOG.PrintTimeAndLog( 0, "CreateGuildNode DB Load: %d - %s - %s - %d - %d - %d - %d - %d - %d", pGuildNode->GetGuildIndex(), pGuildNode->GetGuildName().c_str(),
							pGuildNode->GetGuildTitle().c_str(), pGuildNode->GetGuildMark(), pGuildNode->GetGuildMaxEntry(), pGuildNode->GetGuildRegDate(),
							pGuildNode->GetGuildRank(), pGuildNode->GetGuildJoinUser(), pGuildNode->GetGuildPoint() );
	return pGuildNode;
}

void GuildNodeManager::RemoveGuild( GuildNode *pGuild )
{
	{
		// ������Ʈ ����Ʈ���� ����
		vGuildNode_iter iter = m_vUpdateGuildNode.begin();
		for(iter = m_vUpdateGuildNode.begin();iter != m_vUpdateGuildNode.end();iter++)
		{
			GuildNode *pNode = *iter;
			if( pNode == pGuild )
			{
				m_vUpdateGuildNode.erase( iter );
				break;
			}
		}	
	}

	{
		// ��忡�� ����
		vGuildNode_iter iter = m_vGuildNode.begin();
		while( iter != m_vGuildNode.end() )
		{
			GuildNode *pNode = *iter;
			if( pNode == pGuild )
			{
				SAFEDELETE( pNode );
				m_vGuildNode.erase( iter );
				break;
			}		
			++iter;
		}
		ApplyChangeGuildRank();
	}
}

GuildNode *GuildNodeManager::GetGuildNode( DWORD dwGuildIndex )
{
	vGuildNode_iter iter = m_vGuildNode.begin();
	while( iter != m_vGuildNode.end() )
	{
		GuildNode *pNode = *iter++;
		if( pNode->GetGuildIndex() == dwGuildIndex )
		{
			return pNode;
		}		
	}
	return NULL;
}

GuildNode * GuildNodeManager::GetGuildNodeExist( const ioHashString &rszGuildName )
{
	vGuildNode_iter iter = m_vGuildNode.begin();
	while( iter != m_vGuildNode.end() )
	{
		GuildNode *pNode = *iter++;
		if( pNode->GetGuildName() == rszGuildName )
		{
			return pNode;
		}		
	}
	return NULL;
}

GuildNode * GuildNodeManager::GetGuildNodeLowercaseExist( const ioHashString &rszGuildName )
{
	vGuildNode_iter iter = m_vGuildNode.begin();
	while( iter != m_vGuildNode.end() )
	{
		GuildNode *pNode = *iter++;
		if( !pNode )
			continue;

		if( pNode->GetGuildName().Length() != rszGuildName.Length() )
			continue;

		if( _stricmp( pNode->GetGuildName().c_str(), rszGuildName.c_str() ) == 0 )
			return pNode;
	}
	return NULL;
}

float GuildNodeManager::GetGuildLevelBonus( DWORD dwLevel )
{
	int iMaxLevel = m_GuildLevelList.size();
	for(int i = 0;i < iMaxLevel;i++)
	{
		GuildLevel &kGuildLevel = m_GuildLevelList[i];
		if( kGuildLevel.m_dwLevel == dwLevel )
			return kGuildLevel.m_fLevelBonus;
	}

	LOG.PrintTimeAndLog( 0, "GetGuildLevelBonus �˼����� ��� ���� : %d ", dwLevel );		
	return 0.0f;
}

void GuildNodeManager::UpdateGuildNode( GuildNode *pGuildNode, bool bDirectSave )
{
	if( pGuildNode == NULL ) return;

	if( !pGuildNode->IsSaveReg() )
	{
		if( bDirectSave )

		{
			pGuildNode->Save();
		}
		else
		{
			pGuildNode->SetSaveReg(); // ��� ���� ���� ��ϵ�
			m_vUpdateGuildNode.push_back( pGuildNode );
		}
	}
	else if( bDirectSave )
	{	
		vGuildNode_iter iter = m_vUpdateGuildNode.begin();
		for(iter = m_vUpdateGuildNode.begin();iter != m_vUpdateGuildNode.end();iter++)
		{
			GuildNode *pNode = *iter;
			if( pNode == pGuildNode )
			{
				if( bDirectSave )
				{
					pNode->Save();
					m_vUpdateGuildNode.erase( iter );
				}
				return;
			}
		}		
	}
}

void GuildNodeManager::ChangeGuildPointSort( GuildNode *pGuildNode )
{
	/*
	�� �Լ��� ��� ����Ʈ�� ����Ǿ��� ���� �����Ѵ�.
	��� ����Ʈ +- ȹ���� �� ����	  
	*/

	// ���� ��ġ���� ����. ������ġ���� ���ٰ��ؼ� �߸��Ȱ� �ƴϴ�. 
	vGuildNode_iter iter = m_vGuildNode.begin();
	for(iter = m_vGuildNode.begin();iter != m_vGuildNode.end();iter++)
	{
		GuildNode *pNode = *iter;
		if( pNode == pGuildNode )
		{
			m_vGuildNode.erase( iter );
			break;
		}
	}	

	// ���ο� ��ġ�� �߰�
	for(iter = m_vGuildNode.begin();iter != m_vGuildNode.end();iter++)
	{
		GuildNode *pNode = *iter;
		if( pNode->GetCurGuildPoint() > pGuildNode->GetCurGuildPoint() ) continue;
		if( pNode->GetCurGuildPoint() == pGuildNode->GetCurGuildPoint() )
		{
			if( pNode->GetGuildLevel() > pGuildNode->GetGuildLevel() ) 
				continue;
			else if( pNode->GetGuildIndex() < pGuildNode->GetGuildIndex() )
				continue;
		}

		m_vGuildNode.insert( iter, pGuildNode );
		break;
	}	

	// ���� : ��ŷ�� ��� �Ǿ���.
	if( iter == m_vGuildNode.end() )
	{
		m_vGuildNode.push_back( pGuildNode );
	}

	// �ٲ� ��ŷ ����
	ApplyChangeGuildRank();
}

void GuildNodeManager::SortGuildRankAll()
{
	std::sort( m_vGuildNode.begin(), m_vGuildNode.end(), GuildSort() );
	ApplyChangeGuildRank();
}

void GuildNodeManager::ApplyChangeGuildRank()
{
	DWORD dwRank = 1;
	vGuildNode_iter iter = m_vGuildNode.begin();
	for(iter = m_vGuildNode.begin();iter != m_vGuildNode.end();iter++ )
	{
		GuildNode *pNode = *iter;
		if( pNode->GetGuildRank() != dwRank )
		{
			pNode->SetGuildRank( dwRank );
			// ���� ���μ��� ��
			UpdateGuildNode( pNode );
		}
		dwRank++;		
	}	
}

DWORD GuildNodeManager::ChangeGuildMaxUser( DWORD dwGuildIndex, DWORD dwGuildMaxUser )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildMaxEntry( dwGuildMaxUser );
		if( pGuildNode->GetGuildMaxEntry() <= pGuildNode->GetGuildJoinUser() )
		{
			// ����� �ϰ� ����
			g_DBClient.OnDeleteGuildEntryDelayMember( dwGuildIndex );
		}
		return pGuildNode->GetGuildMaxEntry();
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� �ο� ���� ��û :%d - %d", dwGuildIndex, dwGuildMaxUser );
	return 0;
}

bool GuildNodeManager::GuildEntryUserAgree( DWORD dwGuildIndex )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		if( pGuildNode->GetGuildMaxEntry() <= pGuildNode->GetGuildJoinUser() )
		{
			// ����� �ϰ� ����
			g_DBClient.OnDeleteGuildEntryDelayMember( dwGuildIndex );
			pGuildNode->SetGuildJoinUser( pGuildNode->GetGuildJoinUser() + 1 );
			return true;
		}

		pGuildNode->SetGuildJoinUser( pGuildNode->GetGuildJoinUser() + 1 );
		
// 		if( pGuildNode->GetGuildMaxEntry() <= pGuildNode->GetGuildJoinUser() )
// 		{
// 			// ����� �ϰ� ����
// 			g_DBClient.OnDeleteGuildEntryDelayMember( dwGuildIndex );
// 			pGuildNode->SetGuildMaxEntry( pGuildNode->GetGuildJoinUser() );
// 		}
		return true;
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� ���� ���� ��û :%d", dwGuildIndex );

	return false;
}

void GuildNodeManager::GuildDecreaseUserCount(DWORD dwGuildIndex)
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildJoinUser( max( 0, (int)pGuildNode->GetGuildJoinUser() - 1 ) );
		if( pGuildNode->GetGuildJoinUser() == 0 )
		{
			LOG.PrintTimeAndLog( 0, "[warning][guild]Guild user count is zero : [%d]", dwGuildIndex ); 
		}
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid Guild info : [%d]", dwGuildIndex );
}

void GuildNodeManager::GuildLeaveUser( DWORD dwGuildIndex )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildJoinUser( max( 0, (int)pGuildNode->GetGuildJoinUser() - 1 ) );
		if( pGuildNode->GetGuildJoinUser() == 0 )
		{
			// ������ ������ ��� ��ü ���� ����
			LOG.PrintTimeAndLog( 0, "[info][guild]Disband a guild : [%d] [%s] [%d] [%d] [%d] [%d] [%s] [%d] [%d] [%d] [%d] [%d] [%d] [%d] )",
									pGuildNode->GetGuildIndex(), pGuildNode->GetGuildName().c_str(), pGuildNode->GetGuildJoinUser(),
									pGuildNode->GetGuildMark(), pGuildNode->GetGuildRegDate(), pGuildNode->GetGuildMaxEntry(),
									pGuildNode->GetGuildTitle().c_str(), pGuildNode->GetGuildRank(), pGuildNode->GetGuildPoint(), pGuildNode->GetGuildLevel(),
									pGuildNode->GetGuildRecord().m_iWin, pGuildNode->GetGuildRecord().m_iLose, pGuildNode->GetGuildRecord().m_iKill,
									pGuildNode->GetGuildRecord().m_iDeath );
			g_DBClient.OnDeleteGuild( pGuildNode->GetGuildIndex() );
			RemoveGuild( pGuildNode );
		}
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid guild info : [%d]", dwGuildIndex );
}

void GuildNodeManager::GuildTitleChange( DWORD dwGuildIndex, const ioHashString &szGuildTitle )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildTitle( szGuildTitle );
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid guild info : [%d]", dwGuildIndex );
}

void GuildNodeManager::GuildJoinUser( DWORD dwGuildIndex, DWORD dwGuildJoinUser )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildJoinUser( dwGuildJoinUser );
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid guild info : [%d]", dwGuildIndex );
}

void GuildNodeManager::GuildMarkChange( DWORD dwGuildIndex, DWORD dwGuildMark )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->SetGuildMark( dwGuildMark );
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid guild info : [%d]", dwGuildIndex );
}

void GuildNodeManager::GuildAddLadderPoint( DWORD dwGuildIndex, int iAddLadderPoint )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->AddGuildPoint( iAddLadderPoint );
		ChangeGuildPointSort( pGuildNode );
	}
	else
		LOG.PrintTimeAndLog( 0, "[warning][guild]Invalid guild info : [%d]", dwGuildIndex );
}

float GuildNodeManager::GetGuildBonus( DWORD dwGuildIndex )
{
	float fGuildBonus = 0.0f;
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
		fGuildBonus = pGuildNode->GetGuildBonus();
	return fGuildBonus;
}

GuildNodeManager::GuildLevel &GuildNodeManager::CheckGuildLevel( float fGuildRankPer )
{
	if( !m_bLevelCheckReverse )
	{
		int iMaxLevel = m_GuildLevelList.size();
		for(int i = 0;i < iMaxLevel;i++)
		{
			GuildLevel &kGuildLevel = m_GuildLevelList[i];
			if( kGuildLevel.m_dwLevel == 0 ) continue;

			if( fGuildRankPer <= kGuildLevel.m_fLevelPer )
				return kGuildLevel;
		}
	}
	else
	{
		int iMaxLevel = m_GuildLevelList.size();
		for(int i = iMaxLevel - 1;i >= 0;i--)
		{
			GuildLevel &kGuildLevel = m_GuildLevelList[i];
			if( kGuildLevel.m_dwLevel == 0 ) continue;

			if( fGuildRankPer >= kGuildLevel.m_fLevelPer )
				return kGuildLevel;
		}
	}
	return m_GuildLevel_Default;        
}

void GuildNodeManager::ProcessGuildNode()
{
	FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 �� �̻� �ɸ���α� ����

	if( m_bFastUpdateGuildNode )
	{
		if( TIMEGETTIME() - m_dwCurUpdateTime < FAST_GUILD_UPDATE_TIME ) return;       // ���� ������ 5�ʸ��� �߻�
	}
	else
	{
		if( TIMEGETTIME() - m_dwCurUpdateTime < m_dwUpdateTime ) return;
	}

	ProcessUpdateGuild();	

	m_dwCurUpdateTime = TIMEGETTIME();
}

void GuildNodeManager::ServerDownAllUpdateGuild()
{
	// ������Ʈ�� �ִ� ��ü ����Ʈ�� ���ŵȴ�.
	int iSaveCount = 0;
	vGuildNode_iter iter = m_vUpdateGuildNode.begin();
	for(;iter != m_vUpdateGuildNode.end();)
	{
		GuildNode *pNode = *iter++;
		if( pNode )
			pNode->Save();    // 

		if( ++iSaveCount >= MAX_FAST_GUILD_UPDATE )
		{
			iSaveCount = 0;
			Sleep( 100 );
		}
	}	
	m_vUpdateGuildNode.clear();
}

void GuildNodeManager::ProcessUpdateGuild()
{
	// ��� ���� ����
	if( m_bFastUpdateGuildNode )
	{
		if( m_vUpdateGuildNode.empty() )
			m_bFastUpdateGuildNode = false;
		else
		{
			int iSaveCount = 0;
			vGuildNode_iter iter = m_vUpdateGuildNode.begin();
			for(;iter != m_vUpdateGuildNode.end();)
			{
				GuildNode *pNode = *iter++;
				if( pNode )
					pNode->Save();

				if( ++iSaveCount >= MAX_FAST_GUILD_UPDATE )
					break;
			}	
			m_vUpdateGuildNode.erase( m_vUpdateGuildNode.begin(), iter );      // >= <

			if( m_vUpdateGuildNode.empty() )
				m_bFastUpdateGuildNode = false;
		}
	}
	else
	{
		int iSaveCount = 0;
		vGuildNode_iter iter = m_vUpdateGuildNode.begin();
		for(;iter != m_vUpdateGuildNode.end();)
		{
			GuildNode *pNode = *iter++;
			if( pNode )
				pNode->Save();

			if( ++iSaveCount >= MAX_GUILD_UPDATE )
				break;
		}	
		m_vUpdateGuildNode.erase( m_vUpdateGuildNode.begin(), iter );          // >= <
	}
}

void GuildNodeManager::ProcessUpdateGuildLevel()
{
	// ����� ���� ����
	{
		static vGuildNode vGuildLevelNode;
		vGuildLevelNode.clear();

		vGuildNode_iter iter = m_vGuildNode.begin();
		for(iter = m_vGuildNode.begin();iter != m_vGuildNode.end();iter++)
		{
			GuildNode *pNode = *iter;
			
			// ���� ����
			if( pNode->GetCurGuildPoint() >= m_dwDefaultGuildPoint )
			{
				vGuildLevelNode.push_back( pNode );
			}
			else        // D���
			{
				if( pNode->GetGuildLevel() != m_GuildLevel_Default.m_dwLevel )
				{
					pNode->SetGuildLevel( m_GuildLevel_Default.m_dwLevel, m_GuildLevel_Default.m_dwMaxEntry );
				}
			}

			// ������ ȹ�� ����Ʈ �ʱ�ȭ
			if( pNode->GetCurGuildPoint() != 0 )
			{
				pNode->InitGuildCurPoint();
				UpdateGuildNode( pNode );
			}
		}	

		// ���� ������ �ƴ� �������Ʈ�� �⺻�̻��� ��带 ������ ������ ���
		int iTempRank = 1;
		int iRankSize = vGuildLevelNode.size();
		for(iter = vGuildLevelNode.begin();iter != vGuildLevelNode.end();iter++,iTempRank++)
		{
			GuildNode *pNode = *iter;		
			GuildLevel &rkLevel = CheckGuildLevel( ( (float)iTempRank / (float)iRankSize ) * 100.0f );
			if( pNode->GetGuildLevel() != rkLevel.m_dwLevel )
			{
				pNode->SetGuildLevel( rkLevel.m_dwLevel, rkLevel.m_dwMaxEntry );
			}
		}
		vGuildLevelNode.clear();
		
		StartFastUpdateGuild();        // ������ ����Ǿ����Ƿ� ��� ������ ������ ����.
	}
}

void GuildNodeManager::SendCurGuildList( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	DWORD dwUserIndex, dwGuildIndex;
	int   iCurPage, iMaxCount;
	rkPacket >> dwUserIndex >> dwGuildIndex >> iCurPage >> iMaxCount;

	static vGuildNode vGuildNode;
	vGuildNode.clear();

	vGuildNode_iter iter = m_vGuildNode.begin();
	for(iter = m_vGuildNode.begin();iter != m_vGuildNode.end();iter++)
	{
		GuildNode *pNode = *iter;
		if( !pNode ) continue;
		
		vGuildNode.push_back( pNode );
	}	

	int iMaxList = vGuildNode.size();
	if( iMaxList == 0 )
	{
		SP2Packet kPacket( MSTPK_GUILD_RANK_LIST );
		kPacket << dwUserIndex << 0 << 0 << 0;
		pServerNode->SendMessage( kPacket );
		return;
	}

	// Ư�� ����� �������� ȣ���Ѵ�.
	if( dwGuildIndex != 0 && iMaxCount != 0 )
	{
		GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
		if( pGuildNode )
			iCurPage = max( pGuildNode->GetGuildRank() - 1, 0 )  / iMaxCount;
	}

	int iStartPos = iCurPage * iMaxCount;
	int iCurSize  = max( 0, min( iMaxList - iStartPos, iMaxCount ) );

	SP2Packet kPacket( MSTPK_GUILD_RANK_LIST );
	kPacket << dwUserIndex << iCurPage << iMaxList << iCurSize;
	for(int i = iStartPos; i < iStartPos + iCurSize;i++)
	{
		GuildNode *pGuildNode = vGuildNode[i];
		if( pGuildNode )
		{
			kPacket << pGuildNode->GetGuildIndex() << pGuildNode->GetGuildRank() << pGuildNode->GetGuildLevel() << pGuildNode->GetGuildMark()
					<< pGuildNode->GetGuildName() << pGuildNode->GetGuildJoinUser() << pGuildNode->GetGuildMaxEntry() << pGuildNode->GetGuildBonus() << pGuildNode->GetCurGuildPoint();
		}
		else    //����
		{
			kPacket << 0 << 0 << 0 << 0 << "" << 0 << 0 << 1.0f << 0;
		}
	}	
	pServerNode->SendMessage( kPacket );
	vGuildNode.clear();
}

void GuildNodeManager::SendCurGuildInfo( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	bool bLoginInfo = false;
	DWORD dwUserIndex = 0, dwGuildIndex = 0;
	rkPacket >> bLoginInfo >> dwUserIndex >> dwGuildIndex;

	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		SP2Packet kPacket( MSTPK_GUILD_INFO );
		kPacket << bLoginInfo << dwUserIndex << dwGuildIndex;
		kPacket << pGuildNode->GetGuildMark() << pGuildNode->GetGuildRank() << pGuildNode->GetGuildLevel() << pGuildNode->GetGuildJoinUser()
			    << pGuildNode->GetGuildMaxEntry() << pGuildNode->GetGuildPoint() << pGuildNode->GetGuildRegDate() << pGuildNode->GetGuildBonus()
				<< pGuildNode->GetGuildName() << pGuildNode->GetGuildTitle() << pGuildNode->GetGuildPoint() << pGuildNode->GetCurGuildPoint();
		pServerNode->SendMessage( kPacket );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� ���� ��û 1:%d - %d", dwUserIndex, dwGuildIndex );
}

void GuildNodeManager::SendCurGuildSimpleInfo( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	DWORD dwUserIndex, dwGuildIndex;
	ioHashString szGuildUserID;
	rkPacket >> dwUserIndex >> szGuildUserID >> dwGuildIndex;

	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		SP2Packet kPacket( MSTPK_GUILD_SIMPLE_INFO );
		kPacket << dwUserIndex << szGuildUserID << dwGuildIndex << pGuildNode->GetGuildName() << pGuildNode->GetGuildMark();
		pServerNode->SendMessage( kPacket );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� ���� ��û 2:%d - %d", dwUserIndex, dwGuildIndex );
}

void GuildNodeManager::SendGuildMarkBlockInfo( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	ioHashString szDeveloperID, szGuildName;
	rkPacket >> szDeveloperID >> szGuildName;

	GuildNode *pGuildNode = GetGuildNodeExist( szGuildName );
	if( pGuildNode )
	{
		SP2Packet kPacket( MSTPK_GUILD_MARK_BLOCK_INFO );
		kPacket << szDeveloperID << pGuildNode->GetGuildIndex() << pGuildNode->GetGuildMark();
		pServerNode->SendMessage( kPacket );
		LOG.PrintTimeAndLog( 0, "%s�Բ��� %s����� ��帶ũ �� ��û!!", szDeveloperID.c_str(), szGuildName.c_str() );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� ��ũ �� ���� ��û :%s - %s", szDeveloperID.c_str(), szGuildName.c_str() );
}

void GuildNodeManager::SendGuildTitleSync( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	DWORD dwUserIndex, dwGuildIndex;
	rkPacket >> dwUserIndex >> dwGuildIndex;
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		SP2Packet kPacket( MSTPK_GUILD_TITLE_SYNC );
		kPacket << dwUserIndex << pGuildNode->GetGuildIndex() << pGuildNode->GetGuildTitle();
		pServerNode->SendMessage( kPacket );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ��� Ÿ��Ʋ ��û :%d - %d", dwUserIndex, dwGuildIndex );
}

void GuildNodeManager::SendGuildNameChange( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode )
		return;

	DWORD dwGuildIndex;
	ioHashString szNewGuildName;
	rkPacket >> dwGuildIndex >> szNewGuildName;

	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		pGuildNode->ChangeGuildName( szNewGuildName );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ���� ���� ��û :%d - %s", dwGuildIndex, szNewGuildName.c_str() );
}

void GuildNodeManager::OnLadderModeResult( DWORD dwGuildIndex, SP2Packet &rkPacket )
{
	GuildNode *pGuildNode = GetGuildNode( dwGuildIndex );
	if( pGuildNode )
	{
		int iWinLoseTieType, iWinLoseTiePoint;
		GUILDRECORD kGuildRecord;
		rkPacket >> iWinLoseTieType >> iWinLoseTiePoint >> kGuildRecord.m_iKill >> kGuildRecord.m_iDeath;
		switch( iWinLoseTieType )
		{
		case LADDER_TEAM_RESULT_WIN:
			kGuildRecord.m_iWin = iWinLoseTiePoint;
			break;
		case LADDER_TEAM_RESULT_LOSE:
			kGuildRecord.m_iLose = iWinLoseTiePoint;
			break;
		}
		pGuildNode->AddGuildRecord( kGuildRecord.m_iWin, kGuildRecord.m_iLose, kGuildRecord.m_iKill, kGuildRecord.m_iDeath );
		LOG.PrintTimeAndLog( 0, "OnLadderModeResult : %s(%d - %d - %d - %d)", pGuildNode->GetGuildName().c_str(),
								kGuildRecord.m_iWin, kGuildRecord.m_iLose, kGuildRecord.m_iKill, kGuildRecord.m_iDeath );
	}
	else
		LOG.PrintTimeAndLog( 0, "�˼����� ���� ������Ʈ ��û :%d", dwGuildIndex  );
}