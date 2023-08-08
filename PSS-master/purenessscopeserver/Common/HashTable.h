#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "define.h"

enum class EM_HASH_STATE
{
	EM_SELECT = 0,
	EM_INSERT,
};

enum class EM_HASH_DEBUG
{
	HASH_DEBUG_ON = 0,
	HASH_DEBUG_OFF,
};

#define DEF_HASH_KEY_SIZE 20           //Ĭ�ϵ�Hash key����

//hash���ṹ
template <class T>
struct _Hash_Table_Cell
{
	int32 m_nPosIndex = 0;                       //��ǰ�����ڶ���������±�
	char* m_pKey = nullptr;                    //��ǰ��keyֵ
	T* m_pValue = nullptr;                    //��ǰ������ָ��
	short m_sKeyLen = DEF_HASH_KEY_SIZE;       //��ǰ��key����
	char  m_cExists = 0;                       //��ǰ���Ƿ��Ѿ�ʹ��,1�Ѿ�ʹ�ã�0û�б�ʹ��

	_Hash_Table_Cell()
	{
	}

	//��ʼ������
	void Init(char* pKey, int32 nKeyLen, int32 nIndex)
	{
		m_pKey = pKey;
		m_sKeyLen = nKeyLen;
		m_nPosIndex = nIndex;
	}

	//����������Ϣ
	void Clear()
	{
		m_cExists = 0;

		if (nullptr != m_pKey)
		{
			m_pKey[0] = '\0';
		}

		if (nullptr != m_pValue)
		{
			m_pValue = nullptr;
		}
	}

	//�õ���ǰ�����С
	int32 Get_Size(short sKeyLen)
	{
		return sizeof(_Hash_Table_Cell) + sKeyLen * sizeof(char);
	}
};

//hash�����ṹ
template <class T>
struct _Hash_Link_Info
{
	int32   m_cExists = 0;                //��ǰ���Ƿ��Ѿ�ʹ��,1�Ѿ�ʹ�ã�0û�б�ʹ��
	int32   m_nPosIndex = 0;                //��ǰ�����ڶ���������±�
	_Hash_Table_Cell<T>* m_pData = nullptr;             //��ǰ����
	_Hash_Link_Info* m_pNext = nullptr;             //��һ������ָ��(��������)
	_Hash_Link_Info* m_pPerv = nullptr;             //��һ������ָ��(��������)

	_Hash_Link_Info()
	{
	}

	//��ʼ������
	void Init(int32 nIndex)
	{
		m_cExists = 0;
		m_nPosIndex = nIndex;
	}

	//����������Ϣ
	void Clear()
	{
		m_cExists = 0;
		m_pData = nullptr;
		m_pNext = nullptr;
		m_pPerv = nullptr;
	}

	//�õ���ǰ�����С
	int32 Get_Size()
	{
		return sizeof(_Hash_Link_Info);
	}
};

//Hash����ؽṹ(����Hash���ݶ���ʵ��)
template <class T>
class CHashPool
{
public:
	CHashPool()
	{
	}

	~CHashPool()
	{
		Close();
	}

	int32 Get_Size(int32 nHashCount, short sKeySize = DEF_HASH_KEY_SIZE)
	{
		_Hash_Table_Cell<T> objCell;
		int32 nCellSize = objCell.Get_Size(sKeySize);
		return nCellSize * nHashCount;
	}

	void Init(char* pData, int32 nHashCount, short sKeySize = DEF_HASH_KEY_SIZE)
	{
		Set_Base_Addr(pData, nHashCount, sKeySize);
	}

	_Hash_Table_Cell<T>* Get_Index(int32 nIndex)
	{
		return &m_lpTable[nIndex];
	}

	void Close()
	{
		if (nullptr != m_pBase)
		{
			//delete[] m_pBase;
			m_pBase = nullptr;
			m_lpTable = nullptr;
		}

		m_nCount = 0;
		m_nUsedCount = 0;
		m_nCurrIndex = 0;
		m_sKeyLen = 0;
	}

	//����һ����֪���ڴ������(�����ʼ������)
	void Set_Base_Addr(char* pData, int32 nCount, short sKeySize)
	{
		m_pBase = pData;
		m_lpTable = reinterpret_cast<_Hash_Table_Cell<T>*>(pData);
		m_nCount = nCount;
		m_sKeyLen = sKeySize;
		m_nCurrIndex = 0;
		int32 nKeyPool = sizeof(_Hash_Table_Cell<T>) * nCount;

		for (int32 i = 0; i < m_nCount; i++)
		{
			char* pKey = pData + nKeyPool + i * sKeySize;
			m_lpTable[i].Init(pKey, sKeySize, i);
		}
	}

	//�õ���ǰkey�ĳ���
	short Get_Key_Length()
	{
		return m_sKeyLen;
	}

	//�õ���ǰ������ܸ���
	int32 Get_Count()
	{
		return m_nCount;
	}

