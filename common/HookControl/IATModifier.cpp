#include "IATModifier.h"
#include <string>
#include <Windows.h>
#include <tchar.h>

using namespace std;

IATModifier::IATModifier(const CProcess& process)
	: m_cProcess(process), m_pImportDescrTblAddr(NULL), m_uImportDescrTblSize(0)
{
}

IATModifier::~IATModifier()
{
}

// check if supplied IBA is a valid executable header, so we can locate the import descriptor
bool IATModifier::SetImageBase(uintptr_t address)
{
	bool bIsOK = false;
	IMAGE_DOS_HEADER dosHeader;

	do 
	{
		m_cProcess.ReadMemory((void*)address, &dosHeader, sizeof(IMAGE_DOS_HEADER));

		if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
			break;

		IMAGE_NT_HEADERS ntHeaders;
		ntHeadersAddr_ = address + dosHeader.e_lfanew;

		m_cProcess.ReadMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS));

		if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
			break;

		if (ntHeaders.FileHeader.Characteristics & IMAGE_FILE_DLL)
			break;

		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0)
			break;

		bIsOK = true;
		m_uImportDescrTblSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
		m_pImportDescrTblAddr = (PIMAGE_IMPORT_DESCRIPTOR)(ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + (DWORD)address);
	} while (false);

	return bIsOK;
}

