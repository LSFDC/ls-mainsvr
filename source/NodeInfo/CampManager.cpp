#include "../stdafx.h"

#include "../ioProcessChecker.h"
#include "../Network/GameServer.h"
#include "../QueryData/QueryResultData.h"
#include "../DataBase/DBClient.h"

#include "../EtcHelpFunc.h"

#include "GuildNodeManager.h"
#include "ServerNode.h"
#include "ServerNodeManager.h"
#include "TournamentManager.h"
#include "CampManager.h"


CampManager *CampManager::sg_Instance = NULL;

CampManager::CampManager() : m_dwProcessTime( 0 ), m_dwRecessTime( 0 )
{
	memset( m_szStringHelp, 0, sizeof( m_szStringHelp ) );
	m_SeasonState = SS_NONE;
	m_RecessState = RECESS_NONE;
}

CampManager::~CampManager()
{
	m_CampData.clear();
}

CampManager &CampManager::GetInstance()
{
	if( !sg_Instance )
		sg_Instance = new CampManager;

	return *sg_Instance;
}

void CampManager::ReleaseInstance()
{
	SAFEDELETE( sg_Instance );
}

void CampManager::LoadINIData()
{
	ioINILoader kLoader( "config/sp2_camp.ini" );
		
	kLoader.SetTitle( "CampData" );	
	int i;
	int iMaxCamp = kLoader.LoadInt( "MaxCamp", 0 );
	char szKey[MAX_PATH], szBuf[MAX_PATH];
	for(i = 0;i < iMaxCamp;i++)
	{
		CampData kCampData;

		sprintf_s( szKey, "Camp%d_Type", i + 1 );
		kCampData.m_iCampType = kLoader.LoadInt( szKey, 0 );

		sprintf_s( szKey, "Camp%d_Name", i + 1 );
		kLoader.LoadString( szKey, "", szBuf, MAX_PATH );
		kCampData.m_szCampName = szBuf;

		m_CampData.push_back( kCampData );
		LOG.PrintTimeAndLog( 0,"Camp Load INI : %d:%s", kCampData.m_iCampType, kCampData.m_szCampName.c_str() );
	}

	kLoader.SetTitle( "Alarm" );
	m_Alarm.m_fCheckList.clear();
	int iMaxCheck = kLoader.LoadInt( "MaxCheck", 0 );
	for(i = 0;i < iMaxCheck;i++)
	{
		sprintf_s( szKey, "Check_%d", i + 1 );
		m_Alarm.m_fCheckList.push_back( kLoader.LoadFloat( szKey, 0.0f ) );
	}
	m_Alarm.IsChange( 0.5f );

	LoadSeasonDateINI();

	LOG.PrintTimeAndLog( 0, "Camp Load INI : %u - %d - %d - %d - %d", m_SeasonDate.m_dwLastSeasonDate, m_SeasonDate.m_wPeriodTime, m_SeasonDate.m_wRecessTime, m_SeasonDate.m_wPlayStartHour, m_SeasonDate.m_wPlayPreceedTime );
	ProcessSeason( SS_NONE );
	UpdateCampStringHelp();
}

void CampManager::LoadSeasonDateINI( bool bReload )
{
	ioINILoader kLoader( "config/sp2_camp.ini" );

	bool bReloadSuccess = false;
	kLoader.SetTitle( "SeasonDate" );
	if( bReload )
	{
		if( kLoader.ReloadFile( "config/sp2_camp.ini" ) )
		{
			LOG.PrintTimeAndLog(0, "%s - INI file reload sucess", __FUNCTION__ );
			bReloadSuccess = true;
		}
		else
		{
			LOG.PrintTimeAndLog(0, "%s - INI file reload failed!!", __FUNCTION__ );
			return;
		}
	}
	else
		m_SeasonDate.m_dwLastSeasonDate = kLoader.LoadInt( "LastSeasonDate", 0 );

	//m_SeasonDate.m_dwLastSeasonDate = kLoader.LoadInt( "LastSeasonDate", 0 );
	m_SeasonDate.m_wPeriodTime		= (WORD)kLoader.LoadInt( "PeriodTime", 0 );
	m_SeasonDate.m_wRecessTime		= (WORD)max( 1, kLoader.LoadInt( "RecessTime", 0 ) );
	m_SeasonDate.m_wPlayStartHour   = (WORD)kLoader.LoadInt( "PlayStartHour", 0 );
	m_SeasonDate.m_wPlayPreceedTime = (WORD)kLoader.LoadInt( "PlayPreceedTime", 0 );

	if( bReloadSuccess )
	{
		LOG.PrintTimeAndLog(0, "%s Reload Value - LastSeasonDate : %d, PeriodTime : %d, RecessTime : %d, PlayStartHour : %d, PlayPreceedTime : %d", __FUNCTION__, 
			m_SeasonDate.m_dwLastSeasonDate,
			m_SeasonDate.m_wPeriodTime,
			m_SeasonDate.m_wRecessTime,
			m_SeasonDate.m_wPlayStartHour,
			m_SeasonDate.m_wPlayPreceedTime
		);
	}
}

