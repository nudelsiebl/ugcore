/*
 * mg_dof_manager.h
 *
 *  Created on: 12.07.2010
 *      Author: andreasvogel
 */

#ifndef __H__LIB_DISCRETIZATION__DOF_MANAGER__MG_DOF_MANAGER__
#define __H__LIB_DISCRETIZATION__DOF_MANAGER__MG_DOF_MANAGER__

#include <vector>

#include "lib_grid/lg_base.h"
#include "./function_pattern.h"
#include "lib_discretization/dof_manager/dof_distribution.h"

namespace ug{

/**
 * A MultiGridDoFManager handles the distribution of degrees of freedom on a
 * MultiGrid. It distributes the dof on each grid level and for the surface grid.
 * Thus, it creates num_level + 1 DoFDistributions
 *
 * \tparam 	TDoFDistribution	Type of DoF Distribution
 */
template <typename TDoFDistribution>
class MGDoFManager : public GridObserver
{
	public:
	///	DoF Distribution type
		typedef IDoFDistribution<TDoFDistribution> dof_distribution_type;

	public:
	///	Default Constructor
		MGDoFManager()
			: m_pMGSubsetHandler(NULL), m_pMultiGrid(NULL), m_pSurfaceView(NULL),
				m_pFuncPattern(NULL), m_pSurfDD(NULL)
		{
			m_vLevelDD.clear();
		};

	///	Constructor setting Function Pattern and Multi Grid Subset Handler
		MGDoFManager(MultiGridSubsetHandler& mgsh, FunctionPattern& dp)
			: m_pMGSubsetHandler(NULL), m_pMultiGrid(NULL), m_pSurfaceView(NULL),
				m_pFuncPattern(NULL), m_pSurfDD(NULL)
		{
			m_vLevelDD.clear();
			assign_multi_grid_subset_handler(mgsh);
			assign_function_pattern(dp);
		};

	/// set multi grid subset handler
		bool assign_multi_grid_subset_handler(MultiGridSubsetHandler& mgsh);

	/// set function pattern
		bool assign_function_pattern(FunctionPattern& dp);

	/// number of levels
		size_t num_levels() const
		{
		//	without SubsetHandler, we have no level information
			if(m_pMGSubsetHandler == NULL) return 0;

		//	forward request
			return m_pMGSubsetHandler->num_levels();
		}

	/// distribute dofs on all levels + surface level
		bool enable_dofs();

	/// distribute dofs on all levels
		bool enable_level_dofs();

	///	distribute dofs on surface grid
		bool enable_surface_dofs();

	///	returns if level dofs are enabled
		bool level_dofs_enabled() const {return m_vLevelDD.size() != 0;}

	///	returns if surface dofs are enabled
		bool surface_dofs_enabled() const {return m_pSurfDD != NULL;}

	///	returns Surface DoF Distribution
		dof_distribution_type* get_surface_dof_distribution()
		{
		// 	update surface distribution
			if(!surface_distribution_required())
			{
				UG_LOG("Cannot update surface distribution.\n");
				throw(UGFatalError("Surface DoF Distribution missing but requested."));
			}

			return m_pSurfDD;
		}

	///	returns Surface DoF Distribution
		const dof_distribution_type* get_surface_dof_distribution() const
		{
		// 	update surface distribution
			if(m_pSurfDD == NULL)
			{
				UG_LOG("surface distribution missing.\n");
				throw(UGFatalError("Surface DoF Distribution missing but requested."));
			}

			return m_pSurfDD;
		}

	///	returns Level DoF Distribution
		dof_distribution_type* get_level_dof_distribution(size_t level)
		{
			if(level < m_vLevelDD.size())
				return m_vLevelDD[level];
			else
				return NULL;
		}

	///	returns Level DoF Distribution
		const dof_distribution_type* get_level_dof_distribution(size_t level) const
		{
			if(level < m_vLevelDD.size())
				return m_vLevelDD[level];
			else
				return NULL;
		}

	///	returns the Level DoF Distributions in a vector
		std::vector<const dof_distribution_type*> get_level_dof_distributions() const
		{
			std::vector<const dof_distribution_type*> vLevelDD;
			for(size_t i = 0; i < m_vLevelDD.size(); ++i)
				vLevelDD.push_back(m_vLevelDD[i]);
			return vLevelDD;
		}

