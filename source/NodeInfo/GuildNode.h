#ifndef _GuildNode_h_
#define _GuildNode_h_
#include "../BoostPooler.h"

class SP2Packet;
typedef struct tagGuildRecord
{
	int m_iWin;
	int m_iLose;
	int m_iKill;
	int m_iDeath;

	bool m_bChange;
	tagGuildRecord()
	{
		Init();
	}

	void Init()
	{
		m_iWin = m_iLose = 0;
		m_iKill = m_iDeath = 0;
		m_bChange = false;
	}
}GUILDRECORD;

class GuildNode : public BoostPooler<GuildNode>
{
protected:
	// �⺻ ����
	DWORD m_dwGuildIndex;            //��� ���� �ε���
	DWORD m_dwGuildMark;             //��� ��ũ ������Ʈ �ε���	
	DWORD m_dwGuildRegDate;          //��� ���� ����
	DWORD m_dwGuildMaxEntry;         //�ִ� ����
	DWORD m_dwGuildJoinUser;		 //���� ����ο�
	
	ioHashString m_szGuildName;      //��� �̸�.
	ioHashString m_szGuildTitle;     //��� �Ұ�.

	// ���� �������� ����Ǹ� DB�� ������Ʈ �Ǿ���� ��.
	DWORD m_dwGuildRank;             //��� ��ŷ
	DWORD m_dwGuildPoint;            //��� ����Ʈ GP : ��ŷ������ ȹ���� ����Ʈ (������ �ɼ�����)
	DWORD m_dwCurGuildPoint;         //������ ȹ���� ��� ����Ʈ(������ �ɼ�����)
	DWORD m_dwGuildLevel;            //��� ����

	// ���� ���� 
	GUILDRECORD m_Record;            //��� ����

	// ��� ���� ���� ���
	bool m_bSaveReg;
protected:
	void InitData();

public:
	void CreateGuild( SP2Packet &rkPacket, DWORD dwDefaultLevel );
	BOOL CreateGuild( CQueryResultData *pQueryData );

protected:
	DWORD GetLimitMaxEntry();

public:
	inline const DWORD GetGuildIndex() const { return m_dwGuildIndex; }
	inline DWORD GetGuildMark() { return m_dwGuildMark; }
	inline DWORD GetGuildRegDate() { return m_dwGuildRegDate; }
	inline DWORD GetGuildMaxEntry() { return m_dwGuildMaxEntry; }
	inline DWORD GetGuildJoinUser() { return m_dwGuildJoinUser; }

	inline ioHashString GetGuildName() { return m_szGuildName; }
	inline ioHashString GetGuildTitle() { return m_szGuildTitle; }

	inline DWORD GetGuildRank() { return m_dwGuildRank; }
	inline const DWORD GetGuildPoint() const { return m_dwGuildPoint; }
	inline const DWORD GetCurGuildPoint() const { return m_dwCurGuildPoint; }
	inline const DWORD GetGuildLevel() const { return m_dwGuildLevel; }
	inline GUILDRECORD GetGuildRecord() { return m_Record; }
	
	float GetGuildBonus();

public:
	inline void SetGuildRank( DWORD dwSetRank ) { m_dwGuildRank = dwSetRank; }
	inline void SetGuildTitle( const ioHashString &szGuildTitle ) { m_szGuildTitle = szGuildTitle; }
	inline void SetGuildMark( DWORD dwGuildMark ) { m_dwGuildMark = dwGuildMark; }
	void SetGuildMaxEntry( DWORD dwMaxEntry );
	void SetGuildJoinUser( DWORD dwJoinUserCount );
	void SetGuildLevel( DWORD dwGuildLevel, DWORD dwGuildMaxEntry );

public:
	void ChangeGuildName( const ioHashString &rkNewName );
	
public:
	inline void InitGuildCurPoint(){ m_dwCurGuildPoint = 0; }


public:
	void AddGuildRecord( int iWin, int iLose, int iKill, int iDeath );
	void AddGuildPoint( int iCurGP );

public:
	void Save();
	void SetSaveReg(){ m_bSaveReg = true; }
	bool IsSaveReg(){ return m_bSaveReg; }

public:
	GuildNode();
	virtual ~GuildNode();
};

#endif