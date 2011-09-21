#pragma once

#include "stdafx.h"

PCWSTR AllocAsciiToUnicode(PCSTR str, int length, int *size);
PCSTR AllocUnicodeToAscii(PCWSTR str, int length, USHORT *size);
HRESULT SetBOOLProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, bool value);
HRESULT SetStringProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, PCWSTR value, int valueLength);
HRESULT SetIntProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, int value);
HRESULT SetVarProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, v8::Handle<v8::Object>);
HRESULT SetULONG64Property(v8::Handle<v8::Object> var, PCWSTR propertyName, ULONG64 value);
HRESULT SetGUIDProperty(v8::Handle<v8::Object> var, PCWSTR propertyName, GUID value);
