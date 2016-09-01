#pragma once

namespace HookControl {
	//
	//Define
	//
	BOOL WriteReadOnlyMemory(LPBYTE lpDest, LPBYTE lpSource, ULONG Length);

	BOOL GetPatchSize(IN	void *Proc, IN	DWORD dwNeedSize, OUT LPDWORD lpPatchSize);

	bool InlineHook(IN	void *OrgProc,		/* ��ҪHook�ĺ�����ַ */IN	void *NewProc,		/* ���汻Hook�����ĵ�ַ */OUT	void **RealProc		/* ����ԭʼ��������ڵ�ַ */);

	void UnInlineHook(void *OrgProc,  /* ��Ҫ�ָ�Hook�ĺ�����ַ */void *RealProc  /* ԭʼ��������ڵ�ַ */);

	void DelHookGuard(void * lpDest);
	void AddHookGuard(void * lpDest, LPBYTE lpSource, ULONG Length);
	DWORD WINAPI ThreadForHookGuard(PVOID);


	typedef struct _HOOKCONTROL_INFO
	{
		void * CallAddress;
		NTSTATUS RetValue;
	}HOOKCONTROL_INFO, *PHOOKCONTROL_INFO;

#ifndef _AMD64_
#define GetRetAddress(retaddr)				__asm mov eax, [ebp + 4] __asm sub eax, 5 __asm mov retaddr, eax
#else
#define GetRetAddress(retaddr)
#endif


#define HciSetRetAddr(hdciInfo)			GetRetAddress(hdciInfo.CallAddress)
#define HciSetRetValue(hdciInfo,retvalue)			hdciInfo.RetValue = retvalue
#define HcSetProcAddr(ProcName,ProcAddr)									*((void **)&ProcName) = ProcAddr

	inline HINSTANCE GetReturnModule()
	{
		PVOID pRetAddr = NULL;
		MEMORY_BASIC_INFORMATION miMemoryInfo = { 0 };

#ifndef _AMD64_
		GetRetAddress(pRetAddr);
#endif

		VirtualQuery(pRetAddr, &miMemoryInfo, sizeof(MEMORY_BASIC_INFORMATION));

		return (HMODULE)miMemoryInfo.AllocationBase;
	}
}