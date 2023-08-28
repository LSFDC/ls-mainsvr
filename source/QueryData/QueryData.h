#pragma once

#include "../util/cBuffer.h"

const int G_MAX_QUERY = 1024 * 31;

class cSerialize;

// ������ Ÿ�԰� ������
enum VariableType 
{ 
	vChar = 100,
	vWChar,
	vTimeStamp,
	vLONG,
	vINT64,
	vSHORT,
	vWrong
};

struct ValueType
{
	VariableType type;			// � Ÿ���� �������ΰ�
	int size;					// �������?
	ValueType()
	{
		type = vWrong;
		size = 0;
	}
};

typedef vector<ValueType> vVALUETYPE;

// �������
struct QueryHeader
{
	QueryHeader()
	{
		Clear();	
	}
	void Clear()
	{
		nMsgType			= -1;
		nQueryType			= -1;
		nFieldLength		= 0;
		nResultLength		= 0;
		nReturnLength		= -1;
		nValueTypeCnt		= -1;
		nQueryBufferSize	= -1;
		nIndex				= 0;
		nResultType			= 0;
		nQueryID			= 0;
		nDatabaseID			= 0;
	}

	int nMsgType;				// �޼��� Ÿ��,	DB�������� ����ϱ⺸�� ���߿� ���Ӽ����� ����� �޾Ƽ� ó���Ҷ� �ʿ�
	int nQueryType;				// ������ Ÿ�� (insert = 0, delete = 1, select = 2, update = 3)
	int nFieldLength;			// ���� �κ� ������
	int nResultLength;			// ��� �κ� ������
	int nReturnLength;			// �ٽ� �ǵ��� �޾ƾ� �ϴ� �������� ������
	int nValueTypeCnt;			// ������� ����
	int nSetValue;				// �̻��
	int nQueryBufferSize;       // ������ ������ (���� ������)
	unsigned int nIndex;        // ���� ������ ���� �ε���.
	int nResultType;			// ��� ������ �ൿ.
	int nQueryID;				// �����ϰ��� �ϴ� ������ȣ
	int nDatabaseID;			// ��񱸺�
};

class CQueryData 
{
public:
	CQueryData();
	virtual ~CQueryData();

public:
	void Clear();
	void Copy( CQueryData &queryData );

	BOOL Deserialize(const char *buffer);

public: 
	int GetMsgType()			{ return m_Header.nMsgType; }
	int GetQueryType()			{ return m_Header.nQueryType; }
	int GetFieldSize()			{ return m_Header.nFieldLength; }
	int GetResultSize()			{ return m_Header.nResultLength; }
	int GetValueCnt()			{ return m_Header.nValueTypeCnt; }
	int GetReturnSize()			{ return m_Header.nReturnLength; }
	int GetBufferSize()			{ return m_Header.nQueryBufferSize; }
	unsigned int GetIndex()		{ return m_Header.nIndex; }
	int GetResultType()			{ return m_Header.nResultType; }
	int GetQueryID()			{ return m_Header.nQueryID; }
	int GetDatabaseID()			{ return m_Header.nDatabaseID; }

	QueryHeader *GetHeader()	{ return &m_Header; }
	char *GetBuffer()			{ return m_Buffer->GetString(); }
	
	void GetFields(cSerialize& fieldTypes);
	void GetResults(vVALUETYPE& valueTypes);
	void GetReturns(char* pBuffer, int &nSize);

public:
	void SetData(
		unsigned int nIndex,
		int nResultType,
		int nMsgType, 
		int nQueryType,
		int nQueryID, 
		cSerialize& fieldTypes,
		vVALUETYPE& valueTypes);
	void SetReturnData( const void *pData, int nSize );

	void ResizeReturnData( uint32 length )	{ m_Return->Resize( length ); }

protected:
	QueryHeader	m_Header;

	cBuffer *m_Buffer;
	cBuffer *m_Return;
};
