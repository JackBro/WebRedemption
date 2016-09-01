#pragma once
#include  <stdio.h>
#include <Windows.h>
#include  "Competence.h"
#include <iphlpapi.h>

#pragma comment(lib,"iphlpapi.lib")

namespace Common{

	unsigned long IP2LONG(const char *ip)
	{
		char szAddress[MAX_IP4_LEN] = { 0 };

		sscanf(ip, "%d.%d.%d.%d", szAddress, szAddress + 1, szAddress + 2, szAddress + 3);

		return *((unsigned long*)(&szAddress));
	}

	bool GetMACAddr(char * pszMACBuffer, char * pszIPBuffer /*= NULL*/)
	{
		bool bIsOK = false;
		unsigned long uBufferLength = 0;//�õ��ṹ���С,����GetAdaptersInfo����
		PIP_ADAPTER_INFO pIpAdapterInfo = NULL; //PIP_ADAPTER_INFO�ṹ��ָ��洢����������Ϣ

		unsigned long uStatus = GetAdaptersInfo(pIpAdapterInfo, &uBufferLength);//����GetAdaptersInfo����,���pIpAdapterInfoָ�����;����stSize��������һ��������Ҳ��һ�������

		if (ERROR_BUFFER_OVERFLOW == uStatus) //����������ص���ERROR_BUFFER_OVERFLOW��˵��GetAdaptersInfo�������ݵ��ڴ�ռ䲻��,ͬʱ�䴫��stSize,��ʾ��Ҫ�Ŀռ��С
		{
			pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[uBufferLength];
			uStatus = GetAdaptersInfo(pIpAdapterInfo, &uBufferLength);//�ٴε���GetAdaptersInfo����,���pIpAdapterInfoָ�����
		}

		do 
		{
			if (ERROR_SUCCESS != uStatus)
				break;

			for (UINT i = 0; i < 6; i++, pszMACBuffer += 3)
				sprintf(pszMACBuffer, "%02x%c", pIpAdapterInfo->Address[i], (i == pIpAdapterInfo->AddressLength - 1) ? '\0' : '-');

			if (pszIPBuffer)
				memcpy(pszIPBuffer, pIpAdapterInfo->IpAddressList.IpAddress.String, 16);

			bIsOK = true;
		} while (false);

		if (pIpAdapterInfo)
			delete pIpAdapterInfo;

		return bIsOK;
	}

	void MACConverter(char * pszMACString, byte * pszMACBuffer)
	{
		for (UINT i = 0; i < 6; i++, pszMACBuffer += 3)
			sscanf(pszMACString, "%02x-%02x-%02x-%02x-%02x-%02x", pszMACBuffer, pszMACBuffer + 1, pszMACBuffer + 2, pszMACBuffer + 3, pszMACBuffer + 4, pszMACBuffer + 5);

	}

	void MACConverter(byte * pszMACBuffer, char * pszMACString)
	{
		for (UINT i = 0; i < 6; i++, pszMACString += 3)
			sprintf(pszMACString, "%02x%c", pszMACBuffer[i], (i == 5) ? '\0' : '-');
	}

	bool GetMACAddr(byte * pszMACBuffer)
	{
		unsigned long uBufferLength = 0;//�õ��ṹ���С,����GetAdaptersInfo����
		PIP_ADAPTER_INFO pIpAdapterInfo = NULL; //PIP_ADAPTER_INFO�ṹ��ָ��洢����������Ϣ

		unsigned long uStatus = GetAdaptersInfo(pIpAdapterInfo, &uBufferLength);//����GetAdaptersInfo����,���pIpAdapterInfoָ�����;����stSize��������һ��������Ҳ��һ�������

		if (ERROR_BUFFER_OVERFLOW == uStatus) //����������ص���ERROR_BUFFER_OVERFLOW��˵��GetAdaptersInfo�������ݵ��ڴ�ռ䲻��,ͬʱ�䴫��stSize,��ʾ��Ҫ�Ŀռ��С
		{
			pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[uBufferLength];
			uStatus = GetAdaptersInfo(pIpAdapterInfo, &uBufferLength);//�ٴε���GetAdaptersInfo����,���pIpAdapterInfoָ�����
		}

		if (ERROR_SUCCESS != uStatus)
			return false;

		if (pszMACBuffer)
			memcpy(pszMACBuffer, pIpAdapterInfo->Address, 8);

		if (pIpAdapterInfo)
			delete pIpAdapterInfo;

		return true;
	}
};
