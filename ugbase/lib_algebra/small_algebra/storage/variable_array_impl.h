/*
 * Copyright (c) 2010-2015:  G-CSC, Goethe University Frankfurt
 * Author: Martin Rupp
 * 
 * This file is part of UG4.
 * 
 * UG4 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License version 3 (as published by the
 * Free Software Foundation) with the following additional attribution
 * requirements (according to LGPL/GPL v3 §7):
 * 
 * (1) The following notice must be displayed in the Appropriate Legal Notices
 * of covered and combined works: "Based on UG4 (www.ug4.org/license)".
 * 
 * (2) The following notice must be displayed at a prominent place in the
 * terminal output of covered works: "Based on UG4 (www.ug4.org/license)".
 * 
 * (3) The following bibliography is recommended for citation and must be
 * preserved in all covered files:
 * "Reiter, S., Vogel, A., Heppner, I., Rupp, M., and Wittum, G. A massively
 *   parallel geometric multigrid solver on hierarchically distributed grids.
 *   Computing and visualization in science 16, 4 (2013), 151-164"
 * "Vogel, A., Reiter, S., Rupp, M., Nägel, A., and Wittum, G. UG4 -- a novel
 *   flexible software system for simulating pde based models on high performance
 *   computers. Computing and visualization in science 16, 4 (2013), 165-179"
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */


#ifndef __H__UG__COMMON__VARIABLE_ARRAY_IMPL_H__
#define __H__UG__COMMON__VARIABLE_ARRAY_IMPL_H__

#include "storage.h"
#include "variable_array.h"
#include "common/common.h"
#include <algorithm> // for min
#include <cstring>
#include <type_traits>