void CampManager::SaveSeasonDateINI()
{
	ioINILoader kLoader( "config/sp2_camp.ini" );
	kLoader.SaveInt( "SeasonDate", "LastSeasonDate", m_SeasonDate.m_dwLastSeasonDate );	
}

void CampManager::DBToCampData( int iBlueCampPoint, int iBlueCampTodayPoint, int iBlueCampBonusPoint, int iRedCampPoint, int iRedCampTodayPoint, int iRedCampBonusPoint )
{
	//
	{
		CampData &rkBlueCampData = GetCampData( CAMP_BLUE );
		rkBlueCampData.m_iCampPoint      = iBlueCampPoint;
		rkBlueCampData.m_iCampTodayPoint = iBlueCampTodayPoint;
		rkBlueCampData.m_iCampBonusPoint = iBlueCampBonusPoint;
		rkBlueCampData.BackUP();

		CampData &rkRedCampData = GetCampData( CAMP_RED );
		rkRedCampData.m_iCampPoint      = iRedCampPoint;
		rkRedCampData.m_iCampTodayPoint = iRedCampTodayPoint;
		rkRedCampData.m_iCampBonusPoint = iRedCampBonusPoint;
		rkRedCampData.BackUP();

		LOG.PrintTimeAndLog( 0, "CampManager::DBToCampData : BLUE(%d,%d,%d) - RED(%d,%d,%d)",
								rkBlueCampData.m_iCampPoint, rkBlueCampData.m_iCampTodayPoint, rkBlueCampData.m_iCampBonusPoint,
								rkRedCampData.m_iCampPoint, rkRedCampData.m_iCampTodayPoint, rkRedCampData.m_iCampBonusPoint );
	}

	int iBluePoint = GetCampPoint( CAMP_BLUE ) + GetCampBonusPoint( CAMP_BLUE );
	int iRedPoint = GetCampPoint( CAMP_RED ) + GetCampBonusPoint( CAMP_RED );
	int iTotalPoint = max( 1, iBluePoint ) + max( 1, iRedPoint );
	float fBlueInfluence = ( (float)max( 1, iBluePoint ) / (float)iTotalPoint ) + 0.005f;
	m_Alarm.IsChange( fBlueInfluence );
	UpdateCampStringHelp();

	LOG.PrintTimeAndLog( 0, "CampManager::DBToCampData : BLUE(%d) - RED(%d)", iBluePoint, iRedPoint );
}

void CampManager::DBToCampSpecialUserCount( int iCampType, int iCampSpecialEntry )
{
	CampData &rkCampData = GetCampData( (CampType)iCampType );
	if( rkCampData.m_iCampType != iCampType )
	{
		LOG.PrintTimeAndLog( 0, "DBToCampSpecialUserCount ���� ���� Ÿ�� : %d - %d", iCampType, iCampSpecialEntry );
		return;
	}
	rkCampData.m_iCampSpecialEntryUserCount = iCampSpecialEntry;
	rkCampData.m_iCampEntryUserCount = max( rkCampData.m_iCampEntryUserCount, iCampSpecialEntry );
	UpdateCampStringHelp();
	LOG.PrintTimeAndLog( 0, "DBToCampSpecialUserCount %d - %d", iCampType, iCampSpecialEntry );
}

void CampManager::UpdateCampDataDB()
{
	CampData &rkBlueCampData = GetCampData( CAMP_BLUE );	
	CampData &rkRedCampData = GetCampData( CAMP_RED );

	if( rkBlueCampData.m_iCampType == CAMP_NONE ||  rkRedCampData.m_iCampType == CAMP_NONE )
	{
		LOG.PrintTimeAndLog( 0, "UpdateCampDataDB ���� ���� Ÿ�� ������Ʈ����: %d - %d", rkBlueCampData.m_iCampType, rkRedCampData.m_iCampType );
		return;
	}

	if( rkBlueCampData.IsChange() || rkRedCampData.IsChange() )
	{
		g_DBClient.OnUpdateCampData( rkBlueCampData.m_iCampPoint, rkBlueCampData.m_iCampTodayPoint, rkBlueCampData.m_iCampBonusPoint,
									 rkRedCampData.m_iCampPoint, rkRedCampData.m_iCampTodayPoint, rkRedCampData.m_iCampBonusPoint );
		//
		rkBlueCampData.BackUP();
		rkRedCampData.BackUP();
		LOG.PrintTimeAndLog( 0, "CampManager::UpdateCampDataDB : BLUE(%d,%d,%d) - RED(%d,%d,%d)",
								rkBlueCampData.m_iCampPoint, rkBlueCampData.m_iCampTodayPoint, rkBlueCampData.m_iCampBonusPoint,
								rkRedCampData.m_iCampPoint, rkRedCampData.m_iCampTodayPoint, rkRedCampData.m_iCampBonusPoint );
	}
}