bool IATModifier::WriteIAT(const char * pszDllPath)
{
	bool bIsOK = false;
	char* pNewDescrTbl = NULL;
	// allocate memory for the new descriptor
	IMAGE_NT_HEADERS ntHeaders;

	do
	{
		if (NULL == pszDllPath)
			break;

		if (m_pImportDescrTblAddr == NULL)
			break;

		if (false == m_cProcess.ReadMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS)))
			break;


		// the size of all newly added data, i.e. size without original data in IID
		// we need n additional IMAGE_IMPORT_DESCRIPTOR
		DWORD customDataSize = sizeof(IMAGE_IMPORT_DESCRIPTOR);

		// make sure the string sizes are padded to 32 bit boundary
		customDataSize += padToDword(_tcslen(pszDllPath) + 1);

		// IMAGE_THUNK_DATA, 2 entries each (OriginalFirstThunk+FirstThunk)
		customDataSize += 4 * sizeof(DWORD);

		DWORD origIIDTblSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
		DWORD newDescrTblSize = customDataSize + origIIDTblSize;
		pNewDescrTbl = new char[newDescrTblSize];

		// allocate and build new import descriptor
		DWORD iba = ntHeaders.OptionalHeader.ImageBase;
		void* newDescrTblAddress = allocateMemAboveBase((void*)iba, newDescrTblSize);

		// RVA pointing directly behind the old table
		DWORD newTblRVA = (DWORD)newDescrTblAddress - iba + origIIDTblSize + sizeof(IMAGE_IMPORT_DESCRIPTOR);
		DWORD currentRVA = newTblRVA;

		// step 1: prepend new IID entries for our dlls and fill with the correct RVAs
		PIMAGE_IMPORT_DESCRIPTOR currentIDD = (PIMAGE_IMPORT_DESCRIPTOR)pNewDescrTbl;

			// layout: [<orig_first_thunk><IAT><name>]...[...]
			currentIDD->OriginalFirstThunk = currentIDD->FirstThunk = currentRVA;
			currentIDD->TimeDateStamp = currentIDD->ForwarderChain = 0;
			currentRVA += 4 * sizeof(DWORD);
			currentIDD->Name = currentRVA;
			currentRVA += padToDword(_tcslen(pszDllPath) + 1);

		// step 2: add old IID entries
		// we need to save the original import descriptor; read process memory directly into our new IID table
		for (PIMAGE_IMPORT_DESCRIPTOR descrAddr = m_pImportDescrTblAddr;; ++descrAddr, ++currentIDD)
		{
			if (m_cProcess.ReadMemory(descrAddr, currentIDD, sizeof(IMAGE_IMPORT_DESCRIPTOR)))
				break;

			if (currentIDD->FirstThunk == 0 && currentIDD->OriginalFirstThunk == 0)
				break;
		}

		// step 3: build blocks made of IMAGE_THUNK_DATA, IAT and dll name string
		// let curBlock point after IIDs
		PDWORD curBlock = (PDWORD)(pNewDescrTbl + origIIDTblSize + sizeof(IMAGE_IMPORT_DESCRIPTOR));

			// force the dll to export at least one entry with ordinal 1
			// OriginalFirstThunk
			*curBlock++ = IMAGE_ORDINAL_FLAG | 1;
			*curBlock++ = 0;
			// FirstThunk
			*curBlock++ = IMAGE_ORDINAL_FLAG | 1;
			*curBlock++ = 0;
			memcpy((char*)curBlock, pszDllPath, _tcslen(pszDllPath) + 1);
			curBlock += padToDword(_tcslen(pszDllPath) + 1) >> 2;

		// if IAT is zero, set it to VA of section which holds import directory (needed for delphi programs?!)
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress == 0)
		{
			uintptr_t sectionHdrAddr = ntHeadersAddr_ + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + ntHeaders.FileHeader.SizeOfOptionalHeader;
			IMAGE_SECTION_HEADER ish;
			for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i, sectionHdrAddr += sizeof(IMAGE_SECTION_HEADER))
			{
				if (false == m_cProcess.ReadMemory((LPVOID)sectionHdrAddr, &ish, sizeof(ish)))
					continue;

				if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress >= ish.VirtualAddress &&
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress < ish.VirtualAddress + ish.SizeOfRawData)
				{
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = ish.VirtualAddress;
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = ish.SizeOfRawData;
				}
			}
		}

		// finally write new descriptor, fix RVAs and update IMAGE_NT_HEADERS
		if (false == m_cProcess.WriteMemory(newDescrTblAddress, pNewDescrTbl, newDescrTblSize))
			break;

		DWORD newIIDRVA = (DWORD)newDescrTblAddress - iba;
		ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = newIIDRVA;
		ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = newDescrTblSize;

		// only clear BOUND directory if we need to
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress != -1
			&& ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress != 0)
		{
			ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
			ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
		}

		// handle injection in .NET process
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress != 0)
		{
			uintptr_t comDescriptorAddr = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress + iba;
			IMAGE_COR20_HEADER cor20Header;

			if (false == m_cProcess.ReadMemory((LPVOID)comDescriptorAddr, &cor20Header, sizeof(IMAGE_COR20_HEADER)))
				break;

			if (cor20Header.Flags | COMIMAGE_FLAGS_ILONLY)
			{
				// pure IL executables trigger more restrictive PE header checks
				// so just remove the corresponding flag
				cor20Header.Flags = cor20Header.Flags & ~COMIMAGE_FLAGS_ILONLY;
				DWORD oldProtect = m_cProcess.ProtectMemory((LPVOID)comDescriptorAddr, sizeof(IMAGE_COR20_HEADER), PAGE_EXECUTE_READWRITE);

				if (false == m_cProcess.WriteMemory((LPVOID)comDescriptorAddr, &cor20Header, sizeof(IMAGE_COR20_HEADER)))
					break;

				if (false == m_cProcess.ProtectMemory((LPVOID)comDescriptorAddr, sizeof(IMAGE_COR20_HEADER), oldProtect))
					break;
			}
		}

		// finally write new NT headers and reset page protection afterwards
		DWORD oldProtect = m_cProcess.ProtectMemory((void*)ntHeadersAddr_, sizeof(IMAGE_NT_HEADERS), PAGE_EXECUTE_READWRITE);
		
		if (false == m_cProcess.WriteMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS)))
			break;

		if (false == m_cProcess.ProtectMemory((void*)ntHeadersAddr_, sizeof(IMAGE_NT_HEADERS), oldProtect))
			break;

	} while (false);

	if (pNewDescrTbl)
		delete[] pNewDescrTbl;

	return bIsOK;
}