	//�õ�Ŀǰ����ʹ�õĶ������
	int32 Get_Used_Count()
	{
		return m_nUsedCount;
	}

	//���һ������Ļ����
	_Hash_Table_Cell<T>* Create()
	{
		_Hash_Table_Cell<T>* pCurrCell = nullptr;

		if (nullptr == m_lpTable || m_nUsedCount == m_nCount)
		{
			return pCurrCell;
		}

		//���ȴӵ�ǰ��������
		for (int32 i = m_nCurrIndex; i < m_nCount; i++)
		{
			//printf("[CHashPool::Create]i=%d, m_nCurrIndex=%d, m_nCount=%d.\n", i, m_nCurrIndex, m_nCount);
			if (m_lpTable[i].m_cExists == 1)
			{
				continue;
			}
			else
			{
				pCurrCell = &m_lpTable[i];
				pCurrCell->m_cExists = 1;

				if (i < m_nCount - 1)
				{
					m_nCurrIndex = i + 1;
				}
				else
				{
					m_nCurrIndex = 0;
				}

				m_nUsedCount++;

				return pCurrCell;
			}
		}

		//���û�ҵ�����0����ǰ��
		for (int32 i = 0; i < m_nCurrIndex; i++)
		{
			if (m_lpTable[i].m_cExists == 1)
			{
				continue;
			}
			else
			{
				pCurrCell = &m_lpTable[i];
				pCurrCell->m_cExists = 1;
				m_nCurrIndex = i + 1;
				m_nUsedCount++;

				return pCurrCell;
			}
		}

		//�Ѿ�û�п���
		return pCurrCell;
	}

	//�ͷ�һ������ʹ�õĻ����
	void Delete(_Hash_Table_Cell<T>* pData)
	{
		if (nullptr != pData)
		{
			int32 nIndex = pData->m_nPosIndex;

			if (nIndex >= 0 && nIndex < m_nCount && pData->m_cExists == 1)
			{
				m_nUsedCount--;
				pData->Clear();
			}
		}
	}

private:
	char* m_pBase = nullptr;               //��ǰ�ڴ���ʼλ��
	_Hash_Table_Cell<T>* m_lpTable = nullptr;               //��ǰHash��������
	int32                m_nCount = 0;                  //��ǰHash�������
	int32                m_nUsedCount = 0;                  //��ǰ��ʹ�õ�Hash����
	int32                m_nCurrIndex = 0;                  //�Ѿ����е��ĵ�ǰHash����λ��
	short                m_sKeyLen = DEF_HASH_KEY_SIZE;  //��ǰkey�ĳ���
};

//HashTable������
template <class T>
class CHashLinkPool
{
public:
	CHashLinkPool()
	{
	}

	~CHashLinkPool()
	{
		Close();
	}

	int32 Get_Size(int32 nHashCount)
	{
		_Hash_Link_Info<T> objLink;
		int32 nLinkSize = objLink.Get_Size();
		return nLinkSize * nHashCount;
	}

	void Init(char* pData, int32 nHashCount)
	{
		Set_Base_Addr(pData, nHashCount);
	}

	void Close()
	{
		if (nullptr != m_pBase)
		{
			//delete[] m_pBase;
			m_pBase = nullptr;
			m_lpTable = nullptr;
		}

		m_nCount = 0;
		m_nUsedCount = 0;
		m_nCurrIndex = 0;
	}

	//����һ����֪���ڴ������(�����ʼ������)
	void Set_Base_Addr(char* pData, int32 nCount)
	{
		m_pBase = pData;
		m_lpTable = reinterpret_cast<_Hash_Link_Info<T>*>(pData);
		m_nCount = nCount;
		m_nCurrIndex = 0;

		for (int32 i = 0; i < m_nCount; i++)
		{
			m_lpTable[i].Init(i);
		}
	}

	//�õ���ǰ������ܸ���
	int32 Get_Count()
	{
		return m_nCount;
	}

	//�õ�Ŀǰ����ʹ�õĶ������
	int32 Get_Used_Count()
	{
		return m_nUsedCount;
	}

	//���һ������Ļ����
	_Hash_Link_Info<T>* Create()
	{
		_Hash_Link_Info<T>* pCurrCell = nullptr;

		if (nullptr == m_lpTable || m_nUsedCount == m_nCount)
		{
			return pCurrCell;
		}

		//���ȴӵ�ǰ��������
		for (int32 i = m_nCurrIndex; i < m_nCount; i++)
		{
			if (m_lpTable[i].m_cExists == 1)
			{
				continue;
			}
			else
			{
				pCurrCell = &m_lpTable[i];
				pCurrCell->m_cExists = 1;

				if (i < m_nCount - 1)
				{
					m_nCurrIndex = i + 1;
				}
				else
				{
					m_nCurrIndex = 0;
				}

				m_nUsedCount++;
				return pCurrCell;
			}
		}

		//���û�ҵ�����0����ǰ��
		for (int32 i = 0; i < m_nCurrIndex; i++)
		{
			if (m_lpTable[i].m_cExists == 1)
			{
				continue;
			}
			else
			{
				pCurrCell = &m_lpTable[i];
				pCurrCell->m_cExists = 1;
				m_nCurrIndex = i + 1;
				m_nUsedCount++;
				return pCurrCell;
			}
		}

		//�Ѿ�û�п���
		return pCurrCell;
	}