void CampManager::ProcessSeason( SeasonState eSeasonState )
{
	// 1�и��� Call
	switch( eSeasonState )
	{
	case SS_NONE:
		{
			CTime cCurTime = CTime::GetCurrentTime();
			CTimeSpan cRecessTime( (m_SeasonDate.m_wRecessTime / 24), (m_SeasonDate.m_wRecessTime % 24), 0, 0 );
			CTime cCheckTime;
			{
				// INI�� 0.0.0.0���� �Ǿ������� ���� �ð����� ����
				if( m_SeasonDate.m_dwLastSeasonDate == 0 )
				{
					m_SeasonDate.SetLastSeasonTime( cCurTime );
					cCheckTime = m_SeasonDate.GetLastSeasonTime() - cRecessTime; 
				}
				else
				{
					cCheckTime = m_SeasonDate.GetLastSeasonTime() + cRecessTime; 
				}
			}			

			if( cCurTime > cCheckTime )      // ���� ������
			{
				CTimeSpan cPeriodTime( (m_SeasonDate.m_wPeriodTime / 24), (m_SeasonDate.m_wPeriodTime % 24), 0, 0 );
				cCheckTime = m_SeasonDate.GetLastSeasonTime() + cPeriodTime + cRecessTime;
				m_SeasonState = (SeasonState)GetCurrentPlayState();
			}
			else                             // ���� �޽���
			{
				m_SeasonState = SS_RECESS;
			}
			m_SeasonDate.SetNextSeasonTime( cCheckTime );
			LOG.PrintTimeAndLog( 0, "Camp SetNextSeasonTime : %u - %d", m_SeasonDate.m_dwNextSeasonDate, (int)m_SeasonState );
		}
		break;
	case SS_PLAY_DELAY:
	case SS_PLAY_PROCEED:
		{
			CTime cCurTime = CTime::GetCurrentTime();
			CTime cNextTime= m_SeasonDate.GetNextSeasonTime();
			if( cCurTime > cNextTime )
			{
				SeasonEnd();
				UpdateCampStringHelp();
			}
			else
			{
				DWORD dwPrevState = m_SeasonState;
				m_SeasonState = (SeasonState)GetCurrentPlayState();
				if( dwPrevState != m_SeasonState )
				{
					LOG.PrintTimeAndLog( 0, "Change Play State : %d - > %d", dwPrevState, (int)m_SeasonState );
					if( m_SeasonState == SS_PLAY_DELAY )
					{
						// ������ �º� ����.
						SeasonTodayPointEnd();
					}
					// ��ü ������ �˸�
					SP2Packet kPacket( MSTPK_CAMP_BATTLE_INFO );
					kPacket << IsCampBattlePlay();
					kPacket << GetActiveCampDate();
					g_ServerNodeManager.SendMessageAllNode( kPacket );
					UpdateCampStringHelp();
					//LOG.PrintTimeAndLog( 0, "SS_PLAY_PROCEED : Active camp Date : %d",  GetActiveCampDate() ); //������
				}
			}
		}
		break;
	case SS_RECESS:
		{
			CTime cCurTime = CTime::GetCurrentTime();
			CTime cNextTime= m_SeasonDate.GetNextSeasonTime();
			if( cCurTime > cNextTime )
			{
				SeasonStart( cNextTime );
				UpdateCampStringHelp();
			}
			else if( m_dwRecessTime != 0 )
			{
				switch( m_RecessState )
				{				
				case RECESS_FIRST_BACKUP:
					if( TIMEGETTIME() - m_dwRecessTime > 300000 )     //5Min
					{
						g_DBClient.OnEndCampSeasonFirstBackup();
						m_RecessState  = RECESS_RESULT;
						m_dwRecessTime = TIMEGETTIME();
					}
					break;
				case RECESS_RESULT:
					if( TIMEGETTIME() - m_dwRecessTime > 300000 )     //5Min
					{
						SeasonResult();
						m_RecessState  = RECESS_LAST_BACKUP;
						m_dwRecessTime = TIMEGETTIME();
					}
					break;
				case RECESS_LAST_BACKUP:
					if( TIMEGETTIME() - m_dwRecessTime > 300000 )     //5Min
					{
						g_DBClient.OnEndCampSeasonLastBackup();
						m_RecessState  = RECESS_INIT;
						m_dwRecessTime = TIMEGETTIME();
					}
					break;
				case RECESS_INIT:
					if( TIMEGETTIME() - m_dwRecessTime > 1800000 )     //30Min
					{
						SeasonPrepare();
						m_RecessState  = RECESS_NONE;
						m_dwRecessTime = 0;
					}
					break;
				}
			}
		}
		break;
	}
}

