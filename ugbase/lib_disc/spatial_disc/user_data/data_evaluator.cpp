/*
 * data_evaluator_impl.h
 *
 *  Created on: 03.07.2011
 *      Author: andreasvogel
 */

#include <sstream>

#include "data_evaluator.h"
#include "lib_disc/common/groups_util.h"

namespace ug{

///////////////////////////////////////////////////////////////////////////////
// DataEvaluator Setup
///////////////////////////////////////////////////////////////////////////////

template <typename TDomain>
DataEvaluator<TDomain>::DataEvaluator(int discPart,
                             const std::vector<IElemDisc<TDomain>*>& vElemDisc,
                             const FunctionPattern& fctPat,
                             const int subset,
                             const bool bNonRegularGrid,
                             LocalVectorTimeSeries* pLocTimeSeries,
                             const std::vector<number>* pvScaleMass,
                             const std::vector<number>* pvScaleStiff)
                             : m_fctPatt(fctPat)
{
//	remember needed disc parts
	m_discPart = discPart;

//	currently only fast assembles allowed
	for(size_t i = 0; i < vElemDisc.size(); ++i){
		if(!vElemDisc[i]->fast_add_elem_enabled()){
			UG_THROW("DataEvaluator: currently only fast assemble allowed."
					 " Please use enable_fast_add_elem in all IElemDisc.");
		}
	}

// handle time dependency
	if(pLocTimeSeries != NULL && pvScaleMass != NULL && pvScaleStiff != NULL){
		for(size_t i = 0; i < vElemDisc.size(); ++i)
			vElemDisc[i]->set_time_dependent(*pLocTimeSeries, *pvScaleMass, *pvScaleStiff);
	}
	else if(pLocTimeSeries != NULL){
		for(size_t i = 0; i < vElemDisc.size(); ++i)
			vElemDisc[i]->set_time_dependent(*pLocTimeSeries, std::vector<number>(), std::vector<number>());
	}
	else{
		for(size_t i = 0; i < vElemDisc.size(); ++i)
			vElemDisc[i]->set_time_independent();
	}

// remember time series
	m_pLocTimeSeries = pLocTimeSeries;
	m_bNeedLocTimeSeries = false; // initially
	m_bUseHanging = false; // initially
	m_subset = subset;

	m_vElemDisc[PT_ALL] = vElemDisc;

//	create FunctionIndexMapping for each Disc
	for(size_t i = 0; i < m_vElemDisc[PT_ALL].size(); ++i)
	{
	//	get elem disc
		IElemDisc<TDomain>* disc = m_vElemDisc[PT_ALL][i];

	// checks
		disc->check_setup();

	//	check if time dependent
		m_bNeedLocTimeSeries |= disc->local_time_series_needed();

	//  let disc use non-regular grid assemblings
		if(!disc->request_non_regular_grid(bNonRegularGrid))
		{
			UG_THROW("DataEvaluator<TDomain>::set_non_regular_grid: "
					" Elem Disc " << i << " does not support non-regular"
					" grids, but this is requested.\n");
		}

		if(bNonRegularGrid)
			m_bUseHanging |= disc->use_hanging();

		ProcessType process;
		if(disc->is_stationary()) process = PT_STATIONARY;
		else process = PT_INSTATIONARY;
		m_vElemDisc[process].push_back(disc);
	}
}

template <typename TDomain>
void DataEvaluator<TDomain>::clear_extracted_data_and_mappings()
{
	for(int type = 0; type < MAX_PROCESS; ++type){
		m_vImport[type][MASS].clear();
		m_vImport[type][STIFF].clear();
		m_vImport[type][RHS].clear();
	}

	m_vConstData.clear();
	m_vPosData.clear();
	m_vDependentData.clear();
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_data_to_eval_data(std::vector<SmartPtr<ICplUserData<dim> > >& vEvalData,
												   std::vector<SmartPtr<ICplUserData<dim> > >& vTryingToAdd)
{
//	if empty, we're done
	if(vTryingToAdd.empty()) return;

//	search for element in already scheduled data
	typename std::vector<SmartPtr<ICplUserData<dim> > >::iterator it, itEnd;
	it = find(vEvalData.begin(), vEvalData.end(), vTryingToAdd.back());

//	if found, skip this data
	if(it != vEvalData.end())
	{
		vTryingToAdd.pop_back();
		return;
	}

//	search if element already contained in list. Then, the element
//	did start the adding procedure before and a circle dependency
//	is found
	itEnd = vTryingToAdd.end(); itEnd--;
	it = find(vTryingToAdd.begin(), itEnd, *itEnd);

//	if found, return error of circle dependency
	if(it != itEnd)
		UG_THROW("DataEvaluator<TDomain>::add_data_to_eval_data:"
						" Circle dependency of data detected for UserData.");

//	add all dependent datas
	SmartPtr<ICplUserData<dim> > data = vTryingToAdd.back();
	for(size_t i = 0; i < data->num_needed_data(); ++i)
	{
	//	add each data separately
		vTryingToAdd.push_back(data->needed_data(i));
		add_data_to_eval_data(vEvalData, vTryingToAdd);
	}

//	add this data to the evaluation list
	vEvalData.push_back(data);

//	pop last one, since now added to eval list
	if(!vTryingToAdd.empty())
		vTryingToAdd.pop_back();
}

template <typename TDomain>
void DataEvaluator<TDomain>::extract_imports_and_userdata(int discPart)
{
	clear_extracted_data_and_mappings();

//	queue for all user data needed
	std::vector<SmartPtr<ICplUserData<dim> > > vEvalData;
	std::vector<SmartPtr<ICplUserData<dim> > > vTryingToAdd;

//	In the next loop we extract all need UserData:
//	We only process the DataImport if there has been set data to the import
//	since otherwise no evaluation is needed.
//	If there is data given, we get the connected UserData and add it to the vector
//	of EvaluationData. This simply adds the UserData to the queue for UserData, if
//	the data does not depend on other Data. But if the UserData itself has
//	dependencies to other UserData, this data is added first (in a recursive
//	process). Of coarse, no circle dependency between UserData is allowed.

//	In the same loop over the data imports, we schedule the DataImports for
//	evaluation and compute the correct FunctionMapping for the linearization
//	of the defect and the Data, the Import is connected to:
//	If the UserData does not depend on the primary unknowns, we're done. Else
//	we have to setup the Function mappings between the common function group
//	and the DataImport-FunctionGroup. This is simply the same function map as
//	for the element discretization, since the DataImport depends by definition
//	from and only from the primary variables of its associated IElemDisc.

//	loop elem discs
	for(size_t d = 0; d < m_vElemDisc[PT_ALL].size(); ++d)
	{
		IElemDisc<TDomain>* disc = m_vElemDisc[PT_ALL][d];

	//	loop imports
		for(size_t i = 0; i < disc->num_imports(); ++i)
		{
		//	get import
			IDataImport<dim>* iimp = &(disc->get_import(i));

		//	skip non-given data (no need for evaluation)
			if(!iimp->data_given()) continue;

		//	check part
			if( !(iimp->part() & discPart) ) continue;

		//	check correct process type
			if(iimp->part() == MASS)
				if(disc->is_stationary()) continue;

		//	push export on stack of needed data
			vTryingToAdd.push_back(iimp->data());

		//	add data and all dependency to evaluation list
			try{
				add_data_to_eval_data(vEvalData, vTryingToAdd);
			}
			UG_CATCH_THROW("DataEvaluator:"
						" Circle dependency of data detected for UserData.");

		//	check that queue is empty now, else some internal error occured
			if(!vTryingToAdd.empty())
				UG_THROW("DataEvaluator:"
						" Internal Error, UserData queue not empty after adding.");

		//	done if and only if zero-derivative
			if(iimp->zero_derivative()) continue;

		//	remember Import
			ProcessType process;
			if(disc->is_stationary()) process = PT_STATIONARY;
			else process = PT_INSTATIONARY;

			m_vImport[PT_ALL][iimp->part()].push_back(iimp);
			m_vImport[process][iimp->part()].push_back(iimp);
		}
	}

//	Now, we have processed all imports, that must be evaluated and have a long
//	vector of UserData that is connected to those imports. The UserData is already
//	sorted in this way: Data that depends on other data appears after the data
//	it depends on. This is important since we will schedule now the data for
//	evaluation and the data, that is needed by other data, will be computed
//	first. In addition, the data linker have to update their FunctionGroup and
//	must be sure that the data they depend on has already a correct FunctionGroup
//	set. This all is ensured by the (already produced) correct ordering.
//
//	In the next loop we process all UserData, that will be evaluated during
//	assembling (i.e. is connected to an Import). First, we check if the data
//	is constant. If so simply add it the the Constant Data vector; nothing more
//	has to be done here. Else we check if the data depends on the primary
//	unknowns. If this is not the case, the UserData must be a Position-dependent
//	data, but not constant. Thus, schedule it at the Position Data vector.
//	If the data depends on the primary unknowns we must proceed as follows:
//	First, we update the FunctionGroup of the Data, since it could be a linker
//	and having an incorrect FunctionGroup (iff the FunctionGroup of the data
//	the linker depends on has been changed). Then we create the function
//	mapping between the functions the linker depends on and the common Function
//	Group.

//	loop all needed user data and group it
	for(size_t i = 0; i < vEvalData.size(); ++i)
	{
	//	get the user data
		SmartPtr<ICplUserData<dim> > ipData = vEvalData[i];

	//	sort data into const and non-solution dependent
		if(ipData->constant()) {m_vConstData.push_back(ipData); continue;}
		if(ipData->zero_derivative()){m_vPosData.push_back(ipData); continue;}

	//	update function group of dependent data
		try{ipData->update_function_group_and_map();}
		UG_CATCH_THROW("DataEvaluator: Cannot update FunctionGroup of IDependentData.");

	//	save as dependent data
		m_vDependentData.push_back(ipData);
	}

// 	Handle time dependency
// 	NOTE: constant data is not processed.
	if(m_pLocTimeSeries != NULL){
		for(size_t i = 0; i < m_vPosData.size(); ++i)
			m_vPosData[i]->set_times(m_pLocTimeSeries->times());
		for(size_t i = 0; i < m_vDependentData.size(); ++i)
			m_vDependentData[i]->set_times(m_pLocTimeSeries->times());
	}

// 	NOTE: constant data is not processed, since constant == independent of si
	for(size_t i = 0; i < m_vPosData.size(); ++i)
		m_vPosData[i]->set_subset(m_subset);
	for(size_t i = 0; i < m_vDependentData.size(); ++i)
		m_vDependentData[i]->set_subset(m_subset);
}


template <typename TDomain>
void DataEvaluator<TDomain>::set_time_point(const size_t timePoint)
{
	for(size_t i = 0; i < m_vElemDisc[PT_ALL].size(); ++i)
		m_vElemDisc[PT_ALL][i]->set_time_point(timePoint);

	// NOTE: constant data is not processed.
	for(size_t i = 0; i < m_vPosData.size(); ++i)
		m_vPosData[i]->set_time_point(timePoint);
	for(size_t i = 0; i < m_vDependentData.size(); ++i)
		m_vDependentData[i]->set_time_point(timePoint);
}

///////////////////////////////////////////////////////////////////////////////
// Assemble routines
///////////////////////////////////////////////////////////////////////////////

template <typename TDomain>
void DataEvaluator<TDomain>::compute_elem_data(LocalVector& u, GeometricObject* elem,
                                               const MathVector<dim> vCornerCoords[], bool bDeriv)
{
//	evaluate position data
	for(size_t i = 0; i < m_vPosData.size(); ++i)
		m_vPosData[i]->compute(&u, elem, NULL, false);

// 	process dependent data:
//	We can not simply compute exports first, then Linker, because an export
//	itself could depend on other data if implemented somehow in the IElemDisc
//	(e.g. using data from some DataImport). Thus, we have to loop the sorted
//	vector of all dependent data (that is correctly sorted the way that always
//	needed data has previously computed). We look up, if a Export is given, if
//	so compute it, else compute the linker

//	loop all dependent data
	for(size_t i = 0; i < m_vDependentData.size(); ++i){
	//	compute the data
		try{
			u.access_by_map(m_vDependentData[i]->map());
			m_vDependentData[i]->compute(&u, elem, vCornerCoords, bDeriv);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::compute_elem_data:"
						"Cannot compute data for Export " << i);
	}
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_JA_elem(LocalMatrix& A, LocalVector& u, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & STIFF, "Using add_JA_elem, but not STIFF requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		u.access_by_map(map);
		A.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble JA
		try{
			m_vElemDisc[type][i]->fast_add_jac_A_elem(A, u);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_jac_A_elem: "
						"Cannot assemble Jacobian (A) for IElemDisc "<<i);
	}

	add_coupl_JA(A, u, type);
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_JM_elem(LocalMatrix& M, LocalVector& u, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & MASS, "Using add_JM_elem, but not MASS requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		u.access_by_map(map);
		M.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble JM
		try{
			if(!m_vElemDisc[type][i]->is_stationary())
				m_vElemDisc[type][i]->fast_add_jac_M_elem(M, u);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_jac_M_elem: "
						"Cannot assemble Jacobian (M) for IElemDisc "<<i);
	}

	add_coupl_JM(M, u, type);
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_dA_elem(LocalVector& d, LocalVector& u, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & STIFF, "Using add_dA_elem, but not STIFF requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		u.access_by_map(map);
		d.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble dA
		try{
			m_vElemDisc[type][i]->fast_add_def_A_elem(d, u);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_def_A_elem: "
						"Cannot assemble Defect (A) for IElemDisc "<<i);
	}
}

/////////////////////////////////////////////////////////////////////////////

// explicit terms for reaction, reaction_rate, source explicit

template <typename TDomain>
void DataEvaluator<TDomain>::add_dA_elem_explicit(LocalVector& d, LocalVector& u, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & EXPL, "Using add_dA_elem, but not EXPL requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		u.access_by_map(map);
		d.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble dA
		try{
			m_vElemDisc[type][i]->fast_add_def_A_elem_explicit(d, u);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_def_A_elem_explicit: "
						"Cannot assemble Defect (A) for IElemDisc "<<i);
	}
}

/////////////////////////////////////////////////////////////////////////////

template <typename TDomain>
void DataEvaluator<TDomain>::add_dM_elem(LocalVector& d, LocalVector& u, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & MASS, "Using add_dM_elem, but not MASS requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		u.access_by_map(map);
		d.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble dM
		try{
			if(!m_vElemDisc[type][i]->is_stationary())
				m_vElemDisc[type][i]->fast_add_def_M_elem(d, u);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_def_M_elem: "
						"Cannot assemble Defect (M) for IElemDisc "<<i);
	}
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_rhs_elem(LocalVector& rhs, GeometricObject* elem, ProcessType type)
{
	UG_ASSERT(m_discPart & RHS, "Using add_rhs_elem, but not RHS requested.")

	for(size_t i = 0; i < m_vElemDisc[type].size(); ++i)
	{
	//	get map
		const FunctionIndexMapping& map = m_vElemDisc[type][i]->map();

	//	access disc functions
		rhs.access_by_map(map);

		if(m_vElemDisc[type][i]->local_time_series_needed())
			for(size_t t=0; t < m_pLocTimeSeries->size(); ++t)
				m_pLocTimeSeries->solution(t).access_by_map(map);

	//	assemble rhs
		try{
			m_vElemDisc[type][i]->fast_add_rhs_elem(rhs);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_rhs_elem: "
						"Cannot assemble rhs for IElemDisc "<<i);
	}
}

template <typename TDomain>
void DataEvaluator<TDomain>::finish_elem_loop()
{
	for(size_t i = 0; i < m_vElemDisc[PT_ALL].size(); ++i)
	{
		try{
			m_vElemDisc[PT_ALL][i]->fast_fsh_elem_loop();
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::fsh_elem_loop: "
						"Cannot finish element loop for IElemDisc "<<i);
	}

	clear_positions_in_user_data();
}

template <typename TDomain>
void DataEvaluator<TDomain>::clear_positions_in_user_data()
{
//	remove ip series for all used UserData
	for(size_t i = 0; i < m_vConstData.size(); ++i) m_vConstData[i]->clear();
	for(size_t i = 0; i < m_vPosData.size(); ++i)   m_vPosData[i]->clear();
	for(size_t i = 0; i < m_vDependentData.size(); ++i) m_vDependentData[i]->clear();

//	we remove all ips, since they may have been set in prepare_elem
	for(size_t d = 0; d < m_vElemDisc[PT_ALL].size(); ++d)
		for(size_t i = 0; i < m_vElemDisc[PT_ALL][d]->num_imports(); ++i)
			m_vElemDisc[PT_ALL][d]->get_import(i).clear_ips();
}

///////////////////////////////////////////////////////////////////////////////
// Coupling
///////////////////////////////////////////////////////////////////////////////

template <typename TDomain>
void DataEvaluator<TDomain>::add_coupl_JA(LocalMatrix& J, LocalVector& u, ProcessType type)
{
//	compute linearized defect
	for(size_t i = 0; i < m_vImport[type][STIFF].size(); ++i)
	{
	//	set correct access for import
		u.access_by_map(m_vImport[type][STIFF][i]->map());

	//	compute linearization of defect
		try{m_vImport[type][STIFF][i]->compute_lin_defect(u);
		}UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JA: Cannot compute"
						" linearized defect for Import " << i <<" (Stiffness part).");
	}
//	compute linearized defect
	for(size_t i = 0; i < m_vImport[type][RHS].size(); ++i)
	{
	//	set correct access for import
		u.access_by_map(m_vImport[type][RHS][i]->map());

	//	compute linearization of defect
		try{m_vImport[type][RHS][i]->compute_lin_defect(u);
		}UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JA: Cannot compute"
						" linearized defect for Import " << i <<" (Rhs part).");
	}

