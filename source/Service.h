#pragma once

#include <vector>
#include <string>

typedef std::basic_string<TCHAR> tstring;

class Service  
{
public:
	Service(int argc, TCHAR **argv);
	virtual ~Service();

public:
	void ServiceMainProc();											// ���� ���� ���� �Լ�

	static Service* _serviceMainClass;
	static void WINAPI _ServiceMain(int argc, TCHAR **argv);			// ���� ���� �Լ�(ServiceMain()�Լ��� ȣ��)
	static void WINAPI _ServiceHandler(DWORD fdwControl);			// ���� ���� ���� �Լ�(ServiceHandler()�Լ��� ȣ��)

	void ServiceMain(int argc, TCHAR **argv);						// ���� ������ ������
	void ServiceHandler(DWORD fdwControl);							// ���� ������ ������

public:
	virtual BOOL ExecuteProcess() = 0;								// ���� ����ÿ� ȣ��Ǵ� �Լ�
	virtual BOOL ServiceStart() = 0;								// ���� ����(START)�ÿ� ȣ���
	virtual void ServiceStop() = 0;									// ���� ����(STOP)�ÿ� ȣ���

	virtual void Debug(const TCHAR *message) = 0;					// ������ �޼��� ���

protected:
	BOOL _ExecuteProcess();											// ������ ����� ȣ���

	// Service Control
	BOOL Install(const TCHAR* serviceName, const TCHAR* displayName);	// ���� ���
	BOOL Uninstall(const TCHAR* serviceName);						// ���� ����
	BOOL RunService(const TCHAR* serviceName);						// ���� ����(START)
	BOOL KillService(const TCHAR* serviceName);						// ���� ����(STOP)

protected:
	std::vector<tstring> m_arguments;
	tstring m_serviceName;
	tstring m_displayName;

	SERVICE_STATUS_HANDLE   m_hServiceStatusHandle; 
	SERVICE_STATUS          m_serviceStatus; 
};