void CampManager::SeasonStart( const CTime &rkNextTime )
{
	m_SeasonState = (SeasonState)GetCurrentPlayState();		
	{
		// �¶��� ���� ���� ����Ʈ �ʱ�ȭ
		SP2Packet kPacket( MSTPK_USER_CAMP_POINT_RECORD_INIT );
		g_ServerNodeManager.SendMessageAllNode( kPacket );
	}

	{
		// ���� ������ ������ ���� �˸�
		SP2Packet kPacket( MSTPK_CAMP_BATTLE_INFO );
		kPacket << IsCampBattlePlay();
		kPacket << GetActiveCampDate();
		//LOG.PrintTimeAndLog( 0, "SeasonStart : Active camp Date : %d",  GetActiveCampDate() ); //������
		g_ServerNodeManager.SendMessageAllNode( kPacket );
	}	
	CTimeSpan cPeriodTime( (m_SeasonDate.m_wPeriodTime / 24), (m_SeasonDate.m_wPeriodTime % 24), 0, 0 );
	m_SeasonDate.SetNextSeasonTime( rkNextTime + cPeriodTime );
	LOG.PrintTimeAndLog( 0, "Camp SetNextSeasonTime : %u - %d", m_SeasonDate.m_dwNextSeasonDate, (int)m_SeasonState );
}

void CampManager::SeasonEnd()
{
	LOG.PrintTimeAndLog( 0, "Camp Season End : %u ~ %u", m_SeasonDate.m_dwLastSeasonDate, m_SeasonDate.m_dwNextSeasonDate );

	// �޽Ľð� ����
	DWORD dwPrevDate = m_SeasonDate.m_dwLastSeasonDate;
	m_SeasonDate.m_dwLastSeasonDate = m_SeasonDate.m_dwNextSeasonDate;
	m_SeasonState = SS_RECESS;
	m_RecessState = RECESS_FIRST_BACKUP;
	m_dwRecessTime= TIMEGETTIME();	
	{
		// ����� �Ⱓ �ݿ�
		LoadSeasonDateINI( true );

		// ���� �ð� ���
		SaveSeasonDateINI();

		// ���� ������ ������ ���� �˸�
		SP2Packet kPacket( MSTPK_CAMP_BATTLE_INFO );
		kPacket << IsCampBattlePlay();
		kPacket << dwPrevDate;
		g_ServerNodeManager.SendMessageAllNode( kPacket );
		//LOG.PrintTimeAndLog( 0, "SeasonEnd : Active camp Date : %d",  GetActiveCampDate() ); //������
	}
	CTimeSpan cRecessTime( (m_SeasonDate.m_wRecessTime / 24), (m_SeasonDate.m_wRecessTime % 24), 0, 0 );
	m_SeasonDate.SetNextSeasonTime( m_SeasonDate.GetLastSeasonTime() + cRecessTime );
	LOG.PrintTimeAndLog( 0, "Camp SetNextSeasonTime : %u - %d", m_SeasonDate.m_dwNextSeasonDate, (int)m_SeasonState );

	// ���� ���� ������Ʈ
	UpdateCampDataDB();
}

void CampManager::SeasonResult( bool bServerClose /* = false  */ )
{
	// ���� ���� ó�� 
	
	// ���� ���� ������Ʈ
	UpdateCampDataDB();

	// ���� ��ȸ ���� å��
	g_TournamentManager.InsertRegularTournamentReward();
	
	// ��� ��ŷ
	SP2Packet kPacket( MSTPK_CAMP_BATTLE_INFO );
	kPacket << IsCampBattlePlay();
	kPacket << GetActiveCampDate();
	g_ServerNodeManager.SendMessageAllNode( kPacket );

	g_GuildNodeManager.ProcessUpdateGuildLevel();
	LOG.PrintTimeAndLog( 0, "CampManager::SeasonResult Update GuildLevel" );

	// DB�� ���� ���� ó�� ��û
	if( bServerClose )
	{
		g_DBClient.OnEndCampSeasonServerClose();
		LOG.PrintTimeAndLog( 0, "CampManager::SeasonResult Update OnEndCampSeasonServerClose Call" );
	}
	else
	{
		g_DBClient.OnEndCampSeasonProcess();
		LOG.PrintTimeAndLog( 0, "CampManager::SeasonResult Update OnEndCampSeasonProcess Call" );
	}
	//
	LOG.PrintTimeAndLog( 0, "CampManager::SeasonResult End" );
}

