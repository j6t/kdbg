// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

// an array template class that holds values (not pointers to values)

// need a placement new
#include "config.h"
#ifdef HAVE_PLACEMENT_NEW
#include <new>
#else
inline void* operator new(size_t, void* p) { return p; }
#endif

template<class T>
class ValArray
{
public:
    ValArray() : m_pData(0), m_size(0), m_space(0) { }
    ~ValArray();
    const T& operator[](int i) const { return m_pData[i]; }
    T& operator[](int i) { return m_pData[i]; }
    void setSize(int newSize);
    int size() const { return m_size; }
    void append(const T& newElem, int count = 1) { expand(newElem, m_size+count); }

protected:
    T* m_pData;
    int m_size;
    int m_space;

    void expand(const T& newElem, int newSize);
};

template<class T>
ValArray<T>::~ValArray()
{
    setSize(0);
    delete[] reinterpret_cast<char*>(m_pData);
}

template<class T>
void ValArray<T>::setSize(int newSize)
{
    if (newSize == m_size) return;
    if (newSize > m_size) {
	expand(T(), newSize);
    } else {
	do {
	    m_size--;
	    m_pData[m_size].~T();
	} while (m_size > newSize);
    }
}

template<class T>
void ValArray<T>::expand(const T& newElem, int newSize)
{
    if (newSize > m_space) {
	// reallocate
	int newSpace = m_space + m_space;
	if (newSpace < 8) newSpace = 8;
	if (newSpace < newSize) newSpace = newSize;
	T* newData = reinterpret_cast<T*>(new char[newSpace * sizeof(T)]);
	// care about exception safety as much as possible
	// copy-construct the elements into the new array
	// TODO: clean up when exception is thrown here
	for (int i = 0; i < m_size; i++) {
	    new(&newData[i]) T(m_pData[i]);
	}
	// replace the pointer
	T* oldData = m_pData;
	m_pData = newData;
	m_space = newSpace;
	// destruct the old data
	for (int i = m_size-1; i >= 0; i--) {
	    oldData[i].~T();
	}
	delete[] reinterpret_cast<char*>(oldData);
    }
    // copy the new element into the new space
    while (m_size < newSize) {
	new(&m_pData[m_size]) T(newElem);
	m_size++;
    }
}