// write one or more new import descriptors by allocating a new import descriptor table
bool IATModifier::WriteIAT(const vector<string>& dlls)
{
	// allocate memory for the new descriptor
	IMAGE_NT_HEADERS ntHeaders;

	do 
	{
		if (dlls.empty())
			break;

		if (m_pImportDescrTblAddr == NULL)
			break;

		if (false == m_cProcess.ReadMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS)))
			break;


		// the size of all newly added data, i.e. size without original data in IID
		// we need n additional IMAGE_IMPORT_DESCRIPTOR
		DWORD customDataSize = (DWORD)dlls.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR);

		// make sure the string sizes are padded to 32 bit boundary
		for (size_t i = 0; i < dlls.size(); ++i) customDataSize += padToDword(dlls[i].size() + 1);

		// IMAGE_THUNK_DATA, 2 entries each (OriginalFirstThunk+FirstThunk)
		customDataSize += 4 * sizeof(DWORD) * dlls.size();
		DWORD origIIDTblSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
		DWORD newDescrTblSize = customDataSize + origIIDTblSize;
		char* newDescrTbl = new char[newDescrTblSize];

		// allocate and build new import descriptor
		DWORD iba = ntHeaders.OptionalHeader.ImageBase;
		void* newDescrTblAddress = allocateMemAboveBase((void*)iba, newDescrTblSize);

		// RVA pointing directly behind the old table
		DWORD newTblRVA = (DWORD)newDescrTblAddress - iba + origIIDTblSize + dlls.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR);
		DWORD currentRVA = newTblRVA;

		// step 1: prepend new IID entries for our dlls and fill with the correct RVAs
		PIMAGE_IMPORT_DESCRIPTOR currentIDD = (PIMAGE_IMPORT_DESCRIPTOR)newDescrTbl;
		for (size_t i = 0; i < dlls.size(); ++i, ++currentIDD)
		{
			// layout: [<orig_first_thunk><IAT><name>]...[...]
			currentIDD->OriginalFirstThunk = currentIDD->FirstThunk = currentRVA;
			currentIDD->TimeDateStamp = currentIDD->ForwarderChain = 0;
			currentRVA += 4 * sizeof(DWORD);
			currentIDD->Name = currentRVA;
			currentRVA += padToDword(dlls[i].size() + 1);
		}

		// step 2: add old IID entries
		// we need to save the original import descriptor; read process memory directly into our new IID table
		for (PIMAGE_IMPORT_DESCRIPTOR descrAddr = m_pImportDescrTblAddr;; ++descrAddr, ++currentIDD)
		{
			if (m_cProcess.ReadMemory(descrAddr, currentIDD, sizeof(IMAGE_IMPORT_DESCRIPTOR)))
				break;

			if (currentIDD->FirstThunk == 0 && currentIDD->OriginalFirstThunk == 0) 
				break;
		}

		// step 3: build blocks made of IMAGE_THUNK_DATA, IAT and dll name string
		// let curBlock point after IIDs
		PDWORD curBlock = (PDWORD)(newDescrTbl + origIIDTblSize + dlls.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR));
		for (size_t i = 0; i < dlls.size(); ++i)
		{
			// force the dll to export at least one entry with ordinal 1
			// OriginalFirstThunk
			*curBlock++ = IMAGE_ORDINAL_FLAG | 1;
			*curBlock++ = 0;
			// FirstThunk
			*curBlock++ = IMAGE_ORDINAL_FLAG | 1;
			*curBlock++ = 0;
			memcpy((char*)curBlock, dlls[i].c_str(), dlls[i].size() + 1);
			curBlock += padToDword(dlls[i].size() + 1) >> 2;
		}

		// if IAT is zero, set it to VA of section which holds import directory (needed for delphi programs?!)
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress == 0)
		{
			uintptr_t sectionHdrAddr = ntHeadersAddr_ + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + ntHeaders.FileHeader.SizeOfOptionalHeader;
			IMAGE_SECTION_HEADER ish;
			for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i, sectionHdrAddr += sizeof(IMAGE_SECTION_HEADER))
			{
				m_cProcess.ReadMemory((LPVOID)sectionHdrAddr, &ish, sizeof(ish));
				if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress >= ish.VirtualAddress &&
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress < ish.VirtualAddress + ish.SizeOfRawData)
				{
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = ish.VirtualAddress;
					ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = ish.SizeOfRawData;
				}
			}
		}

		// finally write new descriptor, fix RVAs and update IMAGE_NT_HEADERS
		m_cProcess.WriteMemory(newDescrTblAddress, newDescrTbl, newDescrTblSize);
		DWORD newIIDRVA = (DWORD)newDescrTblAddress - iba;
		ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = newIIDRVA;
		ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = newDescrTblSize;

		// only clear BOUND directory if we need to
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress != -1
			&& ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress != 0)
		{
			ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
			ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
		}

		// handle injection in .NET process
		if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress != 0)
		{
			uintptr_t comDescriptorAddr = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress + iba;
			IMAGE_COR20_HEADER cor20Header;
			m_cProcess.ReadMemory((LPVOID)comDescriptorAddr, &cor20Header, sizeof(IMAGE_COR20_HEADER));
			if (cor20Header.Flags | COMIMAGE_FLAGS_ILONLY)
			{
				// pure IL executables trigger more restrictive PE header checks
				// so just remove the corresponding flag
				cor20Header.Flags = cor20Header.Flags & ~COMIMAGE_FLAGS_ILONLY;
				DWORD oldProtect = m_cProcess.ProtectMemory((LPVOID)comDescriptorAddr, sizeof(IMAGE_COR20_HEADER), PAGE_EXECUTE_READWRITE);
				m_cProcess.WriteMemory((LPVOID)comDescriptorAddr, &cor20Header, sizeof(IMAGE_COR20_HEADER));
				m_cProcess.ProtectMemory((LPVOID)comDescriptorAddr, sizeof(IMAGE_COR20_HEADER), oldProtect);
			}
		}

		// finally write new NT headers and reset page protection afterwards
		DWORD oldProtect = m_cProcess.ProtectMemory((void*)ntHeadersAddr_, sizeof(IMAGE_NT_HEADERS), PAGE_EXECUTE_READWRITE);
		m_cProcess.WriteMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS));
		m_cProcess.ProtectMemory((void*)ntHeadersAddr_, sizeof(IMAGE_NT_HEADERS), oldProtect);
		delete[] newDescrTbl;
	} while (false);

	return true;
}