void CampManager::SeasonPrepare( bool bServerClose /* = false  */ )
{
	// ���� ���� �غ�	
	if( !bServerClose )
	{
		// �������� ���� ���� ����Ʈ �ʱ�ȭ
		g_DBClient.OnInitCampSeason();
		LOG.PrintTimeAndLog( 0, "CampManager::SeasonPrepare OnInitCampSeason Call" );
	}
	else
	{
		LOG.PrintTimeAndLog( 0, "CampManager::SeasonPrepare OnInitCampSeason �Լ��� ���� ����� ���� ȣ����� ����" );
	}

	{		
		// ������ ������ �ʱ�ȭ
		int iSize = m_CampData.size();
		for(int i = 0;i < iSize;i++)
		{
			CampData &rkCampData = m_CampData[i];
			rkCampData.m_iCampSpecialEntryUserCount = rkCampData.m_iCampEntryUserCount = 0;
		}

		// �¶��� ���� ���� ����Ʈ �ʱ�ȭ
		SP2Packet kPacket( MSTPK_USER_CAMP_POINT_RECORD_INIT );
		g_ServerNodeManager.SendMessageAllNode( kPacket );
	}

	{
		// ������ ���� �ʱ�ȭ
		int iSize = m_CampData.size();
		for(int i = 0;i < iSize;i++)
		{
			CampData &rkCampData = m_CampData[i];
			rkCampData.m_iCampPoint = 0;
			rkCampData.m_iCampBonusPoint = 0;
			rkCampData.m_iCampTodayPoint = 0;
		}
		LOG.PrintTimeAndLog( 0, "CampManager::SeasonPrepare Init Camp CampPoint" );
	}

	UpdateCampStringHelp();
	LOG.PrintTimeAndLog( 0, "CampManager::SeasonPrepare End" );
}

CampManager::CampData &CampManager::GetCampData( CampType eCampType )
{
	int iSize = m_CampData.size();
	for(int i = 0;i < iSize;i++)
	{
		CampData &rkCampData = m_CampData[i];
		if( (CampType)rkCampData.m_iCampType == eCampType )
			return rkCampData;
	}

	LOG.PrintTimeAndLog( 0, "CampManager::GetCampData �� ������ ���� Ÿ�� : %d", (int)eCampType );
	static CampData kTemp;
	return kTemp;
}

int CampManager::GetCampPoint( CampType eCampType )
{
	CampData &rkCampData = GetCampData( eCampType );
	return rkCampData.m_iCampPoint;
}

int CampManager::GetCampBonusPoint( CampType eCampType )
{
	CampData &rkCampData = GetCampData( eCampType );
	return rkCampData.m_iCampBonusPoint;
}

char *CampManager::GetCampStringHelp()
{
	return m_szStringHelp;
}

void CampManager::UpdateCampStringHelp()
{
	memset( m_szStringHelp, 0, sizeof( m_szStringHelp ) );
	char szBuf[MAX_PATH] = "";
	int iSize = m_CampData.size();
	for(int i = 0;i < iSize;i++)
	{
		CampData &rkCampData = m_CampData[i];
		sprintf_s( szBuf, "%s(%d:%d) ", rkCampData.m_szCampName.c_str(), rkCampData.m_iCampPoint, rkCampData.m_iCampEntryUserCount );
		strcat_s( m_szStringHelp, szBuf );
	}
	switch( m_SeasonState )
	{
	case SS_PLAY_DELAY:
		{
			sprintf_s( szBuf, " :��������(%d ~ %d) ", m_SeasonDate.m_dwLastSeasonDate, m_SeasonDate.m_dwNextSeasonDate );
			strcat_s( m_szStringHelp, szBuf );
		}
		break;
	case SS_PLAY_PROCEED:
		{
			sprintf_s( szBuf, " :������(%d ~ %d) ", m_SeasonDate.m_dwLastSeasonDate, m_SeasonDate.m_dwNextSeasonDate );
			strcat_s( m_szStringHelp, szBuf );
		}
		break;
	case SS_RECESS:
		{
			sprintf_s( szBuf, " :�޽���(%d)(%d ~ %d) ", (int)m_RecessState, m_SeasonDate.m_dwLastSeasonDate, m_SeasonDate.m_dwNextSeasonDate );
			strcat_s( m_szStringHelp, szBuf );
		}
		break;
	}
}

