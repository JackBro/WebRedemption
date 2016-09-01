#pragma once

#ifdef _M_AMD64
#ifndef _WIN64
#define _WIN64
#endif
#endif

namespace HookControl {

	typedef bool(*__pfnIATHookNotifyRoutine)(IN PIMAGE_THUNK_DATA pCurrentFuncAddrThunk, IN LPCSTR pszCurrentFuncName, LPCVOID pConnext);

	//
	// �޸�IAT��ĺ�����ͨ������ģ��Ļ���ַhModule�ҵ�����������Ŀ¼���ȡIAT����ָ�룬��ѯĿ�꺯����ַ���ڵ�λ�ã�Ȼ���޸�Ϊ���Ӻ����ĵ�ַ��
	//
	bool IATHook(IN HMODULE hHookModule, IN LPCTSTR pszTargetFileName, IN LPCVOID pTargetFuncAddr, IN LPCVOID pReplaceFuncAddr);

	//
	// �޸�IAT��ĺ�����ͨ������ģ��Ļ���ַhModule�ҵ�����������Ŀ¼���ȡIAT����ָ�룬��ѯĿ�꺯����ַ���ڵ�λ�ã�Ȼ���޸�Ϊ���Ӻ����ĵ�ַ��
	//
	bool IATHook(IN HMODULE hHookModule, IN LPCTSTR pszTargetFileName, IN LPCSTR pTargetFuncName, IN LPCVOID pReplaceFuncAddr);

	int SetIATHookNotifyRoutine(IN HMODULE hHookModule, IN LPCTSTR pszTargetDllName, __pfnIATHookNotifyRoutine pfnIATHookNotifyRoutine, IN LPCVOID pConnext = NULL);

	bool IATHooks(IN HMODULE hHookModule, IN LPCVOID pTargetFuncAddr, IN LPCVOID pReplaceFuncAddr);
}