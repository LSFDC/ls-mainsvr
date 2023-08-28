

#ifndef _ioINIParser_h_
#define _ioINIParser_h_


class ioTextStream;

class ioINIParser
{
private:
	typedef std::map< ioHashString, std::string > KeyList;
	typedef std::map< ioHashString, KeyList* > TitleList;

	TitleList m_TitleList;

	mutable ioHashString m_PreTitle;	// ������ ����� Ÿ��Ʋ ĳ�ÿ뵵
	mutable KeyList *m_pPreList;

	mutable ioHashString m_szTitleBuf;
	mutable ioHashString m_szKeyNameBuf;

public:
	bool ParsingFile( const char *szFileName );
	void ParseINI( ioTextStream &rkStream );
	void Clear();

private:
	std::string ParseTitle( const std::string &szTitle, ioTextStream &rkStream );
	void ParseKey( const std::string &line, KeyList *pKeyList );

public:
	int GetNumTotalTitle() const;
	int GetNumTotalKey( int iTitle ) const;

public:	// ������ ""
	const char* GetTitle( int iIdx ) const;
	const char* GetKey( int iTitle, int iKey ) const;

public:	// ������ NULL
	const char* GetValue( int iTitle, int iKey ) const;
	const char* GetValue( const char *szTitle, const char *szKeyName ) const;

public:
	ioINIParser();
	~ioINIParser();
};

#endif