DWORD CampManager::GetCurrentPlayState()
{
	CTime cCurTime = CTime::GetCurrentTime();	
	DWORD dwMinHour = m_SeasonDate.m_wPlayStartHour;
	DWORD dwMaxHour = m_SeasonDate.m_wPlayStartHour + m_SeasonDate.m_wPlayPreceedTime;	
	// ���� �ð��� ������ �Ѿ�� üũ�� ���� �������. �װ��� % 24.
	if( COMPARE( cCurTime.GetHour(), (int)dwMinHour, (int)dwMaxHour ) )
	{
		return SS_PLAY_PROCEED;
	}	
	else if( dwMaxHour > 24 && cCurTime.GetHour() < (int)dwMaxHour % 24 ) 
	{
		return SS_PLAY_PROCEED;
	}
	return SS_PLAY_DELAY;
}

int CampManager::GetNextTodayBattleSec()
{
	if( m_SeasonState != SS_PLAY_PROCEED ) return -1;
	
	// ���� �ʸ� �����Ѵ�.
	CTime cCurTime = CTime::GetCurrentTime();	
	CTime cStartTime( cCurTime.GetYear(), cCurTime.GetMonth(), cCurTime.GetDay(), m_SeasonDate.m_wPlayStartHour, 0, 0 );

	{   // �Ϸ簡 �������� Ȯ���Ͽ� �Ϸ簡 �������� ���� �ð��� �Ϸ������� �ٲ۴�.
		DWORD dwEndTime = m_SeasonDate.m_wPlayStartHour+m_SeasonDate.m_wPlayPreceedTime;
		int iStartMinusDay = 0;
		if( dwEndTime > 24 && cCurTime.GetHour() < (int)dwEndTime % 24 )
			iStartMinusDay = 1;
		CTimeSpan cStartMinusTime( iStartMinusDay, 0, 0, 0 );
		cStartTime = cStartTime - cStartMinusTime;
	}

	CTimeSpan cGapTime( 0, m_SeasonDate.m_wPlayPreceedTime, 0, 0 );
	CTime cEndTime = cStartTime + cGapTime;
	cGapTime = cEndTime - cCurTime;
	return (int)max( 0, cGapTime.GetTotalSeconds() );
}

void CampManager::SendCampDataSync( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode ) return;

	SeasonDate kStartSeason;     //���� �ð��� ����Ͽ� �־��ش�.
	if( m_SeasonState == SS_PLAY_DELAY || m_SeasonState == SS_PLAY_PROCEED )
	{
		CTimeSpan cRecessTime( (m_SeasonDate.m_wRecessTime / 24), (m_SeasonDate.m_wRecessTime % 24), 0, 0 );
		kStartSeason.SetLastSeasonTime( m_SeasonDate.GetLastSeasonTime() + cRecessTime );
	}
	else
	{
		kStartSeason.m_dwLastSeasonDate = m_SeasonDate.m_dwLastSeasonDate;
	}

	DWORD dwUserIndex;
	rkPacket >> dwUserIndex;

	SP2Packet kPacket( MSTPK_CAMP_DATA_SYNC );
	kPacket << dwUserIndex;
	kPacket << (int)m_SeasonState << kStartSeason.m_dwLastSeasonDate << m_SeasonDate.m_dwNextSeasonDate;

	// ���� ���� ����Ʈ�� ���� �ð��� �������ش�. 10�� ����
	{
		kPacket << m_SeasonDate.m_wPlayStartHour << m_SeasonDate.m_wPlayPreceedTime << GetNextTodayBattleSec();
	}
	
	int iSize = m_CampData.size();
	kPacket << iSize;
	for(int i = 0;i < iSize;i++)
	{
		CampData &rkCampData = m_CampData[i];
		kPacket << rkCampData.m_iCampType << rkCampData.m_iCampPoint << rkCampData.m_iCampTodayPoint << rkCampData.m_iCampBonusPoint
			    << rkCampData.m_iCampEntryUserCount << rkCampData.m_iCampSpecialEntryUserCount;
	}
	pServerNode->SendMessage( kPacket );
}