	//�ͷ�һ������ʹ�õĻ����
	void Delete(_Hash_Link_Info<T>* pData)
	{
		if (nullptr != pData)
		{
			int32 nIndex = pData->m_nPosIndex;

			if (nIndex >= 0 && nIndex < m_nCount && pData->m_cExists == 1)
			{
				m_nUsedCount--;
				pData->Clear();
			}
		}
	}

private:
	char* m_pBase = nullptr;    //��ǰ�ڴ���ʼλ��
	_Hash_Link_Info<T>* m_lpTable = nullptr;    //��ǰHash��������
	int32                m_nCount = 0;       //��ǰHash�������
	int32                m_nUsedCount = 0;       //��ǰ��ʹ�õ�Hash����
	int32                m_nCurrIndex = 0;       //�Ѿ����е��ĵ�ǰHash����λ��
};

//hashTable��
template <class T>
class CHashTable
{
public:
	CHashTable()
	{
	}

	~CHashTable()
	{
		Close();
	}

	//�õ����������ڴ��С
	size_t Get_Size(uint32 u4HashCount, short sKeySize = DEF_HASH_KEY_SIZE)
	{
		return m_objHashPool.Get_Size((int32)u4HashCount, sKeySize)
			+ m_objHashLinkPool.Get_Size((int32)u4HashCount)
			+ sizeof(_Hash_Link_Info<T>*) * u4HashCount;
	}

	//�ر�Hash��
	void Close()
	{
		if (0 == m_cIsDelete)
		{
			m_objHashPool.Close();
			m_objHashLinkPool.Close();
			delete[] m_pBase;
		}

		m_pBase = nullptr;
		m_lpTable = nullptr;
	}

	//��ʼ��Hash��
	void Init(int32 nHashCount, int32 nKeySize = DEF_HASH_KEY_SIZE, EM_HASH_DEBUG emHashDebug = EM_HASH_DEBUG::HASH_DEBUG_OFF)
	{
		size_t stSize = Get_Size(nHashCount, nKeySize);
		char* pData = new char[stSize];
		memset(pData, 0, stSize);
		int32 nPos = 0;
		m_pBase = pData;
		m_nCurrLinkIndex = 0;
		m_cIsDelete = 0;
		m_objHashPool.Init(&pData[nPos], nHashCount, nKeySize);
		nPos += m_objHashPool.Get_Size(nHashCount, nKeySize);
		m_objHashLinkPool.Init(&pData[nPos], nHashCount);
		nPos += m_objHashLinkPool.Get_Size(nHashCount);
		m_lpTable = (_Hash_Link_Info<T>**) & pData[nPos];

		for (int32 i = 0; i < nHashCount; i++)
		{
			m_lpTable[i] = nullptr;
		}

		m_emHashDebug = emHashDebug;
		//nPos += sizeof(_Hash_Link_Info<T>* ) * nHashCount;
	}

	//��ʼ��Hash��(�����ڴ��ַ)
	void Init_By_Memory(char* pData, int32 nHashCount, int32 nKeySize = DEF_HASH_KEY_SIZE, EM_HASH_DEBUG emHashDebug = EM_HASH_DEBUG::HASH_DEBUG_OFF, char cIsDelete = 0)
	{
		memset(pData, 0, Get_Size(nHashCount, nKeySize));
		int32 nPos = 0;
		m_pBase = pData;
		m_nCurrLinkIndex = 0;
		m_cIsDelete = cIsDelete;
		m_objHashPool.Init(&pData[nPos], nHashCount, nKeySize);
		nPos += m_objHashPool.Get_Size(nHashCount, nKeySize);
		m_objHashLinkPool.Init(&pData[nPos], nHashCount);
		nPos += m_objHashLinkPool.Get_Size(nHashCount);
		m_lpTable = (_Hash_Link_Info<T>**) & pData[nPos];

		for (int32 i = 0; i < nHashCount; i++)
		{
			m_lpTable[i] = nullptr;
		}

		m_emHashDebug = emHashDebug;
		//nPos += sizeof(_Hash_Link_Info<T>* ) * nHashCount;
		//printf("[Init]nPos=%d.\n", nPos);
	}

	//�õ���ǰ��������
	int32 Get_Count()
	{
		return m_objHashPool.Get_Count();
	}

	//�õ�����������ʹ�õĸ���
	int32 Get_Used_Count()
	{
		return m_objHashPool.Get_Used_Count();
	}

