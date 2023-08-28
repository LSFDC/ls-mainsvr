// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#include <atltime.h>

#include "winsock2.h"
#include <stdio.h>
#include <tchar.h>
#include <MMSystem.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <set>
#include <list>
#include <deque>
#include <algorithm>
#include <functional>
#include <utility>
#include <limits>
#include <iostream>
#include "common.h"
#include "../include/Log.h"
#include "../iocpSocketDLL/iocpSocketDLL.h"
#include "../FrameTimerDLL/FrameTimerDLL.h"
#include "Util\ioHashString.h"
#include "Define.h"
#include "Protocol.h"
#include "ioPacketChecker.h"
#include "Util\ioStringConverter.h"
#include "../ioINILoader/ioINILoader.h"
#include "Util\ioEncrypted.h"
#include "Util\Singleton.h"
#include "Network\SP2Packet.h"
#include "Version.h"

using namespace std;

extern LONG WINAPI UnHandledExceptionFilter( struct _EXCEPTION_POINTERS* exceptionInfo );

extern void Trace( const char *format, ... );
extern void Debug( const char *format, ... );
extern void Information( const char *format, ... ) ;
extern void Debug();

extern CLog LOG;

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