	///	returns the surface view
		const SurfaceView* get_surface_view() const {return m_pSurfaceView;}

	///	print a statistic on dof distribution
		void print_statistic() const;

	///	print a statistic on layout informations
		void print_layout_statistic() const;

	///	Destructor
		virtual ~MGDoFManager()
		{
			m_levelStorageManager.clear_subset_handler();
			m_surfaceStorageManager.clear_subset_handler();

			disable_level_dofs();
			disable_surface_dofs();
		}

	public:
	///////////////////////////////////////////////////
	//	GridObserver Callbacks
	///////////////////////////////////////////////////

		virtual void vertex_created(Grid* grid, VertexBase* vrt,
									GeometricObject* pParent = NULL,
									bool replacesParent = false)
		{
		//	we do not check pointer m_pMGSubsetHandler != NULL, since we only
		//	register the callback, if that is the case

		//	get level of Vertex
			const int level = m_pMGSubsetHandler->get_level(vrt);

		//	if level dofs enabled, add vertex to level dofs
			if(level_dofs_enabled())
			{
			//	check if level dof distribution must be created
				if(!level_distribution_required(level+1))
					throw(UGFatalError("Cannot create level dof distribution"));

			//	get level dof distribution
				dof_distribution_type* levDoFDistr =
								get_level_dof_distribution(level);

				UG_ASSERT(levDoFDistr != NULL, "Invalid DoF Distribution.");

			//	announce created vertex to level dof distribution
				levDoFDistr->vertex_added(vrt);
			}

		//	if surface dofs enabled, add vertex to surface dofs
			if(surface_dofs_enabled())
			{
			//	get surface dof distribution
				dof_distribution_type* surfDoFDistr = get_surface_dof_distribution();

			//	1. Release index for parent (which may not be part of surface
			//     view after adding the child; created shadows are not part of
			//	   the surface view at this stage.)
				VertexBase* vrtParent = dynamic_cast<VertexBase*>(pParent);
				if(vrtParent != NULL)
					surfDoFDistr->vertex_to_be_removed(vrtParent);

			//	2. Add the created vertex to the surface view
				const int si = m_pMGSubsetHandler->get_subset_index(vrt);
				m_pSurfaceView->assign_subset(vrt, si);

			//	3. Remove parent from surface view
				// \todo: This is for vertices only, handle all cases
				m_pSurfaceView->assign_subset(vrtParent, -1);

			//	4. Add index for child
				surfDoFDistr->vertex_added(vrt);

			//	NOTE: at this point the parent Vertex may be a shadow. But we
			//		  do not add the shadow here, but on a later call of defragment
			}
		}

		void defragment()
		{
		//	we add the shadows to the surface view, that may have been created
		//	due to grid adaption

		}

	protected:
	///	creates the surface view
		virtual bool surface_view_required();

	///	creates the surface distribution
		bool surface_distribution_required();

	///	creates level DoF Distributions iff needed
		bool level_distribution_required(size_t numLevel);

	///	deletes all level distributions
		void disable_level_dofs();

	///	deletes the surface distributions
		void disable_surface_dofs();

	/// print statistic for a DoFDistribution
		void print_statistic(const dof_distribution_type& dd) const;

	/// print statistic on layouts for a DoFDistribution
		void print_layout_statistic(const dof_distribution_type& dd) const;

	protected:
	// 	MultiGridSubsetHandler this DofManager works on
		MultiGridSubsetHandler* m_pMGSubsetHandler;

	// 	MultiGrid associated to the SubsetHandler
		MultiGrid* m_pMultiGrid;

	// 	Surface View
		SurfaceView* m_pSurfaceView;

	// 	DoF Pattern
		FunctionPattern* m_pFuncPattern;

	// 	Level DoF Distributors
		std::vector<dof_distribution_type*> m_vLevelDD;

	// 	Surface Grid DoF Distributor
		dof_distribution_type* m_pSurfDD;

	// 	Storage manager
		typename TDoFDistribution::storage_manager_type	m_levelStorageManager;
		typename TDoFDistribution::storage_manager_type	m_surfaceStorageManager;
};

} // end namespace ug

// include implementation
#include "mg_dof_manager_impl.h"

#endif /* __H__LIB_DISCRETIZATION__DOF_MANAGER__MG_DOF_MANAGER__ */