	//������һ������HashTable�е�pt
	T* Pop_Uint32(uint32& u4Pos)
	{
		T* pT = nullptr;

		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Pop]m_lpTable is nullptr.");
			}

			return pT;
		}

		//Ѱ��һ�������õĶ��󣬵�������
		for (int32 i = m_nCurrLinkIndex; i < m_objHashPool.Get_Count(); i++)
		{
			if (m_lpTable[i] != nullptr)
			{
				//ȡ����ǰ������
				pT = m_lpTable[i]->m_pData->m_pValue;
				char* pKey = m_lpTable[i]->m_pData->m_pKey;

				//����״̬
				m_nCurrLinkIndex = i;

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]1 pKey = {0}.", pKey);
				}

				//��������
				int32 nRet = Del_Hash_Data_By_Unit32(i);

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]1 index={0}, Del_Hash_Data({1}), Currused={2} pT={3}.", m_nCurrLinkIndex, nRet, Get_Used_Count(), fmt::ptr(pT));
				}

				u4Pos = i;
				return pT;
			}
		}

		for (int32 i = 0; i < m_nCurrLinkIndex; i++)
		{
			if (m_lpTable[i] != nullptr)
			{
				//ȡ����ǰ������
				pT = m_lpTable[i]->m_pData->m_pValue;
				char* pKey = m_lpTable[i]->m_pData->m_pKey;

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]2 pKey = {0}.", pKey);
				}

				//����״̬
				m_nCurrLinkIndex = i;

				//��������
				int32 nRet = Del_Hash_Data_By_Unit32(i);

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]2 index={0}, Del_Hash_Data{1}, Currused={2}, pT={3}.", m_nCurrLinkIndex, nRet, Get_Used_Count(), fmt::ptr(pT));
				}

				u4Pos = i;
				return pT;
			}
		}

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[Pop]2 index={0}, no Find, Currused={1}.", m_nCurrLinkIndex, Get_Used_Count());
		}

		return pT;
	}

	//����һ���������е�_Hash_Link_Info<T>* pT
	T* Pop()
	{
		T* pT = nullptr;

		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Pop]m_lpTable is nullptr.");
			}

			return pT;
		}

		//Ѱ��һ�������õĶ��󣬵�������
		for (int32 i = m_nCurrLinkIndex; i < m_objHashPool.Get_Count(); i++)
		{
			if (m_lpTable[i] != nullptr)
			{
				//ȡ����ǰ������
				pT = m_lpTable[i]->m_pData->m_pValue;
				char* pKey = m_lpTable[i]->m_pData->m_pKey;

				//����״̬
				if (i < m_objHashPool.Get_Count() - 1)
				{
					m_nCurrLinkIndex = i;
				}
				else
				{
					m_nCurrLinkIndex = 0;
				}

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]1 pKey = {0}.", pKey);
				}

				//��������
				int32 nRet = Del_Hash_Data(pKey);

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]1 index={0}, Del_Hash_Data({1}), Currused={2} pT={3}.", m_nCurrLinkIndex, nRet, Get_Used_Count(), fmt::ptr(pT));
				}

				return pT;
			}
		}

		for (int32 i = 0; i < m_nCurrLinkIndex; i++)
		{
			if (m_lpTable[i] != nullptr)
			{
				//ȡ����ǰ������
				pT = m_lpTable[i]->m_pData->m_pValue;
				char* pKey = m_lpTable[i]->m_pData->m_pKey;

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]2 pKey = {0}.", pKey);
				}

				//����״̬
                if (i < m_objHashPool.Get_Count() - 1)
                {
                    m_nCurrLinkIndex = i + 1;
                }
                else
                {
                    m_nCurrLinkIndex = 0;
                }

				//��������
				int32 nRet = Del_Hash_Data(pKey);

				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Pop]2 index={0}, Del_Hash_Data({1}), Currused={2}, pT={3}.", m_nCurrLinkIndex, nRet, Get_Used_Count(), fmt::ptr(pT));
				}

				return pT;
			}
		}

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[Pop]2 index={0}, no Find, Currused={1}.", m_nCurrLinkIndex, Get_Used_Count());
		}

		return pT;
	}

	//��һ�����󻹸�HashTable
	int32 Push_Uint32(uint32 u4Pos, T* pT)
	{
		return Add_Hash_Data_By_Key_Unit32(u4Pos, pT);
	}

	//��һ���Ѿ�ʹ����ɵĶ��󣬷Żص�����
	bool Push(const char* pKey, T* pT)
	{
		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Push] m_lpTable is nullptr, pKey={0}, pT={1}.", pKey, fmt::ptr(pT));
			}

			return false;
		}

		//����key����������hashλ��
		unsigned long uHashStart = HashString(pKey, m_objHashPool.Get_Count());

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[Push] index ={0}, pKey={1}, pT={2}.", uHashStart, pKey, fmt::ptr(pT));
		}

		_Hash_Link_Info<T>* pLastLink = m_lpTable[uHashStart];

		while (nullptr != pLastLink)
		{
			_Hash_Link_Info<T>* pTempLink = pLastLink->m_pNext;

			if (nullptr != pTempLink)
			{
				pLastLink = pTempLink;
			}
			else
			{
				break;
			}
		}

		//�Ӷ�����л�ȡһ���¶���
		_Hash_Table_Cell<T>* pData = m_objHashPool.Create();

		if (nullptr == pData)
		{
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Push]m_objHashPool pData = nullptr.");
			}

			return false;
		}

		_Hash_Link_Info<T>* pLink = m_objHashLinkPool.Create();

		if (nullptr == pLink)
		{
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Push]m_objHashLinkPool pData = nullptr.");
			}

			return false;
		}

		sprintf_safe(pData->m_pKey, pData->m_sKeyLen, "%s", pKey);
		pData->m_pValue = pT;
		pLink->m_pData = pData;
		pLink->m_pPerv = pLastLink;

		if (pLastLink != nullptr)
		{
			pLastLink->m_pNext = pLink;
		}
		else
		{
			m_lpTable[uHashStart] = pLink;
		}

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[Push] index ={0}, pKey={1}, Currused={2}, pT={3} OK.", m_emHashDebug, pKey, Get_Used_Count(), fmt::ptr(pT));
		}

		return true;
	}

	//�õ���������ʹ�õ�ָ��
	void Get_All_Used(vector<T*>& vecList)
	{
		if (nullptr == m_lpTable)
		{
			return;
		}

		vecList.clear();

		for (int32 i = 0; i < m_objHashPool.Get_Count(); i++)
		{
			if (nullptr != m_lpTable[i])
			{
				_Hash_Link_Info<T>* pLastLink = m_lpTable[i];

				while (nullptr != pLastLink)
				{
					if (nullptr != pLastLink->m_pData)
					{
						vecList.push_back(pLastLink->m_pData->m_pValue);
					}
					else
					{
						PSS_LOGGER_DEBUG("[CHashTable::Get_All_Used]pLastLink->m_pData is nullptr.");
					}

					pLastLink = pLastLink->m_pNext;
				}
			}
		}
	}

	//�õ���������ʹ�õ�key
	void Get_All_Used_Key(vector<string>& vecList)
	{
		if (nullptr == m_lpTable)
		{
			return;
		}

		vecList.clear();

		for (int32 i = 0; i < m_objHashPool.Get_Count(); i++)
		{
			if (nullptr != m_lpTable[i])
			{
				_Hash_Link_Info<T>* pLastLink = m_lpTable[i];

				while (nullptr != pLastLink)
				{
					if (nullptr != pLastLink->m_pData)
					{
						vecList.push_back(pLastLink->m_pData->m_pKey);
					}
					else
					{
						PSS_LOGGER_DEBUG("[CHashTable::Get_All_Used_Key]pLastLink->m_pData is nullptr.");
					}

					pLastLink = pLastLink->m_pNext;
				}
			}
		}
	}

	//������ǰHash�����е����ж���ָ��
	void Clear()
	{
		vector<string> vecList;

		if (nullptr == m_lpTable)
		{
			return;
		}

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[Clear]({0})Data is Clear.", Get_Used_Count());
		}

		for (int32 i = 0; i < m_objHashPool.Get_Count(); i++)
		{
			if (nullptr != m_lpTable[i])
			{
				_Hash_Link_Info<T>* pLastLink = m_lpTable[i];

				while (nullptr != pLastLink)
				{
					if (nullptr != pLastLink->m_pData)
					{
						vecList.push_back(pLastLink->m_pData->m_pKey);
					}
					else
					{
						PSS_LOGGER_DEBUG("[CHashTable::Clear]pLastLink->m_pData is nullptr.");
					}

					pLastLink = pLastLink->m_pNext;
				}
			}
		}

		//��������
		for (int32 i = 0; i < (int32)vecList.size(); i++)
		{
			if (-1 == Del_Hash_Data(vecList[i].c_str()))
			{
				PSS_LOGGER_DEBUG("[CHashTable::Clear]({0}) is Delete error.", vecList[i].c_str());
			}
		}
	}

	void Clear_Unit32()
	{
		vector<uint32> vecList;

		if (nullptr == m_lpTable)
		{
			return;
		}

		if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
		{
			PSS_LOGGER_DEBUG("[CHashTable::Clear_Unit32]({0})Data is Clear.", Get_Used_Count());
		}

		for (uint32 i = 0; i < (uint32)m_objHashPool.Get_Count(); i++)
		{
			if (nullptr != m_lpTable[i])
			{
				_Hash_Link_Info<T>* pLastLink = m_lpTable[i];

				while (nullptr != pLastLink)
				{
					if (nullptr != pLastLink->m_pData)
					{
						vecList.push_back(i);
					}
					else
					{
						PSS_LOGGER_DEBUG("[CHashTable::Clear]pLastLink->m_pData is nullptr.");
					}

					pLastLink = pLastLink->m_pNext;
				}
			}
		}

		//��������
		for (int32 i = 0; i < (int32)vecList.size(); i++)
		{
			if (-1 == Del_Hash_Data_By_Unit32(vecList[i]))
			{
				PSS_LOGGER_DEBUG("[CHashTable::Clear_Unit32]({0}) is Delete error.", vecList[i]);
			}
		}
	}

	//����һ��Hash���ݿ�(keyΪһ������)
	//�����Զ�����������Ϊ�˼��ٽ�����ת��Ϊ�ַ�������Hsah��������衣
	//ֱ�Ӳ���key��Hash������±�
	int32 Add_Hash_Data_By_Key_Unit32(uint32 u4Key, T* pValue)
	{
		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Add_Hash_Data]m_lpTable is nullptr.");
			}

			return -1;
		}

		T* pT = nullptr;
		int32 nPos = GetHashTablePos_By_HashIndex((unsigned long)u4Key, EM_HASH_STATE::EM_INSERT, pT);

		if (-1 == nPos)
		{
			//�ڴ����������������Ѿ�����
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Add_Hash_Data]GetHashTablePos is -1.");
			}

			return -1;
		}
		else
		{
			if (nullptr == m_lpTable[nPos])
			{
				//�Ӷ�����л�ȡһ���¶���
				_Hash_Table_Cell<T>* pData = m_objHashPool.Create();

				if (nullptr == pData)
				{
					return -1;
				}

				_Hash_Link_Info<T>* pLink = m_objHashLinkPool.Create();

				if (nullptr == pLink)
				{
					return -1;
				}
				else
				{
					sprintf_safe(pData->m_pKey, pData->m_sKeyLen, "%d", u4Key);
					pData->m_pValue = pValue;
					pLink->m_pData = pData;
					m_lpTable[nPos] = pLink;
				}
			}
			else
			{
				//����Ѿ����ڶ������ҵ���ǰ���������һ��������֮
				_Hash_Link_Info<T>* pLastLink = m_lpTable[nPos];

				while (nullptr != pLastLink)
				{
					if (pLastLink->m_pNext == nullptr)
					{
						break;
					}

					pLastLink = pLastLink->m_pNext;
				}

				//�Ӷ�����л�ȡһ���¶���
				_Hash_Table_Cell<T>* pData = m_objHashPool.Create();

				if (nullptr == pData)
				{
					return -1;
				}

				_Hash_Link_Info<T>* pLink = m_objHashLinkPool.Create();

				if (nullptr == pLink)
				{
					return -1;
				}
				else
				{
					sprintf_safe(pData->m_pKey, pData->m_sKeyLen, "%d", u4Key);
					pData->m_pValue = pValue;
					pLink->m_pData = pData;
					pLink->m_pPerv = pLastLink;

					if (nullptr != pLastLink)
					{
						pLastLink->m_pNext = pLink;
					}
				}
			}

			return nPos;
		}
	}

	//����һ��Hash���ݿ�
	int32 Add_Hash_Data(const char* pKey, T* pValue)
	{
		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Add_Hash_Data]m_lpTable is nullptr.");
			}

			return -1;
		}

		if ((short)strlen(pKey) >= m_objHashPool.Get_Key_Length())
		{
			//��ǰkey�ĳ��ȳ�����HashTable��key���ȡ�
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Add_Hash_Data](short)strlen(pKey) >= m_objHashPool.Get_Key_Length().");
			}

			return -1;
		}

		T* pT = nullptr;
		int32 nPos = GetHashTablePos(pKey, EM_HASH_STATE::EM_INSERT, pT);

		if (-1 == nPos)
		{
			//�ڴ����������������Ѿ�����
			if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
			{
				PSS_LOGGER_DEBUG("[Add_Hash_Data]GetHashTablePos is -1.");
			}

			return -1;
		}
		else
		{
			if (nullptr == m_lpTable[nPos])
			{
				//�Ӷ�����л�ȡһ���¶���
				_Hash_Table_Cell<T>* pData = m_objHashPool.Create();

				if (nullptr == pData)
				{
					return -1;
				}

				_Hash_Link_Info<T>* pLink = m_objHashLinkPool.Create();

				if (nullptr == pLink)
				{
					return -1;
				}
				else
				{
					sprintf_safe(pData->m_pKey, pData->m_sKeyLen, "%s", pKey);
					pData->m_pValue = pValue;
					pLink->m_pData = pData;
					m_lpTable[nPos] = pLink;

					if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
					{
						PSS_LOGGER_DEBUG("[Test]Add 1 pKey={0}, pData->m_pKey={1}.", pKey, pData->m_pKey);
					}
				}
			}
			else
			{
				//����Ѿ����ڶ������ҵ���ǰ���������һ��������֮
				_Hash_Link_Info<T>* pLastLink = m_lpTable[nPos];

				while (nullptr != pLastLink)
				{
					if (pLastLink->m_pNext == nullptr)
					{
						break;
					}

					pLastLink = pLastLink->m_pNext;
				}

				//�Ӷ�����л�ȡһ���¶���
				_Hash_Table_Cell<T>* pData = m_objHashPool.Create();

				if (nullptr == pData)
				{
					return -1;
				}

				_Hash_Link_Info<T>* pLink = m_objHashLinkPool.Create();

				if (nullptr == pLink)
				{
					return -1;
				}
				else
				{
					sprintf_safe(pData->m_pKey, pData->m_sKeyLen, "%s", pKey);
					pData->m_pValue = pValue;
					pLink->m_pData = pData;
					pLink->m_pPerv = pLastLink;

					if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
					{
						PSS_LOGGER_DEBUG("[Test]Add 2 pKey={0}, pData->m_pKey={1}.", pKey, pData->m_pKey);
					}

					if (nullptr != pLastLink)
					{
						pLastLink->m_pNext = pLink;
					}
				}
			}

			return nPos;
		}
	}

	//���һ������ӳ���Ӧ��ֵ
	T* Get_Hash_Box_Data(const char* pKey)
	{
		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			return nullptr;
		}

		T* pT = nullptr;
		int32 nPos = GetHashTablePos(pKey, EM_HASH_STATE::EM_SELECT, pT);

		if (-1 == nPos)
		{
			//û���ҵ�
			return nullptr;
		}
		else
		{
			return pT;
		}
	}

	T* Get_Hash_Box_Data_By_Uint32(uint32 u4Key)
	{
		if (nullptr == m_lpTable)
		{
			//û���ҵ������ڴ�
			return nullptr;
		}

		T* pT = nullptr;
		int32 nPos = GetHashTablePos_By_HashIndex(u4Key, EM_HASH_STATE::EM_SELECT, pT);

		if (-1 == nPos)
		{
			//û���ҵ�
			return nullptr;
		}
		else
		{
			return pT;
		}
	}

	//����һ��Hash���ݿ飬����Unit32
	int32 Del_Hash_Data_By_Unit32(uint32 u4Key)
	{
		return DelHashTablePos_By_HashIndex((unsigned long)u4Key);
	}

	//����һ��hash���ݿ�
	int32 Del_Hash_Data(const char* pKey)
	{
		return DelHashTablePos(pKey);
	}