void CampManager::SendCampRoomBattleInfo( ServerNode *pServerNode, SP2Packet &rkPacket )
{
	if( !pServerNode ) 
		return;

	DWORD dwRoomIndex, dwGuildIndex1, dwGuildIndex2;
	rkPacket >> dwRoomIndex >> dwGuildIndex1 >> dwGuildIndex2;

	SP2Packet kPacket( MSTPK_CAMP_ROOM_BATTLE_INFO );
	kPacket << dwRoomIndex << GetCampPoint( CAMP_BLUE ) + GetCampBonusPoint( CAMP_BLUE ) << GetCampPoint( CAMP_RED ) + GetCampBonusPoint( CAMP_RED ) 
			<< dwGuildIndex1 << g_GuildNodeManager.GetGuildBonus( dwGuildIndex1 ) 
			<< dwGuildIndex2 << g_GuildNodeManager.GetGuildBonus( dwGuildIndex2 );
	pServerNode->SendMessage( kPacket );
}

void CampManager::ChangeCampEntryCount( SP2Packet &rkPacket )
{
	bool bEnterCamp;
	int iCampType;
	rkPacket >> iCampType >> bEnterCamp;

	CampData &rkCampData = GetCampData( (CampType)iCampType );
	if( bEnterCamp )
		rkCampData.m_iCampEntryUserCount++;
	else
		rkCampData.m_iCampEntryUserCount--;
	if( rkCampData.m_iCampEntryUserCount < 0 )
		rkCampData.m_iCampEntryUserCount = 0;
	UpdateCampStringHelp();
	LOG.PrintTimeAndLog( 0, "CampManager::ChangeCampEntryCount %d - %d(%d)", iCampType, rkCampData.m_iCampEntryUserCount, (int)bEnterCamp );
}

void CampManager::OnLadderModeResultUpdate( SP2Packet &rkPacket )
{
	{
		int iBlueTotalPoint, iBlueTotalUser;
		int iRedTotalPoint, iRedTotalUser;
		int iUserSize;
		iUserSize = iBlueTotalPoint = iBlueTotalUser = iRedTotalPoint = iRedTotalUser = 0;
		rkPacket >> iUserSize;
		for(int i = 0;i < iUserSize;i++)
		{
			bool bTournament = false;
			DWORD dwTourIndex, dwTeamIndex;
			DWORD dwUserIndex, dwGuildIndex;
			int   iCampType, iTotalLadderPoint;
			rkPacket >> dwUserIndex >> iCampType >> dwGuildIndex >> iTotalLadderPoint >> bTournament;
			if( bTournament )
			{
				rkPacket >> dwTourIndex >> dwTeamIndex;
			}

			if( dwUserIndex != 0 )
			{
				if( iCampType == CAMP_BLUE )
				{
					iBlueTotalPoint += iTotalLadderPoint;
					iBlueTotalUser++;
				}
				else if( iCampType == CAMP_RED )
				{
					iRedTotalPoint += iTotalLadderPoint;
					iRedTotalUser++;
				}

				// �ڽ��� ���� ��忡 ����Ʈ �߰�
				if( dwGuildIndex != 0 )
				{
					g_GuildNodeManager.GuildAddLadderPoint( dwGuildIndex, iTotalLadderPoint );
					LOG.PrintTimeAndLog( 0, "CampManager::OnLadderModeResultUpdate Guild Ladder Point %d - %d - %d", dwUserIndex, dwGuildIndex, iTotalLadderPoint );
				}

				// �ڽ��� ���� ��ʸ�Ʈ ���� ����Ʈ �߰�
				if( bTournament )
				{
					g_TournamentManager.SetTournamentTeamLadderPointAdd( dwTourIndex, dwTeamIndex, iTotalLadderPoint );
				}
			}
		}

		iBlueTotalPoint = (int)( (float)iBlueTotalPoint / (float)max( 1, iBlueTotalUser ) );
		CampData &rkBlueCampData = GetCampData( CAMP_BLUE );
		rkBlueCampData.m_iCampPoint      += iBlueTotalPoint;
		rkBlueCampData.m_iCampTodayPoint += iBlueTotalPoint;

		iRedTotalPoint = (int)( (float)iRedTotalPoint / (float)max( 1, iRedTotalUser ) );
		CampData &rkRedCampData = GetCampData( CAMP_RED );
		rkRedCampData.m_iCampPoint       += iRedTotalPoint;
		rkRedCampData.m_iCampTodayPoint  += iRedTotalPoint;
		LOG.PrintTimeAndLog( 0, "CampManager::OnLadderModeResultUpdate Blue : %d(%d) - Red : %d(%d)", iBlueTotalPoint, iBlueTotalUser, iRedTotalPoint, iRedTotalUser );
	}

	//
	{
		int iBluePoint = GetCampPoint( CAMP_BLUE ) + GetCampBonusPoint( CAMP_BLUE );
		int iRedPoint = GetCampPoint( CAMP_RED ) + GetCampBonusPoint( CAMP_RED );
		int iTotalPoint = max( 1, iBluePoint ) + max( 1, iRedPoint );
		float fBlueInfluence = ( (float)max( 1, iBluePoint ) / (float)iTotalPoint ) + 0.005f;
		if( m_Alarm.IsChange( fBlueInfluence ) )
		{
			LOG.PrintTimeAndLog( 0, "CampManager::OnLadderModeResultUpdate And Alarm Send : %.2f : %d / %d + %d", fBlueInfluence, iBluePoint, iBluePoint, iRedPoint );
		}
	}

	UpdateCampStringHelp();
}

