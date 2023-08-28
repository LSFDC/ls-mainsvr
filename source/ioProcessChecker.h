
#ifndef _ioProcessChecker_h_
#define _ioProcessChecker_h_

class FunctionTimeChecker
{
protected:
	LARGE_INTEGER m_freq;
	UINT64		  m_start;
	UINT64		  m_end;

	bool		  m_bFrequency;
	LONG		  m_iFileLine;	
	LPSTR         m_pFileName;
	double        m_fCheckMicSec;
	DWORD         m_dwPacketID;

public:
	FunctionTimeChecker( LPSTR pFileName, LONG iFileLine, double fCheckMicSec, DWORD dwPacketID );
	virtual ~FunctionTimeChecker();
};
#define FUNCTION_TIME_CHECKER( f, d )        FunctionTimeChecker ftc( __FILE__, __LINE__, f, d )
//////////////////////////////////////////////////////////////////////////
class ioProcessChecker
{
private:
	static ioProcessChecker *sg_Instance;

protected:
	DWORD m_dwCurTime;
	DWORD m_dwLogTime;										// ���ʿ� 1�� �α׸� ������ΰ�?

	// ������ ���� Ƚ��
	DWORD m_dwMainLoop;										// LogTime���� ���� ���� ȣ�� Ƚ��
	DWORD m_dwClientAcceptLoop;                             // LogTime���� ClientAccept ���� ȣ�� Ƚ��
	DWORD m_dwServerAcceptLoop;                             // LogTime���� ServerAccept ���� ȣ�� Ƚ��

	// ��Ŷ ����
	__int64 m_iUserSend;                                     // ������ �������� ���� ��û�� ��Ŷ�� ������
	__int64 m_iUserSendComplete;                             // ������ �������� ���� ó���� ��Ŷ�� ������
	__int64 m_iUserRecv;                                     // �������� ���� ��Ŷ ������

	__int64 m_iServerSend;                                   // ������ �������� ���� ��û�� ��Ŷ�� ������
	__int64 m_iServerSendComplete;                           // ������ �������� ���� ó���� ��Ŷ�� ������
	__int64 m_iServerRecv;                                   // �������� ���� ��Ŷ ������

	__int64 m_iLogDBServerSend;                              // ������ �α�DB�������� ���� ��û�� ��Ŷ�� ������
	__int64 m_iLogDBServerSendComplete;                      // ������ �α�DB�������� ���� ó���� ��Ŷ�� ������
	__int64 m_iLogDBServerRecv;                              // �α�DB�������� ���� ��Ŷ ������

	__int64 m_iDBServerSend;                                 // ������ DB�������� ���� ��û�� ��Ŷ�� ������
	__int64 m_iDBServerSendComplete;                         // ������ DB�������� ���� ó���� ��Ŷ�� ������
	__int64 m_iDBServerRecv;                                 // DB�������� ���� ��Ŷ ������

	int     m_iMainProcessMaxPacket;                         // ���� ���μ������� �� �������� ó���ϴ� ��Ŷ�� �� ���� ���� ��Ŷ

	// ������ ���� �ð�
protected:
	struct PerformanceTime
	{
		LARGE_INTEGER m_freq;
		UINT64		  m_start;
		UINT64		  m_end;
		bool		  m_bFrequency;		
	};
	PerformanceTime   m_MainThreadTime;
	double            m_fMainTreadMaxTime;

	PerformanceTime   m_ServerAThreadTime;
	double            m_fServerATreadMaxTime;

	PerformanceTime   m_ClientAThreadTime;
	double            m_fClientATreadMaxTime;

public:
	static ioProcessChecker &GetInstance();
	static void ReleaseInstance();

public:
	void LoadINI();
	void Initialize();
	void Process();
	void WriteLOG();

public:
	void MainThreadCheckTimeStart();
	void MainThreadCheckTimeEnd();
	void ServerAThreadCheckTimeStart();
	void ServerAThreadCheckTimeEnd();
	void ClientAThreadCheckTimeStart();
	void ClientAThreadCheckTimeEnd();

public:
	void CallMainThread(){ m_dwMainLoop++; }
	void CallClientAccept(){ m_dwClientAcceptLoop++; }
	void CallServerAccept(){ m_dwServerAcceptLoop++; }

public:
	void ProcessIOCP( int iConnectType, DWORD dwFlag, DWORD dwByteTransfer );
	void UserSendMessage( int iSize );
	void UserSendComplete( int iSize );
	void UserRecvMessage( int iSize );
	void ServerSendMessage( int iSize );
	void ServerSendComplete( int iSize );
	void ServerRecvMessage( int iSize );
	void LogDBServerSendMessage( int iSize );
	void LogDBServerSendComplete( int iSize );
	void LogDBServerRecvMessage( int iSize );
	void DBServerSendMessage( int iSize );
	void DBServerSendComplete( int iSize );
	void DBServerRecvMessage( int iSize );
	void MainProcessMaxPacket( int iPacketCnt );

private:
	ioProcessChecker();
	virtual ~ioProcessChecker();
};
#define g_ProcessChecker ioProcessChecker::GetInstance()
#endif