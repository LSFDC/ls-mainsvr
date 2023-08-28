#pragma once

#include "Service.h"

class ServiceLS : public Service
{
public:
	ServiceLS(int argc, TCHAR **argv);
	virtual ~ServiceLS();

public:
	virtual BOOL ExecuteProcess();									// ���� ����ÿ� ȣ��Ǵ� �Լ�(����� ����)
	virtual BOOL ServiceStart();									// ���� ����(START)�ÿ� ȣ���
	virtual void ServiceStop();										// ���� ����(STOP)�ÿ� ȣ���

	virtual void Debug(const TCHAR *message);						// ������ �޼��� ���

protected:
	void ConfigureSystem();
	void ConfigureTime();

	const TCHAR* GetLogFile()		{ return m_logFile.c_str(); }	// ���� �αװ� ���� ����
	const TCHAR* GetCurrentTime()	{ return m_time.c_str(); }		// ���� �ð�

protected:
	tstring m_iniFile;
	tstring m_logFile;
	tstring m_time;
};
