// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

// an array template class that holds values (not pointers to values)

#ifndef VALARRAY_H
#define VALARRAY_H

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
    void operator=(const ValArray<T>& src);
    const T& operator[](int i) const { return m_pData[i]; }
    T& operator[](int i) { return m_pData[i]; }
    void setSize(int newSize);
    int size() const { return m_size; }
    void append(const T& newElem, int count = 1) { expand(newElem, m_size+count); }
    void insertAt(int start, const T& newElem, int count = 1);
    void insertAt(int start, const ValArray<T>& newElems);
    void removeAt(int start, int count = 1);

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
void ValArray<T>::operator=(const ValArray<T>& src)
{
    setSize(src.size());
    for (int i = 0; i < src.size(); i++) {
	m_pData[i] = src.m_pData[i];
    }
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

template<class T>
void ValArray<T>::removeAt(int start, int count)
{
    if (count <= 0)
	return;
    // move elements down
    int nMove = m_size-start-count;
    for (int src = start+count; nMove > 0; src++, start++, nMove--) {
	// first destruct destination
	m_pData[start].~T();
	// copy-construct source to destination
	new(&m_pData[start]) T(m_pData[src]);
    }
    setSize(start);
}

template<class T>
void ValArray<T>::insertAt(int start, const T& newElem, int count)
{
    if (count <= 0)
	return;

    int nMove = m_size - start;
    int src = m_size-1;
    setSize(m_size+count);
    for (int dest = m_size-1; nMove > 0; src--, dest--, nMove--) {
	// first destruct destination
	m_pData[dest].~T();
	// copy-construct source to destination
	new(&m_pData[dest]) T(m_pData[src]);
    }
    // copy new elements (after destruction destination)
    for (; count > 0; start++, count--) {
	m_pData[start].~T();
	new(&m_pData[start]) T(newElem);
    }
}

template<class T>
void ValArray<T>::insertAt(int start, const ValArray<T>& newElems)
{
    int count = newElems.size();
    if (count <= 0)
	return;

    int nMove = m_size - start;
    int src = m_size-1;
    setSize(m_size+count);
    for (int dest = m_size-1; nMove > 0; src--, dest--, nMove--) {
	// first destruct destination
	m_pData[dest].~T();
	// copy-construct source to destination
	new(&m_pData[dest]) T(m_pData[src]);
    }
    // copy new elements (after destruction destination)
    for (int i = 0; count > 0; i++, start++, count--) {
	m_pData[start].~T();
	new(&m_pData[start]) T(newElems[i]);
    }
}

#endif // VALARRAY_H