//	loop all imports located in the stiffness part
	for(size_t i = 0; i < m_vImport[type][STIFF].size(); ++i)
	{
	//	rows are given by import, cols are given by connected data
		J.access_by_map(m_vImport[type][STIFF][i]->map(), m_vImport[type][STIFF][i]->conn_map());

	//	add off diagonal coupling
		try{m_vImport[type][STIFF][i]->add_jacobian(J, 1.0);}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JA: Cannot add couplings.");
	}

//	loop all imports located in the rhs part
	for(size_t i = 0; i < m_vImport[type][RHS].size(); ++i)
	{
	//	rows are given by import, cols are given by connected data
		J.access_by_map(m_vImport[type][RHS][i]->map(), m_vImport[type][RHS][i]->conn_map());

	//	add off diagonal coupling
		try{m_vImport[type][RHS][i]->add_jacobian(J, -1.0);}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JA: Cannot add couplings.");
	}
}

template <typename TDomain>
void DataEvaluator<TDomain>::add_coupl_JM(LocalMatrix& J, LocalVector& u, ProcessType type)
{
//	compute linearized defect
	for(size_t i = 0; i < m_vImport[type][MASS].size(); ++i)
	{
	//	set correct access for import
		u.access_by_map(m_vImport[type][MASS][i]->map());

	//	compute linearization of defect
		try{m_vImport[type][MASS][i]->compute_lin_defect(u);
		}UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JM: Cannot compute"
						" linearized defect for Import " << i <<" (Mass part).");
	}

//	loop all imports located in the mass part
	for(size_t i = 0; i < m_vImport[type][MASS].size(); ++i)
	{
	//	rows are given by import, cols are given by connected data
		J.access_by_map(m_vImport[type][MASS][i]->map(), m_vImport[type][MASS][i]->conn_map());

	//	add off diagonal coupling
		try{
			m_vImport[type][MASS][i]->add_jacobian(J, 1.0);
		}
		UG_CATCH_THROW("DataEvaluator<TDomain>::add_coupl_JM: Cannot add couplings.");
	}
}

////////////////////////////////////////////////////////////////////////////////
//	explicit template instantiations
////////////////////////////////////////////////////////////////////////////////

#ifdef UG_DIM_1
template class DataEvaluator<Domain1d>;
#endif
#ifdef UG_DIM_2
template class DataEvaluator<Domain2d>;
#endif
#ifdef UG_DIM_3
template class DataEvaluator<Domain3d>;
#endif

} // end namespace ug

