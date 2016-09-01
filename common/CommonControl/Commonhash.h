#pragma once

#include "Commondef.h"

namespace HASH {

	static int GetCRC32(const void * InStr, int len)
	{
		//����Crc32�Ĳ�ѯ��
		int crc_table[256];
		int i, j;
		int crc;

		for (i = 0; i < 256; i++) {
			crc = (int)i;
			for (j = 0; j < 8; j++) {
				if (crc & 1)
					crc = 0xedb88320L ^ (crc >> 1);
				else
					crc = crc >> 1;
			}
			crc_table[i] = crc;
		}

		//��ʼ����CRC32У��ֵ
		crc = 0xffffffff;
		for (i = 0; i < len; i++) {
			crc = crc_table[(crc ^ ((const byte *)InStr)[i]) & 0xff] ^ (crc >> 8);
		}

		//Crc ^= 0xFFFFFFFF;
		return crc;
	}
}