void CampManager::ProcessCamp()
{
	FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 �� �̻� �ɸ���α� ����

	if( m_dwProcessTime != 0 && TIMEGETTIME() - m_dwProcessTime < 60000 ) return;

	m_dwProcessTime = TIMEGETTIME();

	ProcessSeason( m_SeasonState );	

	// 10�п� �ѹ��� ȣ��
	static DWORD sdw10Minute = 0;
	sdw10Minute++;
	if( sdw10Minute > 10 )
	{
		sdw10Minute = 0;
		UpdateCampDataDB();
	}
}

void CampManager::SeasonTodayPointEnd()
{
	// ���ʽ� ���� ����Ʈ ����
	ioINILoader kLoader( "config/sp2_camp.ini" );
	kLoader.SetTitle( "SeasonDate" );
	float fTodayBonus = kLoader.LoadFloat( "TodayPointBonus", 0.0f );

	CampData &rkBlueCampData = GetCampData( CAMP_BLUE );
	CampData &rkRedCampData  = GetCampData( CAMP_RED );		
	if( rkBlueCampData.m_iCampTodayPoint > rkRedCampData.m_iCampTodayPoint )
	{
		float fBonus = (float)( rkBlueCampData.m_iCampPoint + rkBlueCampData.m_iCampBonusPoint ) * fTodayBonus;
		rkBlueCampData.m_iCampBonusPoint += (int)( fBonus + 0.5f ); //�ݿø�
		LOG.PrintTimeAndLog( 0, "ProcessTodayPoint ������ �º� ����� ��!! : %d vs %d - Bonus:%.2f Add:%d + %d", 
								rkBlueCampData.m_iCampTodayPoint, rkRedCampData.m_iCampTodayPoint, fBonus, 
								rkBlueCampData.m_iCampPoint, rkBlueCampData.m_iCampBonusPoint );
	}
	else if( rkRedCampData.m_iCampTodayPoint > rkBlueCampData.m_iCampTodayPoint )
	{
		float fBonus = (float)( rkRedCampData.m_iCampPoint + rkRedCampData.m_iCampBonusPoint ) * fTodayBonus;
		rkRedCampData.m_iCampBonusPoint += (int)( fBonus + 0.5f ); //�ݿø�
		LOG.PrintTimeAndLog( 0, "ProcessTodayPoint ������ �º� ������ ��!! : %d vs %d - Bonus:%.2f Add:%d + %d", 
								rkBlueCampData.m_iCampTodayPoint, rkRedCampData.m_iCampTodayPoint, fBonus,
								rkRedCampData.m_iCampPoint, rkRedCampData.m_iCampBonusPoint );
	}
	else
	{
		LOG.PrintTimeAndLog( 0, "ProcessTodayPoint ������ �º� ���º�!! : %d vs %d", 
								rkBlueCampData.m_iCampTodayPoint, rkRedCampData.m_iCampTodayPoint );
	}

	rkBlueCampData.m_iCampTodayPoint = 0;
	rkRedCampData.m_iCampTodayPoint  = 0;

	// ���� ���� ����� �ο� ��û
	g_DBClient.OnSelectCampSpecialUserCount( CAMP_BLUE );
	g_DBClient.OnSelectCampSpecialUserCount( CAMP_RED );
}

void CampManager::ProcessServerClose()
{
	// ���� ����� �޽����̶�� �ʱ�ȭ ����
	if( IsCampBattleRecess() )
	{
		// Brack�� ��� Sleep�� �־ ���� ó���Ѵ�.
		switch( m_RecessState )
		{				
		case RECESS_FIRST_BACKUP:
			g_DBClient.OnEndCampSeasonFirstBackup();
			Sleep( 10000 );
		case RECESS_RESULT:
			SeasonResult( true );
			Sleep( 10000 );
		case RECESS_LAST_BACKUP:
			g_DBClient.OnEndCampSeasonLastBackup();
			Sleep( 10000 );
		case RECESS_INIT:
			SeasonPrepare( true );
			break;
		}
		m_RecessState  = RECESS_NONE;
		m_dwRecessTime = 0;
	}

	// ���� ���� ������Ʈ
	UpdateCampDataDB();
}