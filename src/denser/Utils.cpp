#include "stdafx.h"

PCWSTR AllocAsciiToUnicode(PCSTR str, int length, int *size)
{
	PWSTR result = NULL;
	int s;
	ErrorIf(0 == (s = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, length, NULL, 0)));
	result = new wchar_t[s + 1];
	ErrorIf(s != MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, length, result, s));
	result[s] = L'\0';
	if (size)
	{
		*size = s;
	}

	return result;
Error:
	if (result)
	{
		delete[] result;
	}
	return NULL;
}

PCSTR AllocUnicodeToAscii(PCWSTR str, int length, USHORT *size)
{
	PSTR result = NULL;
	int s;
	ErrorIf(0 == (s = WideCharToMultiByte(CP_ACP, 0, str, length, NULL, 0, NULL, NULL)));
	result = new char[s + 1];
	ErrorIf(s != WideCharToMultiByte(CP_ACP, 0, str, length, result, s, NULL, NULL));
	result[s] = '\0';
	if (size)
	{
		*size = s > 0 ? s - 1 : 0;
	}

	return result;
Error:
	if (result)
	{
		delete[] result;
	}
	return NULL;
}

HRESULT SetBOOLProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, bool value)
{
	ErrorIf(!var->Set(v8::String::New((uint16_t*)propertyName), v8::Boolean::New(value)));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT SetStringProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, PCWSTR value, int valueLength)
{
	if (value)
	{
		ErrorIf(!var->Set(v8::String::New((uint16_t*)propertyName), v8::String::New((uint16_t*)value, valueLength == -1 ? wcslen(value) : valueLength)));
	}

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT SetIntProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, int value)
{
	ErrorIf(!var->Set(v8::String::New((uint16_t*)propertyName), v8::Int32::New(value)));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT SetVarProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, v8::Handle<v8::Object> value)
{
	ErrorIf(!var->Set(v8::String::New((uint16_t*)propertyName), value));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT SetULONG64Property(v8::Handle<v8::Object> var, PCWSTR propertyName, ULONG64 value)
{
	ErrorIf(!var->Set(v8::String::New((uint16_t*)propertyName), v8::Number::New((double)value)));

	return S_OK;
Error:
	return E_FAIL;
}

HRESULT SetGUIDProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, GUID value)
{
	WCHAR guidStr[39];
	int length;

	ErrorIf(0 == (length = StringFromGUID2(value, guidStr, 39)));
	guidStr[length - 1] = L'\0';
	return ::SetStringProperty(var, propertyName, guidStr + 1, length - 3);

Error:
	return E_FAIL;
}