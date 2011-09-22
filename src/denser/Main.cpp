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
	_tprintf(_T("Usage: denser [/useContext] [/config <configfile>] | ([/admin <admin_url> (anonymous|<authorizationKey>)] [<jsfile>]*)\nNote: v8.dll must be in the same directory as denser.exe."));
	_tprintf(_T("denser.exe --help for V8 options."));
	return -1;
}

HRESULT CreateConfig(int argc, _TCHAR* argv[], CAtlString *config, bool* useContextIsolation)
{
	if (argc < 2)
	{
		return Help();
	}	

	*useContextIsolation = false;

	int parameter = 1;

	if (true == (*useContextIsolation = (0 == _tcsicmp(L"/useContext", argv[parameter]))))
	{
		parameter++;
	}

	if (argc > parameter && 0 == _tcsicmp(L"/config", argv[parameter]))
	{
		if (argc != 4)
		{
			return Help();
		}

		parameter++;

		LPWSTR tmp;
		ErrorIf(NULL == (tmp = Denser::GetFileContentAsUnicode(argv[parameter])));
		config->Append(tmp);
		free(tmp);
	}
	else 
	{
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
	char** argv1 = NULL;
	char** argv1cpy = NULL;
	_TCHAR** argv2 = NULL;
	bool useContextIsolation;

	ErrorIf(NULL == (argv1 = new char*[argc + 1]));
	RtlZeroMemory(argv1, sizeof(char*) * (argc + 1));
	for (int i = 0; i < argc; i++)
	{
		ErrorIf(NULL == (argv1[i] = (char*)::AllocUnicodeToAscii(argv[i], wcslen(argv[i]), NULL)));
	}

	ErrorIf(NULL == (argv1cpy = new char*[argc + 1]));
	memcpy(argv1cpy, argv1, sizeof(char*) * (argc + 1));

	v8::V8::SetFlagsFromCommandLine(&argc, argv1, true);

	ErrorIf(NULL == (argv2 = new _TCHAR*[argc + 1]));
	RtlZeroMemory(argv2, sizeof(_TCHAR*) * (argc + 1));
	for (int i = 0; i < argc; i++)
	{
		ErrorIf(NULL == (argv2[i] = (_TCHAR*)::AllocAsciiToUnicode(argv1[i], strlen(argv1[i]), NULL)));
	}
	
	ErrorIf(S_OK != CreateConfig(argc, argv2, &config, &useContextIsolation));
	manager = new DenserManager(useContextIsolation);
	ErrorIf(0 == SetConsoleCtrlHandler(::ControlHandler, TRUE));
	HRESULT result = manager->Run(config);
	delete manager;

	for (char** arg = argv1cpy; *arg; arg++)
		delete *arg;
	delete argv1cpy;
	delete argv1;
	for (_TCHAR** arg = argv2; *arg; arg++)
		delete *arg;
	delete argv2;

	_CrtDumpMemoryLeaks();

	return result;
Error:
	if (argv1cpy)
	{
		for (char** arg = argv1cpy; *arg; arg++)
			delete *arg;
		delete argv1cpy;
		delete argv1;
	}
	else if (argv1)
	{
		for (char** arg = argv1; *arg; arg++)
			delete *arg;
		delete argv1;
	}

	if (argv2)
	{
		for (_TCHAR** arg = argv2; *arg; arg++)
			delete *arg;
		delete argv2;
	}

	if (manager)
	{
		delete manager;
	}
	return E_FAIL;
}