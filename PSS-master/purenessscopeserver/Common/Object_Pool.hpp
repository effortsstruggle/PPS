#pragma once
#include "define.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <assert.h>
#include "ACEMemory.hpp"
#include "ThreadLock.h"

/*
˵��:	
CObjectPool<T>Ϊ������� ���ɺ͹������г��������͵Ķ���.
CObjectPool_FactoryΪ����ع���,������,���ɺ͹������е�CObjectPool<T>����

1.�������ù̶�����,һ�����ú������������ɱ�.
2.�̶�����Ϊ0,�����Զ���������,���ҿ���ʹ��GC����.�������Ķ���.

by liuruiqi 2019.6.20
*/

// ����ػ���ӿڣ��ṩ��gc�����ռ�������
class IObjectPool
{
public:
	// �����ռ�
	virtual void GC(bool bEnforce) = 0;
	virtual ~IObjectPool() {}
};

// ����ص�ʵ���࣬ͨ��ģ��ʵ��ͨ�û�
template<class ObjectType>
class CObjectPool : public IObjectPool
{
private:
	typedef std::unordered_map<int, ObjectType*>  FreePointer;
	typedef std::vector<ObjectType*>   FreeIndex;
public:
	CObjectPool()
	{
		m_isFixed = false;
	}

	~CObjectPool()
	{
		GC(true);
	}

	//���ù̶�����uFixedLength��ֵ
	//uFixedLength����0,�ڴ��������Զ���������GC�ɻ���,����Ϊ�̶�����GC����
	void Init(uint32 uFixedLength = 0)
	{
		CAutoLock lock(&m_lock);
		if (0 == uFixedLength)
		{
			//��64��Ϊ��һ������,֮��ÿ������һ��
			m_growSize = 64;
			m_CurrentObject = 0;
			Grow();
			m_isFixed = false;
		}
		else
		{
			m_growSize = uFixedLength;
			Grow();
			m_isFixed = true;
		}

		SetFixedLength(uFixedLength);
	}

	bool isFiexdPool()
	{
		return m_isFixed;
	}

	uint32 GetCurrentObjects()
	{
		return m_CurrentObject;
	}

	//�޲ι���
	ObjectType* Construct()
	{
		CAutoLock lock(&m_lock);

		auto pData = GetFreePointer();
		if (pData == nullptr)
		{
			//��¼��־
			OUR_DEBUG((LM_ERROR, "[CObjectPool::Construct()] pData == nullptr (%s).(%d)\n", __FILE__, __LINE__));
			assert(pData);
			return nullptr;
		}

		ObjectType * const ret = new (pData)ObjectType();

		return ret;
	}

	//���ι���
	template<class ... Args>
	ObjectType* Construct(Args && ... args)
	{
		CAutoLock lock(&m_lock);

		ObjectType* pData = GetFreePointer();
		if (pData == nullptr)
		{
			//��¼��־
			OUR_DEBUG((LM_ERROR, "[CObjectPool::Construct(Args && ... args)] pData == nullptr (%s).(%d)\n", __FILE__, __LINE__));
			assert(pData);
			return nullptr;
		}

		//����ģ��Ĺ��캯��
		ObjectType* const ret = new (pData) ObjectType(std::forward<Args>(args)...);

		return ret;
	}

	// ����һ������
	void Destroy(ObjectType* const object)
	{
		CAutoLock lock(&m_lock);

		object->~ObjectType();
		m_FreeIndexs.push_back(object);
	}

