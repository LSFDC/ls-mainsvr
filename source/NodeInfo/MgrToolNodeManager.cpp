#include "../stdafx.h"

#include "../ioProcessChecker.h"
#include "../Network/GameServer.h"
#include "MgrToolNodeManager.h"

#include "../QueryData/QueryResultData.h"

extern CLog OperatorLOG;

MgrToolNodeManager *MgrToolNodeManager::sg_Instance = NULL;

MgrToolNodeManager::MgrToolNodeManager() : m_dwCurrentTime(0)
{
}

MgrToolNodeManager::~MgrToolNodeManager()
{
	m_vMgrToolNode.clear();
}

MgrToolNodeManager &MgrToolNodeManager::GetInstance()
{
	if( !sg_Instance )
		sg_Instance = new MgrToolNodeManager;

	return *sg_Instance;
}

void MgrToolNodeManager::ReleaseInstance()
{
	SAFEDELETE( sg_Instance );
}

void MgrToolNodeManager::InitMemoryPool()
{
	MgrToolNode::LoadHackCheckValue();

	ioINILoader kLoader( "ls_config_main.ini" );
	kLoader.SetTitle( "Manager Session Buffer" );
	int iSendBufferSize = kLoader.LoadInt( "SendBufferSize", 16384 );
	int iRecvBufferSize = kLoader.LoadInt( "RecvBufferSize", 16384 );
	kLoader.SetTitle( "MemoryPool" );
	int iMaxTool = kLoader.LoadInt( "tool_pool", 10 );

	m_MemNode.CreatePool(0, iMaxTool, FALSE);
	for(int i = 0;i < iMaxTool;i++)
	{
		m_MemNode.Push( new MgrToolNode( INVALID_SOCKET, iSendBufferSize, iRecvBufferSize ) );
	}
}

void MgrToolNodeManager::ReleaseMemoryPool()
{
	vMgrToolNode_iter iter, iEnd;	

	iEnd = m_vMgrToolNode.end();
	for(iter = m_vMgrToolNode.begin();iter != iEnd;++iter)
	{
		MgrToolNode *pNode = *iter;
		pNode->OnDestroy();
		m_MemNode.Push( pNode );
	}	
	m_vMgrToolNode.clear();
	m_MemNode.DestroyPool();
}

MgrToolNode *MgrToolNodeManager::CreateMgrToolNode( SOCKET s )
{
	MgrToolNode *newNode = m_MemNode.Remove();
	if( !newNode )
	{
		LOG.PrintTimeAndLog(0,"MgrToolNodeManager::CreateMgrToolNode MemPool Zero!");
		return NULL;
	}

	newNode->SetSocket(s);
	newNode->OnCreate();
	return newNode;
}

void MgrToolNodeManager::AddNode( MgrToolNode *pNewNode )
{
	m_vMgrToolNode.push_back( pNewNode );
}

void MgrToolNodeManager::RemoveNode( MgrToolNode *pNode )
{
	vMgrToolNode_iter iter = std::find( m_vMgrToolNode.begin(), m_vMgrToolNode.end(), pNode );
	if( iter != m_vMgrToolNode.end() )
	{
		MgrToolNode *pNode = *iter;
		pNode->OnDestroy();
		m_vMgrToolNode.erase( iter );
		m_MemNode.Push( pNode );
	}	
}

void MgrToolNodeManager::ProcessMgrToolNode()
{
	FUNCTION_TIME_CHECKER( 100000.0f, 0 );          // 0.1 �� �̻� �ɸ���α� ����

	if( m_dwCurrentTime == 0 )
		m_dwCurrentTime = TIMEGETTIME();

	if( TIMEGETTIME() - m_dwCurrentTime < 60000 ) return;

	m_dwCurrentTime = TIMEGETTIME();

	vMgrToolNode_iter iter = m_vMgrToolNode.begin();
	while( iter != m_vMgrToolNode.end() )
	{
		MgrToolNode *pNode = *iter++;
		if( pNode->IsGhostSocket() )
		{
			LOG.PrintTimeAndLog( 0, "Manager Tool : 2�а� ������ ���� ���� �����Ŵ" );
			pNode->ExceptionClose( 0 );
		}
	}
}

void MgrToolNodeManager::MgrToolNode_SendBufferFlush()
{
	if( m_vMgrToolNode.empty() == false )
	{
		vector< MgrToolNode* >::iterator	iter	= m_vMgrToolNode.begin();
		vector< MgrToolNode* >::iterator	iterEnd	= m_vMgrToolNode.end();

		for( iter ; iter != iterEnd ; ++iter )
		{
			MgrToolNode* pToolNode = (*iter);

			if( ! pToolNode->IsActive() )
				continue;
			if( pToolNode->GetSocket() == INVALID_SOCKET )
				continue;

			pToolNode->FlushSendBuffer();
		}
	}
}

void MgrToolNodeManager::ApplyUserBlockDB( CQueryResultData *pQueryData )
{
	DWORD dwMgrToolIndex;
	ioHashString szPublicID;
	LONG lSuccess;
	
	if( !pQueryData->GetValue( (DWORD)dwMgrToolIndex ) ) return;
	if( !pQueryData->GetValue( szPublicID, ID_NUM_PLUS_ONE ) ) return ;
	if( !pQueryData->GetValue( lSuccess ) ) return ;

	if( 1 == lSuccess)
	{
		OperatorLOG.PrintTimeAndLog(0, "ApplyUserBlockDB : DBWrite Success - %s", szPublicID.c_str() );
	}
	else
	{
		OperatorLOG.PrintTimeAndLog(0, "ApplyUserBlockDB : DBWrite Failed!! - %s", szPublicID.c_str() );
	}


	MgrToolNode* pToolNode = GetNode( dwMgrToolIndex );
	if( NULL == pToolNode )
	{
		LOG.PrintTimeAndLog( 0, "ApplyUserBlockDB : GetNode Failed!! : %d", dwMgrToolIndex );
	}
	else
	{
		pToolNode->ApplyUserBlockDB( szPublicID, lSuccess );
	}
}

void MgrToolNodeManager::SendMessageAllNode( SP2Packet &rkPacket )
{
	vMgrToolNode_iter iter = m_vMgrToolNode.begin();
	while( iter != m_vMgrToolNode.end() )
	{
		MgrToolNode *pNode = *iter++;
        pNode->SendMessage( rkPacket );
	}
}

void MgrToolNodeManager::SendMessageGUIDNode( ioHashString& szGUID, SP2Packet& rkPacket )
{
	MgrToolNode* pNode = GetNode( szGUID );
	if( pNode == NULL )
		return;

	pNode->SendMessage( rkPacket );
}

MgrToolNode* MgrToolNodeManager::GetNode( const ioHashString &szGUID )
{
	vMgrToolNode_iter iter, iEnd;
	iEnd = m_vMgrToolNode.end();
	for( iter=m_vMgrToolNode.begin() ; iter!=iEnd ; ++iter )
	{
		MgrToolNode *pNode = *iter;

		if( pNode->GetGUID() == szGUID )
			return pNode;
	}
	return NULL;
}

MgrToolNode* MgrToolNodeManager::GetNode( const DWORD &dwMgrToolIndex )
{
	vMgrToolNode_iter iter, iEnd;
	iEnd = m_vMgrToolNode.end();
	for( iter=m_vMgrToolNode.begin() ; iter!=iEnd ; ++iter )
	{
		MgrToolNode *pNode = *iter;

		if( pNode->GetIndex() == dwMgrToolIndex )
			return pNode;
	}
	return NULL;
}
