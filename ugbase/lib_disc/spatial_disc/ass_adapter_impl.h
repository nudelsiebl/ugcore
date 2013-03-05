/*
 * ass_adapter_impl.h
 *
 *  Created on: 01.03.2013
 *      Author:	raphaelprohl
 */

#ifndef ASS_ADAPTER_IMPL_H_
#define ASS_ADAPTER_IMPL_H_

/*#include "lib_grid/tools/bool_marker.h"
#include "lib_grid/tools/selector_grid.h"*/
#include "ass_adapter.h"
//#include "lib_disc/dof_manager/dof_distribution.h"

namespace ug{

template <typename TAlgebra>
void AssAdapter<TAlgebra>::resize(ConstSmartPtr<DoFDistribution> dd, vector_type& vec)
{
	if (m_assIndex.index_set){vec.resize(1);}
	else{
		const size_t numIndex = dd->num_indices();
		vec.resize(numIndex);
	}
	vec.set(0.0);
}

template <typename TAlgebra>
void AssAdapter<TAlgebra>::resize(ConstSmartPtr<DoFDistribution> dd, matrix_type& mat)
{
	mat.resize(0,0);
	if (m_assIndex.index_set){
		mat.resize(1, 1);
	}
	else{
		const size_t numIndex = dd->num_indices();
		mat.resize(numIndex, numIndex);
	}
}

template <typename TAlgebra>
template <typename TElem>
void AssAdapter<TAlgebra>::elemIter_fromSel(ConstSmartPtr<DoFDistribution> dd,
    	int si, std::vector<TElem*>& elems)
{
	if (!m_pSelector)
		UG_THROW("Selector-iterator not set!")

	Selector* sel = m_pSelector;
	const ISubsetHandler& sh = *dd->subset_handler();

	for(typename Selector::traits<TElem>::iterator iter = sel->begin<TElem>();
		iter != sel->end<TElem>(); ++iter)
	{
		if(sh.get_subset_index(*iter) == si)
			elems.push_back(*iter);
	}
}

/*template <typename TAlgebra>
template <typename TDomain>
void AssAdapter<TAlgebra>:: adaptConstraint(IDomainConstraint<TDomain, TAlgebra>& constraint)
{
	if(m_assIndex.index_set) constraint->set_ass_index(m_assIndex.index);
		else constraint->set_ass_index();
}*/



} // end namespace ug

#endif /* ASS_ADAPTER_IMPL_H_ */