private:
	//����key��Ӧ��hash��ֵ
	unsigned long HashString(const char* pKey, int32 nCount)
	{
		unsigned char* pukey = (unsigned char*)pKey;
		uint32 seed = 131; /* 31 131 1313 13131 131313 etc..*/
		uint32 h = 0;

		while (*pukey)
		{
			h = h * seed + (*pukey++);
		}

		h = h & 0x7FFFFFFF;

		if (0 == nCount)
		{
			return h;
		}
		else
		{
			return h % nCount;
		}
	}

	//�����ṩ���±꣬Ѱ��������ָ����Hash����
	int32 GetHashTablePos_By_HashIndex(unsigned long uHashStart, EM_HASH_STATE emHashState, T*& pT)
	{
		char szCurrKey[DEF_HASH_KEY_SIZE] = { '\0' };
		sprintf_safe(szCurrKey, DEF_HASH_KEY_SIZE, "%d", uHashStart);

		if (m_objHashPool.Get_Count() == 0)
		{
			return -1;
		}

		//�ѵ�ǰ���ֶԵ�ǰHash��������ȡ��
		unsigned long uHashPos = uHashStart % m_objHashPool.Get_Count();

		//��ȡ���������ȶ�
		if (nullptr == m_lpTable[uHashPos])
		{
			if (EM_HASH_STATE::EM_INSERT == emHashState)
			{
				return uHashPos;
			}
			else
			{
				return -1;
			}
		}
		else
		{
			_Hash_Link_Info<T>* pLastLink = m_lpTable[uHashPos];

			while (nullptr != pLastLink)
			{
				if (nullptr != pLastLink->m_pData && strcmp(pLastLink->m_pData->m_pKey, szCurrKey) == 0)
				{
					//�ҵ��˶�Ӧ��key,��������Ѿ�����
					if (EM_HASH_STATE::EM_INSERT == emHashState)
					{
						return -1;
					}
					else
					{
						pT = pLastLink->m_pData->m_pValue;
						return uHashPos;
					}
				}

				pLastLink = pLastLink->m_pNext;
			}

			return uHashPos;
		}
	}

	//�õ�hashָ����λ��
	int32 GetHashTablePos(const char* lpszString, EM_HASH_STATE emHashState, T*& pT)
	{
		if (m_objHashPool.Get_Count() == 0)
		{
			return -1;
		}

		unsigned long uHashStart = HashString(lpszString, m_objHashPool.Get_Count());

		//��ȡ���������ȶ�
		if (nullptr == m_lpTable[uHashStart])
		{
			if (EM_HASH_STATE::EM_INSERT == emHashState)
			{
				return uHashStart;
			}
			else
			{
				return -1;
			}
		}
		else
		{
			_Hash_Link_Info<T>* pLastLink = m_lpTable[uHashStart];

			while (nullptr != pLastLink)
			{
				if (nullptr != pLastLink->m_pData && strcmp(pLastLink->m_pData->m_pKey, lpszString) == 0)
				{
					//�ҵ��˶�Ӧ��key,��������Ѿ�����
					if (EM_HASH_STATE::EM_INSERT == emHashState)
					{
						return -1;
					}
					else
					{
						pT = pLastLink->m_pData->m_pValue;
						return uHashStart;
					}
				}

				pLastLink = pLastLink->m_pNext;
			}

			return uHashStart;
		}
	}

	//�����ṩ���±꣬ɾ��������ָ����Hash����
	int32 DelHashTablePos_By_HashIndex(unsigned long uHashStart)
	{
		char szCurrKey[DEF_HASH_KEY_SIZE] = { '\0' };
		sprintf_safe(szCurrKey, DEF_HASH_KEY_SIZE, "%d", uHashStart);

		if (m_objHashPool.Get_Count() == 0)
		{
			return -1;
		}

		//�ѵ�ǰ���ֶԵ�ǰHash��������ȡ��
		unsigned long uHashPos = uHashStart % m_objHashPool.Get_Count();

		//��ȡ���������ȶ�
		if (nullptr == m_lpTable[uHashPos])
		{
			return -1;
		}
		else
		{
			_Hash_Link_Info<T>* pLastLink = m_lpTable[uHashPos];

			while (nullptr != pLastLink)
			{
				if (nullptr != pLastLink->m_pData)
				{
					//�ҵ��˶�Ӧ��key,��������Ѿ�����
					if (pLastLink->m_pPerv == nullptr)
					{
						m_lpTable[uHashPos] = pLastLink->m_pNext;

						if (nullptr != pLastLink->m_pNext)
						{
							pLastLink->m_pNext->m_pPerv = nullptr;
						}
					}
					else
					{
						pLastLink->m_pPerv->m_pNext = pLastLink->m_pNext;

						if (nullptr != pLastLink->m_pNext)
						{
							pLastLink->m_pNext->m_pPerv = pLastLink->m_pPerv;
						}
					}

					//����ָ�������Ͷ���
					m_objHashPool.Delete(pLastLink->m_pData);
					m_objHashLinkPool.Delete(pLastLink);
					return 0;
				}

				pLastLink = pLastLink->m_pNext;
			}

			return -1;
		}
	}

	//ɾ��ָ��������
	int32 DelHashTablePos(const char* lpszString)
	{
		if (m_objHashPool.Get_Count() == 0)
		{
			return -1;
		}

		unsigned long uHashStart = HashString(lpszString, m_objHashPool.Get_Count());

		//��ȡ���������ȶ�
		if (nullptr == m_lpTable[uHashStart])
		{
			return -1;
		}
		else
		{
			_Hash_Link_Info<T>* pLastLink = m_lpTable[uHashStart];

			while (nullptr != pLastLink)
			{
				if (EM_HASH_DEBUG::HASH_DEBUG_ON == m_emHashDebug)
				{
					PSS_LOGGER_DEBUG("[Test]lpszString={0}, pLastLink->m_pData->m_pKey={1}.", lpszString, pLastLink->m_pData->m_pKey);
				}

				if (nullptr != pLastLink->m_pData && strcmp(pLastLink->m_pData->m_pKey, lpszString) == 0)
				{
					//�ҵ��˶�Ӧ��key,��������Ѿ�����
					if (pLastLink->m_pPerv == nullptr)
					{
						m_lpTable[uHashStart] = pLastLink->m_pNext;

						if (nullptr != pLastLink->m_pNext)
						{
							pLastLink->m_pNext->m_pPerv = nullptr;
						}
					}
					else
					{
						pLastLink->m_pPerv->m_pNext = pLastLink->m_pNext;

						if (nullptr != pLastLink->m_pNext)
						{
							pLastLink->m_pNext->m_pPerv = pLastLink->m_pPerv;
						}
					}

					//����ָ�������Ͷ���
					m_objHashPool.Delete(pLastLink->m_pData);
					m_objHashLinkPool.Delete(pLastLink);
					return 0;
				}

				pLastLink = pLastLink->m_pNext;
			}

			return -1;
		}
	}

private:
	CHashPool<T>         m_objHashPool;                                       //Hash�����
	CHashLinkPool<T>     m_objHashLinkPool;                                   //Hash���������
	_Hash_Link_Info<T>** m_lpTable = nullptr;                           //��ǰHash��������
	int32                m_nCurrLinkIndex = 0;                             //��ǰ����λ��
	char* m_pBase = nullptr;                           //�ڴ��Ļ�����ַ
	char                 m_cIsDelete = 0;                              //��ǰ��������ʱ���Ƿ�����ڴ棬0�ǻ��գ�1�ǲ����ա�
	EM_HASH_DEBUG        m_emHashDebug = EM_HASH_DEBUG::HASH_DEBUG_OFF;  //�Ƿ���DEBUG����
};


#endif