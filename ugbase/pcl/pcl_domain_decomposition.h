/*
 * Copyright (c) 2011-2015:  G-CSC, Goethe University Frankfurt
 * Authors: Sebastian Reiter, Andreas Vogel, Ingo Heppner
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

#ifndef __H__PCL__PCL_DOMAIN_DECOMPOSITION__
#define __H__PCL__PCL_DOMAIN_DECOMPOSITION__

namespace pcl
{

/// \addtogroup pcl
/// \{

class IDomainDecompositionInfo
{

    public:
	/// 	mapping method "proc-id" ==> "subdomain-id"
	/**
	 * This functions determines the subdomain a processor lives in.
	 * \param[out] 	procID		ID of processor.
	 * \return 		int			ID of subdomain the processor operates on.
	 */
		virtual int map_proc_id_to_subdomain_id(int procID) const = 0;

		virtual int get_num_subdomains() const = 0;

		//virtual int get_num_procs_per_subdomain() const = 0;
		virtual int get_num_spatial_dimensions() const = 0;

		virtual void get_subdomain_procs(std::vector<int>& procsOut,
										int subdomIndex) = 0;
	public:
		// destructor
		virtual ~IDomainDecompositionInfo() {};

}; /* end class 'IDomainDecompositionInfo' */

class StandardDomainDecompositionInfo : public IDomainDecompositionInfo
{
	///	constructors
    public:
		StandardDomainDecompositionInfo() :
			m_num_subdomains(1),
			m_num_spatial_dimensions(2),
			m_num_procs_per_subdomain(1)
			{}

		StandardDomainDecompositionInfo(int numSubdomains) :
			m_num_subdomains(numSubdomains),
			m_num_procs_per_subdomain(-1)
			/*m_pDebugWriter(NULL)*/
		{
			if(numSubdomains > 0 && pcl::NumProcs() > 1)
				m_num_procs_per_subdomain = pcl::NumProcs() / numSubdomains;
			else
				m_num_procs_per_subdomain = -1;
		}

    public:
	/// 	mapping method "proc-id" ==> "subdomain-id"
	/**
	 * This functions determines the subdomain a processor lives in.
	 * \param[out] 	procID		ID of processor.
	 * \return 		int			ID of subdomain the processor operates on.
	 */
		int map_proc_id_to_subdomain_id(int procID) const
        {
			return procID / m_num_procs_per_subdomain;
        }

		/// @brief Set the number of subdomains.
		/* Sets the number of subdomains and adjust the processor per subdomain account if applicable.
		*  @param numSubdomains Number of subdomains.
		*/
		void set_num_subdomains(int numSubdomains) {
			m_num_subdomains = numSubdomains;

			if(numSubdomains > 0 && pcl::NumProcs() > 1)
				m_num_procs_per_subdomain = pcl::NumProcs() / numSubdomains;
			else
				m_num_procs_per_subdomain = -1;
		}


		/// @brief Returns the number of subdomains.
		/// @return Number of subdomains.
		int get_num_subdomains() const {return m_num_subdomains;}

		/// @brief Sets the number of spatial dimensions.
		/// @param dim Number of spatial dimensions.
		void set_num_spatial_dimensions(int dim) {
			m_num_spatial_dimensions = dim;
		}

		/// @brief Returns the number of spatial dimensions.
		/// @return Number of spatial dimensions.
		int get_num_spatial_dimensions() const {return m_num_spatial_dimensions;}

		//int get_num_procs_per_subdomain() const {return m_num_procs_per_subdomain;}

		/// @brief Returns a vector consisting of the processors in the specified subdomain.
		/// @param procsOut Vector consisting of the processors in the subdomain.
		/// @param subdomIndex The Index of the domain to be queried.
		virtual void get_subdomain_procs(std::vector<int>& procsOut,
										int subdomIndex)
		{
			procsOut.resize(m_num_procs_per_subdomain);
			int first = subdomIndex * m_num_procs_per_subdomain;
			for(int i = 0; i < m_num_procs_per_subdomain; ++i)
				procsOut[i] = first + i;
		}

	public:
		// destructor
		~StandardDomainDecompositionInfo() {};

	protected:
	// 	Number of subdomains.
		int m_num_subdomains;

	// 	Number of spatial dimensions.
		int m_num_spatial_dimensions;

	// 	Number of procs per subdomain.
		//std::vector<int> m_vnum_procs_per_subdomain; // as vector, for variable distribution of processors over subdomains
		int m_num_procs_per_subdomain;
}; /* end class 'StandardDomainDecompositionInfo' */

// end group pcl
/// \}

}//	end of namespace
#endif