namespace ug{

// 'tors
template<typename T>
VariableArray1<T>::VariableArray1()
{
	n = 0;
	values = NULL;
}


template<typename T>
VariableArray1<T>::VariableArray1(size_t n_)
{
	n = 0;
	values = NULL;
	resize(n_, false);
}

template<typename T>
VariableArray1<T>::VariableArray1(const VariableArray1<T> &other)
{
	if(this == &other) return;
	n = 0;
	values = NULL;
	resize(other.size(), false);
	for(size_type i=0; i<n; i++)
		values[i] = other[i];
}

template<typename T>
VariableArray1<T>::VariableArray1(const VariableArray1<T>&& other)
{
	if(this == &other) return;
	n = other.n;
	values = other.values;
	other.values = NULL;
}

template<typename T>
VariableArray1<T>::~VariableArray1()
{
	if(values) { delete[] values; values = NULL; }
	n = 0;
}


// Capacity

template<typename T>
inline size_t
VariableArray1<T>::size() const
{
	return n;
}

template<typename T>
bool
VariableArray1<T>::resize(size_t newN, bool bCopyValues)
{
	if(newN == n) return true;

	if(newN <= 0)
	{
		if(values) delete[] values;
		values = NULL;
		n = 0;
		return true;
	}
	value_type *new_values = new T[newN];
	UG_ASSERT(new_values != NULL, "out of memory");
	if(new_values == NULL) return false;
	if constexpr(std::is_arithmetic<T>::value) {
		memset(new_values, 0, sizeof(T)*newN); // todo: think about that
	}
	else
	{
		if constexpr(std::is_pointer<T>::value) {
			for(size_t i=0; i<n; ++i)
					new_values[i]->set_zero();
		}
		else
		{
			for(size_t i=0; i<n; ++i)
					new_values[i].set_zero();
		}
	}

	if(bCopyValues)
	{
		size_t minN = std::min(n, newN);
		for(size_t i=0; i<minN; i++)
			std::swap(new_values[i], values[i]);
	}

	if(values) delete[] values;
	values = new_values;
	n = newN;
	return true;
}

template<typename T>
inline size_t
VariableArray1<T>::capacity() const
{
	return n;
}


// use stl::vector if you want to use reserve
template<typename T>
inline bool
VariableArray1<T>::reserve(size_t newCapacity) const
{
	return true;
}

// sets the values to zero, multiple versions for non fundamental types
// necessary in c++11
template<typename T>
inline void
VariableArray1<T>::set_zero() {
	if constexpr(std::is_arithmetic<T>::value) {
		memset(values, 0, sizeof(T)*n);
	}
	else
	{
		if constexpr(std::is_pointer<T>::value) {
			for(size_t i=0; i<n; ++i) 
				values[i]->set_zero();
		}
		else
		{
			for(size_t i=0; i<n; ++i) 
				values[i].set_zero();
		}		
	}
}

// Element access

template<typename T>
T &
VariableArray1<T>::operator[](size_t i)
{
	assert(values);
	assert(i<n);
	return values[i];
}

template<typename T>
const T &
VariableArray1<T>::operator[](size_t i) const
{
	assert(values);
	assert(i<n);
	return values[i];
}

template<typename T>
VariableArray1<T> &
VariableArray1<T>::operator=(const VariableArray1<T>& other) {
	UG_ASSERT(other.n==n, "Error: copy of VariableArray1 to another of unequal size.");
	if (&other==this) {
		return *this;
	}
	if (other.n != n) {
		resize(other.n, false);
	}
	for (size_t i=0; i<n; ++i) {
		values[i] = other.values[i];
	}
}

template<typename T>
VariableArray1<T> &
	VariableArray1<T>::operator=(VariableArray1<T>&& other) {
	UG_ASSERT(other.n==n, "Error: assignment of VariableArray1 to another of unequal size.");
	if (&other==this) {
		return *this;
	}
	if (values != NULL) {
		delete [] values;
	}
	n = other.n;
	values = other.values;
	other.n = 0;
	other.values = NULL;
}

template<typename T>
std::ostream &operator << (std::ostream &out, const VariableArray1<T> &arr)
{
	//out << "VariableArray (n=" << arr.size() << ") [ ";
	for(size_t i=0; i<arr.size(); i++)
		out << arr[i] << " ";
	out << "]";
	return out;
}


////////////////////////////////////////////////////////////////////////////////


template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>::VariableArray2() : values(NULL), rows(0), cols(0) 
{
}


template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>::VariableArray2(size_t rows, size_t cols) : values(NULL), rows(0), cols(0)
{
	resize(rows, cols);
}

template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>::VariableArray2(const VariableArray2<T, T_ordering> &other)
{
	if(this == &other) return;
	values = NULL;
	rows = 0;
	cols = 0;
	resize(other.rows, other.cols, false);
	for(size_type i=0; i<rows*cols; i++)
		values[i] = other.values[i];
}

template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>::VariableArray2(VariableArray2<T, T_ordering>&& other)
{
	if(this == &other) return;
	rows = other.tows;
	cols = other.cols;
	values = other.values;
	other.rows = 0;
	other.cols = 0;
	other.values = NULL;
}

template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>::~VariableArray2()
{
	if(values) { delete[] values; values = NULL; }
	rows = cols = 0;
}

// Capacity

template<typename T, eMatrixOrdering T_ordering>
size_t
VariableArray2<T, T_ordering>::num_rows() const
{
	return rows;
}

template<typename T, eMatrixOrdering T_ordering>
size_t
VariableArray2<T, T_ordering>::num_cols() const
{
	return cols;
}


template<typename T, eMatrixOrdering T_ordering>
bool
VariableArray2<T, T_ordering>::resize(size_t newRows, size_t newCols, bool bCopyValues)
{
	if(newRows == rows && newCols == cols) return true;

	if(newRows == 0 && newCols == 0)
	{
		rows = cols = 0;
		if(values) delete[] values;
		values = NULL;
		return true;
	}

	value_type *new_values = new T[newRows*newCols];
	UG_ASSERT(new_values != NULL, "out of memory");
	if(new_values==NULL) return false;
	if constexpr(std::is_arithmetic<T>::value) {
		memset(new_values, 0, sizeof(T)*newRows*newCols);
	}
	else
	{
		if constexpr (std::is_pointer<T>::value) {
			for(size_t i=0; i<newRows*newCols; ++i)
					new_values[i]->set_zero();
		}
		else
		{
			for(size_t i=0; i<newRows*newCols; ++i)
					new_values[i].set_zero();
		}
		// We could only set the entries zero that are not swapped into further down
		// which might be faster for small size changes in very large matrices but 
		// quite inefficient otherwise due prefetch optimizations in the cpu
		//
		// if(T_ordering==RowMajor) {
		//	if (cols < newCols)
		//		for(size_t r=0; r<newRows; ++r)
		//			for(size_t c=cols; c<newCols; ++c)
		//				new_values[c+r*newCols].set_zero();
		//	if (rows < newRows)
		//		for(size_t r=rows; r<newRows; ++r)
		//			for(size_t c=0; c<cols; ++c)
		//				new_values[c+r*newCols].set_zero();
		// }
		// else
		// {
		//	if (cols < newCols)
		//		for(size_t r=0; r<newRows; ++r)
		//			for(size_t c=cols; c<newCols; ++c)
		//				new_values[r+c*newRows].set_zero();
		//	if (rows < newRows)
		//		for(size_t r=rows; r<newRows; ++r)
		//			for(size_t c=0; c<cols; ++c)
		//				new_values[r+c*newRows].set_zero();
		// }
	}
	/*
	if(storage_traits<value_type>::is_static)
	{
		...
	}
	else {

	 */
	if(bCopyValues)
	{
		size_t minRows = std::min(rows, newRows);
		size_t minCols = std::min(cols, newCols);

		// we are using swap to avoid re-allocations
		if(T_ordering==RowMajor)
			for(size_t r=0; r<minRows; r++)
				for(size_t c=0; c<minCols; c++)
					std::swap(new_values[c+r*newCols], values[c+r*cols]);
		else
			for(size_t r=0; r<minRows; r++)
				for(size_t c=0; c<minCols; c++)
					std::swap(new_values[r+c*newRows], values[r+c*rows]);
	}
	// if values where NULL we would not get to this point
	// if(values) delete[] values;
	delete [] values;
	rows = newRows;
	cols = newCols;
	values = new_values;
	return true;
}

template<typename T, eMatrixOrdering T_ordering>
inline void
VariableArray2<T, T_ordering>::set_zero()
{
	if constexpr (std::is_arithmetic<T>::value) {
		memset(values, 0, sizeof(T)*rows*cols);
	}
	else
	{
		if constexpr (std::is_pointer<T>::value) {
			for(size_t i=0; i<rows*cols; ++i)
					values[i]->set_zero();
		}
		else
		{
			for(size_t i=0; i<rows*cols; ++i)
					values[i].set_zero();
		}
	}
}

template<typename T, eMatrixOrdering T_ordering>
T &
VariableArray2<T, T_ordering>::operator()(size_t r, size_t c)
{
	UG_ASSERT(r<rows, "r = " << r << ", rows = " << rows);
	UG_ASSERT(c<cols, "c = " << c << ", cols = " << cols);
	if(T_ordering==RowMajor)
		return values[c+r*cols];
	else
		return values[r+c*rows];
}

template<typename T, eMatrixOrdering T_ordering>
const T &
VariableArray2<T, T_ordering>::operator()(size_t r, size_t c) const
{
	UG_ASSERT(r<rows, "r = " << r << ", rows = " << rows);
	UG_ASSERT(c<cols, "c = " << c << ", cols = " << cols);
	if(T_ordering==RowMajor)
		return values[c+r*cols];
	else
		return values[r+c*rows];
}

template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>&
VariableArray2<T, T_ordering>::operator=(const VariableArray2<T, T_ordering> &other)
{
	if (&other == this) return *this;
	UG_ASSERT((other.rows==rows)&&(other.cols==cols), "Error: copy of matrix to another of unequal size.");
	if (values != NULL) delete [] values;
	values = new T[rows*cols*sizeof(T)];	
	for (size_type i=0; i<rows*cols; ++i)
		values[i] = other.values[i];
}

template<typename T, eMatrixOrdering T_ordering>
VariableArray2<T, T_ordering>&
VariableArray2<T, T_ordering>::operator=(VariableArray2<T, T_ordering> &&other)
{
	if (&other == this) return *this;
	UG_ASSERT((other.rows==rows)&&(other.cols==cols), "Error: assignment of matrix to another of unequal size.");
	if (values != NULL) delete [] values;
	rows = other.rows;
	cols = other.cols;
	values = other.values;
	other.rows = 0;
	other.cols = 0;
	other.values = NULL;
}

template<typename T, eMatrixOrdering T_ordering>
std::ostream &operator << (std::ostream &out, const VariableArray2<T, T_ordering> &arr)
{
	out << "[ ";
	//out << "VariableArray2 (" << arr.num_rows() << "x" << arr.num_cols() << "), " << ((T_ordering == ColMajor) ? "ColMajor" : "RowMajor") << endl;
	typedef size_t size_type;
	for(size_type r=0; r<arr.num_rows(); r++)
	{
		for(size_type c=0; c<arr.num_cols(); c++)
			out << arr(r, c) << " ";
		if(r != arr.num_rows()-1) out << "| ";
	}
	out << "]";
	return out;
}

}
#endif // __H__UG__COMMON__VARIABLE_ARRAY_IMPL_H__
