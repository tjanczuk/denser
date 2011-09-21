#include "stdafx.h"

DenserManager* manager = NULL;

BOOL WINAPI ControlHandler(DWORD type) 
{
	_tprintf(_T("Shutting down..."));
	manager->InitializeShutdown();
	return TRUE;
}

HRESULT Help()
{
	_tprintf(_T("Usage: denser [/config <configfile>] | ([/admin <admin_url> (anonymous|<authorizationKey>)] [<jsfile>]*)\nNote: jscript9.dll must be in the same directory as denser.exe."));
	return -1;
}

HRESULT CreateConfig(int argc, _TCHAR* argv[], CAtlString *config)
{
	if (argc < 2)
	{
		return Help();
	}	

	if (0 == _tcsicmp(L"/config", argv[1]))
	{
		if (argc != 3)
		{
			return Help();
		}

		LPWSTR tmp;
		ErrorIf(NULL == (tmp = Denser::GetFileContentAsUnicode(argv[2])));
		config->Append(tmp);
		free(tmp);
	}
	else 
	{
		int parameter = 1;
		config->Append(L"{");
		if (0 == _tcsicmp(L"/admin", argv[parameter]))
		{
			if (argc < 4)
			{
				return Help();
			}
			// TODO, tjanczuk, add JSON encoding of admin URL
			config->AppendFormat(L"\"admin\":{\"url\":\"%s\"", argv[parameter + 1]); 
			if (0 != _tcsicmp(L"anonymous", argv[parameter + 2]))
			{
				config->AppendFormat(L",\"authorizationKey\":\"%s\"},", argv[parameter + 2]);
			}
			else 
			{
				config->Append(L"},");
			}
			parameter += 3;
		}
		config->Append(L"\"programs\":[");
		while (parameter < argc)
		{
			// TODO, tjanczuk, add JSON encoding of file names
			config->AppendFormat(L"{\"file\":\"%s\"}%s", argv[parameter], (parameter + 1) < argc ? L"," : L"");
			parameter++;
		}
		config->Append(L"]}");
	}

	return S_OK;
Error:
	return E_FAIL;
}

int _tmain(int argc, _TCHAR* argv[])
{	
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF ); // WARINING - this does not catch SysAllocStringLen allocation leaks

	CAtlString config;
	ErrorIf(S_OK != CreateConfig(argc, argv, &config));
	manager = new DenserManager();
	ErrorIf(0 == SetConsoleCtrlHandler(::ControlHandler, TRUE));
	HRESULT result = manager->Run(config);
	delete manager;

	_CrtDumpMemoryLeaks();

	return result;
Error:
	if (manager)
	{
		delete manager;
	}
	return E_FAIL;
}