	//�ڴ����(trueΪ���ͷ��ڴ�,falseΪ���յ�ַ)
	void GC(bool bEnforce = false)
	{
		CAutoLock lock(&m_lock);
		
		ObjectType* object = nullptr;
		ObjectType* pData = nullptr;

		// ����һ��map��ʹ����m_FreeIndexs���ӿ��һЩ
		std::unordered_map<ObjectType*, bool> findexs;
		{
			for (auto it : m_FreeIndexs)
			{
				findexs.insert(std::make_pair(it, true));
			}
		}

		//�����ڴ�
		std::vector<int> deleteList;
		deleteList.clear();

		bool bCanGC = false;
		
		auto it = m_FreePointerIndexs.begin(), itEnd = m_FreePointerIndexs.end();
		for (; it != itEnd; ++it)
		{
			// �����Ƿ���Ի���[���Լ���ָ���Ƿ�ȫ����m_FreeIndexs����һ���������ѷ�������һ�ݳ�ȥ�����ɻ���]
			bCanGC = true;
			for (int i = 0; i < it->first; ++i)
			{
				pData = it->second + i;
				if (findexs.find(pData) == findexs.end())
				{
					//���bEnforce = false;
					//��ֻ��ƥ������
					if (!bEnforce)
					{
						bCanGC = false;
						break;
					}
					else
					{
						// ǿ�ƻ�����Ҫ���յ�
						object = (ObjectType*)pData;
						object->~ObjectType();
					}
				}
			}
			// ���Ի���
			if (bCanGC)
			{
				//grow()������ӵ�,��ô���ٵ�ʱ��Ӧ�����ȼ���.
				m_growSize /= 2;
				// ���տ�������
				for (int i = 0; i < it->first; ++i)
				{
					pData = it->second + i;
					findexs.erase(pData);
				}
				// ����ָ��
				App_ACEMemory::instance()->free(static_cast<void *>(it->second));
				// ɾ����key
				deleteList.push_back(m_growSize);

				//��¼��ǰ����
				m_CurrentObject -= m_growSize;
			}
		}

		// д�ؿ�������
		m_FreeIndexs.clear();
		for (auto it : findexs)
		{
			m_FreeIndexs.push_back(it.first);
		}
		// ����ɾ��
		size_t size = deleteList.size();
		for (size_t i = 0; i < size; ++i)
		{
			m_FreePointerIndexs.erase(deleteList[i]);
		}
	}

private:
	//���ù̶�����
	void SetFixedLength(uint32 uFixedLength)
	{
		CAutoLock lock(&m_lock);
		m_uFixedLength = uFixedLength;
	}

	uint32 GetFixedLength()
	{
		return m_uFixedLength;
	}

	void Grow()
	{
		//��ǰ���������������,��������.ֱ�ӷ���.
		//�̶������Ѿ�����,�����ٴη����С
		if (isFiexdPool())
		{
			return;
		}
		
		int objectSize = sizeof(ObjectType);

		ObjectType* pData = static_cast<ObjectType*>(App_ACEMemory::instance()->malloc(m_growSize * objectSize));
		if (pData == NULL) 
			return;
		// ����ָ��map��
		m_FreePointerIndexs.insert(std::make_pair(m_growSize, pData));
		// �������������
		for (uint32 i = 0; i < m_growSize; ++i)
		{ 
			m_FreeIndexs.push_back(pData + i);
		}

		//��¼��ǰ����
		m_CurrentObject += m_growSize;
		//���һ������ �´�����һ��
		m_growSize *= 2;
	}

	ObjectType* GetFreePointer()
	{
		if (m_FreeIndexs.empty())
			Grow();
		if (m_FreeIndexs.empty())
			return NULL;
		ObjectType* pData = m_FreeIndexs.back();
		m_FreeIndexs.pop_back();
		return pData;
	}
private:
	FreePointer  m_FreePointerIndexs;// ����ָ������map,keyΪgrowSize
	FreeIndex    m_FreeIndexs;       // ���������б�
	uint32       m_growSize;         // �ڴ������Ĵ�С
	uint32		m_CurrentObject;		//��ǰ�������
	uint32		m_uFixedLength;			//�̶�����
	bool		m_isFixed;				//�̶������ж�trueΪ��
	CThreadLock m_lock;
};

// ����ع���
template<class ObjectType>
class CObjectPool_Factory
{
protected:
	CObjectPool_Factory(){}
public:
	~CObjectPool_Factory(){}

	// ��õ���
	static CObjectPool_Factory& GetSingleton()
	{
		static CObjectPool_Factory poolFactory;
		return poolFactory;
	}

	// ���ObjectPool
	CObjectPool<ObjectType>* GetObjectPool(const std::string& name)
	{
		CAutoLock lock(&m_lock);

		CObjectPool<ObjectType>* pool = nullptr;
		auto it = m_poolMap.find(name);
		if (it == m_poolMap.end())
		{
			pool = new CObjectPool<ObjectType>();
			m_poolMap.insert(std::make_pair(name, pool));
		}
		else
		{
			pool = (CObjectPool<ObjectType>*)it->second;
		}
		return pool;
	}

	// ȫ��gc
	void GC()
	{
		CAutoLock lock(&m_lock);

		for (auto it : m_poolMap)
		{
			it.second->GC(false);
		}
	}

private:
	typedef std::unordered_map<std::string, CObjectPool<ObjectType>* > PoolMap;
	PoolMap m_poolMap;
	CThreadLock m_lock;
};

// �����ָ�붨��
#define DefineObjectPoolPtr(T, pPool) CObjectPool<T>* pPool
// ����ض������ָ�롣
#define GetObjectPoolPtr(T) CObjectPool_Factory<T>::GetSingleton().GetObjectPool(#T)
// ֱ�Ӷ�������
#define ObjectPoolPtr(T, pPool) DefineObjectPoolPtr(T, pPool) = GetObjectPoolPtr(T)