void* IATModifier::allocateMemAboveBase(void* baseAddress, size_t size)
{
	try
	{
		MEMORY_BASIC_INFORMATION mbi;
		for (char* currentAddress = (char*)baseAddress;; currentAddress = (char*)mbi.BaseAddress + mbi.RegionSize)
		{
			
			if (false == m_cProcess.QueryMemory(currentAddress, &mbi))
				break;
			
			if (mbi.State != MEM_FREE) 
				continue;

			// walk memory region in allocation granularity steps
			char* bruteForce = (char*)pad((unsigned int)currentAddress, 0xFFFF);
			while (bruteForce < (char*)mbi.BaseAddress + mbi.RegionSize)
			{
				try
				{
					m_cProcess.AllocMemory(size, bruteForce, MEM_RESERVE | MEM_COMMIT);
					return bruteForce;
				}
				catch (MemoryAllocationException)
				{
				}
				bruteForce += 0x10000;
			}
		}
	}
	catch (const MemoryQueryException&)
	{
		return NULL;
	}

	return NULL;
}

IMAGE_NT_HEADERS IATModifier::readNTHeaders() const
{
	if (ntHeadersAddr_ == 0) throw std::runtime_error("Image base address has not been set - unable to retrieve IMAGE_NT_HEADERS");
	IMAGE_NT_HEADERS ntHeaders;
	m_cProcess.ReadMemory((void*)ntHeadersAddr_, &ntHeaders, sizeof(IMAGE_NT_HEADERS));
	return ntHeaders;
}