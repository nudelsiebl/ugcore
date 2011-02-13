----------------------------------------------------------
--
--   Lua - Script to test FETI on the Laplace-Problem
--
--   Author: Andreas Vogel
--
----------------------------------------------------------
verbosity = 0	-- set to 0 i.e. for time measurements,
		-- >= 1 for writing matrix files etc.

-- make sure that ug_util is in the right path.
-- currently only the path in which you start your application is valid.
ug_load_script("ug_util.lua")

-- choose algebra
InitAlgebra(CPUAlgebraChooser());

-- constants
dim = 2

if dim == 2 then
	gridName = "unit_square_tri.ugx"
	--gridName = "unit_square_quads_8x8.ugx"
end
if dim == 3 then
	gridName = "unit_cube_hex.ugx"
	--gridName = "unit_cube_tets_regular.ugx"
end

numPreRefs = 2
numRefs = 4

numPreRefs = GetParam("-numPreRefs", 2)+0
numRefs    = GetParam("-numRefs",    4)+0 -- '+0' to get a number instead of a string!

--------------------------------
-- User Data Functions (begin)
--------------------------------
	function ourDiffTensor2d(x, y, t)
		return	1, 0, 
				0, 1
	end
	
	function ourVelocityField2d(x, y, t)
		return	0, 0
	end
	
	function ourReaction2d(x, y, t)
		return	0
	end
	
	function ourRhs2d(x, y, t)
		local s = 2*math.pi
		return	s*s*(math.sin(s*x) + math.sin(s*y))
		--return -2*y
		--return 0;
		--return -2*((x*x - 1)+(y*y - 1))
		--return	2*s*s*(math.sin(s*x) * math.sin(s*y))
	end
	
	function ourNeumannBnd2d(x, y, t)
		--local s = 2*math.pi
		--return -s*math.cos(s*x)
		return true, -2*x*y
	end
	
	function ourDirichletBnd2d(x, y, t)
		local s = 2*math.pi
		return true, math.sin(s*x) + math.sin(s*y)
		--return true, x +1
		--return true, 2.5
		--return true, (x*x - 1)*(y*y - 1)
	 	--return true, math.sin(s*x)*math.sin(s*y)
	end

	function ourDiffTensor3d(x, y, z, t)
		return	1, 0, 0,
				0, 1, 0,
				0, 0, 1
	end
	
	function ourVelocityField3d(x, y, z, t)
		return	0, 0, 0
	end
	
	function ourReaction3d(x, y, z, t)
		return	0
	end
	
	function ourRhs3d(x, y, z, t)
		--local s = 2*math.pi
		--return	s*s*(math.sin(s*x) + math.sin(s*y) + math.sin(s*z))
		return 0;
	end
	
	function ourNeumannBnd3d(x, y, t)
		--local s = 2*math.pi
		--return -s*math.cos(s*x)
		return true, -2*x*y*z
	end
	
	function ourDirichletBnd3d(x, y, z, t)
		--local s = 2*math.pi
		--return true, math.sin(s*x) + math.sin(s*y) + math.sin(s*z)
		return true, x
	end
--------------------------------
-- User Data Functions (end)
--------------------------------

-- create Instance of a Domain
print("Create Domain.")
dom = utilCreateDomain(dim)

-- load domain
print("Load Domain from File.")
if utilLoadDomain(dom, gridName) == false then
   print("Loading Domain failed.")
   exit()
end

-- create Refiner
print("Create Refiner")
if numPreRefs > numRefs then
	print("numPreRefs must be smaller/equal than numRefs");
	exit();
end

refiner = GlobalMultiGridRefiner()
refiner:assign_grid(dom:get_grid())
for i=1,numPreRefs do
	refiner:refine()
end

if utilDistributeDomain(dom) == false then
	print("Error while Distributing Grid.")
	exit()
end

print("Refine Parallel Grid")
for i=numPreRefs+1,numRefs do
	utilGlobalRefineParallelDomain(dom)
end

-- get subset handler
sh = dom:get_subset_handler()
if sh:num_subsets() ~= 2 then 
	print("Domain must have 2 Subsets for this problem.")
	exit()
end
sh:set_subset_name("Inner", 0)
sh:set_subset_name("DirichletBoundary", 1)
--sh:set_subset_name("NeumannBoundary", 2)

-- write grid to file for test purpose
if verbosity >= 1 then
	utilSaveDomain(dom, "refined_grid.ugx")
end

-- create function pattern
print("Create Function Pattern")
pattern = P1ConformFunctionPattern()
pattern:set_subset_handler(sh)
AddP1Function(pattern, "c", dim)
pattern:lock()

-- create Approximation Space
print("Create ApproximationSpace")
approxSpace = utilCreateApproximationSpaceWithoutInit(dom, pattern)

--------------------------------------------------------------------------------
-- Gather info for domain decomposition
--------------------------------------------------------------------------------
-- get number of processes
numProcs = NumProcesses()
if numProcs < 2 then
	print("number of processes is smaller than 2 - huh??")
end

-- get number of processes per subdomain
numProcsPerSubdomain = GetParam("-nPPSD", 1)+0 -- '+0' to get a number instead of a string!

if not isPowerOfTwo(numProcsPerSubdomain) then
	print( "WARNING: nPPSD = '" .. numProcsPerSubdomain .. "' is not a power of 2!" )
--	return
end

print( "Check if nPPSD = '" .. numProcsPerSubdomain .. "' process(es) per subdomain makes sense ..." )

-- compute number of subdomains
numSubdomains = numProcs / numProcsPerSubdomain

-- check that numSubdomains is greater 1 && \in \N && a power of 2.
if numSubdomains < 2 then
	print( "ERROR:   numSubdomains = numProcs / numProcsPerSubdomain = '" .. numSubdomains .. "' is smaller than 2!? Aborting!" )
	return
end

if not isNaturalNumber(numSubdomains) then
	print( "ERROR:   numSubdomains = numProcs / numProcsPerSubdomain = '" .. numSubdomains .. "' is NOT a natural number!? Aborting!" )
	return
end

if not isPowerOfTwo(numSubdomains) then
	print( "WARNING: numSubdomains = numProcs / numProcsPerSubdomain = '" .. numSubdomains .. "' is not a power of 2! Continuing ..." )
-- TODO: Maybe switch to a default value then
--	return -- in this case the partition can be quite erratic (at least on small (triangular) grids)..
end

print( "NumProcs is " .. numProcs .. ", NumSubDomains is " .. numSubdomains )
--------------------------------------------------------------------------------

-- create subdomain info
print("Create domainDecompInfo")
domainDecompInfo = StandardDomainDecompositionInfo()
domainDecompInfo:set_num_subdomains(numSubdomains)

approxSpace:init()
approxSpace:print_statistic()
approxSpace:print_layout_statistic()

-------------------------------------------
--  Setup User Functions
-------------------------------------------
print ("Setting up Assembling")

-- Diffusion Tensor setup
	if dim == 2 then
		diffusionMatrix = utilCreateLuaUserMatrix("ourDiffTensor2d", dim)
	elseif dim == 3 then
		diffusionMatrix = utilCreateLuaUserMatrix("ourDiffTensor3d", dim)
	end
	--diffusionMatrix = utilCreateConstDiagUserMatrix(1.0, dim)

-- Velocity Field setup
	if dim == 2 then
		velocityField = utilCreateLuaUserVector("ourVelocityField2d", dim)
	elseif dim == 3 then
		velocityField = utilCreateLuaUserVector("ourVelocityField3d", dim)
	end 
	--velocityField = utilCreateConstUserVector(0.0, dim)

-- Reaction setup
	if dim == 2 then
		reaction = utilCreateLuaUserNumber("ourReaction2d", dim)
	elseif dim == 3 then
		reaction = utilCreateLuaUserNumber("ourReaction3d", dim)
	end
	--reaction = utilCreateConstUserNumber(0.0, dim)

-- rhs setup
	if dim == 2 then
		rhs = utilCreateLuaUserNumber("ourRhs2d", dim)
	elseif dim == 3 then
		rhs = utilCreateLuaUserNumber("ourRhs3d", dim)
	end
	--rhs = utilCreateConstUserNumber(0.0, dim)

-- neumann setup
	if dim == 2 then
		neumann = utilCreateLuaBoundaryNumber("ourNeumannBnd2d", dim)
	elseif dim == 3 then
		neumann = utilCreateLuaBoundaryNumber("ourNeumannBnd3d", dim)
	end
	--neumann = utilCreateConstUserNumber(0.0, dim)

-- dirichlet setup
	if dim == 2 then
		dirichlet = utilCreateLuaBoundaryNumber("ourDirichletBnd2d", dim)
	elseif dim == 3 then
		dirichlet = utilCreateLuaBoundaryNumber("ourDirichletBnd3d", dim)
	end
	--dirichlet = utilCreateConstBoundaryNumber(3.2, dim)
	
-----------------------------------------------------------------
--  Setup FV Convection-Diffusion Element Discretization
-----------------------------------------------------------------

elemDisc = utilCreateFV1ConvDiff(approxSpace, "c", "Inner")
elemDisc:set_upwind_amount(0.0)
elemDisc:set_diffusion_tensor(diffusionMatrix)
elemDisc:set_velocity_field(velocityField)
elemDisc:set_reaction(reaction)
elemDisc:set_rhs(rhs)

-----------------------------------------------------------------
--  Setup Neumann Boundary
-----------------------------------------------------------------

--neumannDisc = utilCreateNeumannBoundary(approxSpace, "Inner")
--neumannDisc:add_boundary_value(neumann, "c", "NeumannBoundary")

-----------------------------------------------------------------
--  Setup Dirichlet Boundary
-----------------------------------------------------------------

dirichletBND = utilCreateDirichletBoundary(approxSpace)
dirichletBND:add_boundary_value(dirichlet, "c", "DirichletBoundary")

-------------------------------------------
--  Setup Domain Discretization
-------------------------------------------

domainDisc = DomainDiscretization()
domainDisc:add_elem_disc(elemDisc)
--domainDisc:add_elem_disc(neumannDisc)
domainDisc:add_post_process(dirichletBND)

-------------------------------------------
--  Algebra
-------------------------------------------
print ("Setting up Algebra Solver")

-- create operator from discretization
linOp = AssembledLinearOperator()
linOp:export_rhs(true)
linOp:set_discretization(domainDisc)
linOp:set_dof_distribution(approxSpace:get_surface_dof_distribution())

-- get grid function
u = approxSpace:create_surface_function("u", true)
b = approxSpace:create_surface_function("b", true)

-- New creation of subdomains and layouts (since 30012011):
-- test one to many interface creation
if verbosity >= 1 then
	for i=0,NumProcesses()-1 do
		print("subdom of proc " .. i .. ": " .. domainDecompInfo:map_proc_id_to_subdomain_id(i))
	end
end

-- BuildDomainDecompositionLayoutsTest2d(u, domainDecompInfo);
-- OneToManyTests2d(u)


-- set initial value
u:set(0.0)

-- init Operator
print ("Assemble Operator ... ")
linOp:init()
print ("done")

-- set dirichlet values in start iterate
linOp:set_dirichlet_values(u)
b:assign(linOp:get_rhs())

-- write matrix for test purpose
if verbosity >= 1 then
	SaveMatrixForConnectionViewer(u, linOp, "Stiffness.mat")
	SaveVectorForConnectionViewer(b, "Rhs.mat")
end

-- debug writer
dbgWriter = utilCreateGridFunctionDebugWriter(dim)
dbgWriter:set_reference_grid_function(u)
dbgWriter:set_vtk_output(false)

-- create algebraic Preconditioner
jac = Jacobi()
jac:set_damp(0.8)
jac2 = Jacobi()
jac2:set_damp(0.8)
jac3 = Jacobi()
jac3:set_damp(0.8)
gs = GaussSeidel()
sgs = SymmetricGaussSeidel()
bgs = BackwardGaussSeidel()
ilu = ILU()
ilu2 = ILU()
ilu3 = ILU()
ilut = ILUT()

-- create GMG ---
-----------------

	-- Base Solver
	baseConvCheck = StandardConvergenceCheck()
	baseConvCheck:set_maximum_steps(500)
	baseConvCheck:set_minimum_defect(1e-8)
	baseConvCheck:set_reduction(1e-30)
	baseConvCheck:set_verbose_level(false)
	-- base = LapackLUSolver()
	base = LinearSolver()
	base:set_convergence_check(baseConvCheck)
	base:set_preconditioner(jac)
	
	-- Transfer and Projection
	transfer = utilCreateP1Prolongation(approxSpace)
	transfer:set_dirichlet_post_process(dirichletBND)
	projection = utilCreateP1Projection(approxSpace)
	
	-- Geometric Multi Grid
	gmg = utilCreateGeometricMultiGrid(approxSpace)
	gmg:set_discretization(domainDisc)
	gmg:set_surface_level(numRefs)
	gmg:set_base_level(0)
	gmg:set_base_solver(base)
	gmg:set_smoother(jac)
	gmg:set_cycle_type(1)
	gmg:set_num_presmooth(3)
	gmg:set_num_postsmooth(3)
	gmg:set_prolongation(transfer)
	gmg:set_projection(projection)

-- create AMG ---
-----------------

	if false then
	amg = AMGPreconditioner()
	amg:set_nu1(2)
	amg:set_nu2(2)
	amg:set_gamma(1)
	amg:set_presmoother(jac)
	amg:set_postsmoother(jac)
	amg:set_base_solver(base)
	--amg:set_debug(u)
	end

-- create BiCGStab Solver
bicgstabSolver = BiCGStab()

-- create Linear Solver
linSolver = LinearSolver()

-- exact Solver
exactSolver = LU()

-- create Neumann CG Solver
neumannConvCheck = StandardConvergenceCheck()
neumannConvCheck:set_maximum_steps(2000)
neumannConvCheck:set_minimum_defect(1e-10)
neumannConvCheck:set_reduction(1e-16)
neumannConvCheck:set_verbose_level(false)
neumannCGSolver = CG()
neumannCGSolver:set_preconditioner(ilu)
neumannCGSolver:set_convergence_check(neumannConvCheck)

-- create Dirichlet CG Solver
dirichletConvCheck = StandardConvergenceCheck()
dirichletConvCheck:set_maximum_steps(2000)
dirichletConvCheck:set_minimum_defect(1e-10)
dirichletConvCheck:set_reduction(1e-16)
dirichletConvCheck:set_verbose_level(false)
dirichletCGSolver = CG()
dirichletCGSolver:set_preconditioner(ilu2)
dirichletCGSolver:set_convergence_check(dirichletConvCheck)

-- create FETI Solver
fetiSolver = FETI()

fetiConvCheck = StandardConvergenceCheck()
fetiConvCheck:set_maximum_steps(20)
fetiConvCheck:set_minimum_defect(1e-8)
fetiConvCheck:set_reduction(1e-16)

--fetiSolver:set_debug(dbgWriter)
fetiSolver:set_convergence_check(fetiConvCheck)
fetiSolver:set_domain_decomp_info(domainDecompInfo)

fetiSolver:set_neumann_solver(neumannCGSolver)
fetiSolver:set_dirichlet_solver(dirichletCGSolver)
fetiSolver:set_coarse_problem_solver(exactSolver)

-- Apply Solver
print( "   numPreRefs is " .. numPreRefs .. ",  numRefs is " .. numRefs)
print( "   numProcs   is " .. numProcs   .. ",  NumSubDomains is " .. numSubdomains )

tBefore = os.clock()
ApplyLinearSolver(linOp, u, b, fetiSolver)
tAfter = os.clock()

-- Output
output = io.open("feti-profile.txt", "a")

assemblePNinit  = GetProfileNode("initLinearSolver")
assemblePNapply = GetProfileNode("applyLinearSolver")

print("\n")
print("dim\tnumPreRefs\tnumRefs\tnumSD\tnumProcsPerSD\t\tCPU time (s)\tinitLinearSolver (ms)\tapplyLinearSolver (ms)");
s = string.format("%d\t%d\t\t%d\t\t%d\t\t%d\t\t\t%.2f\t\t%.2f\t\t\t%.2f\n",
		  dim, numPreRefs, numRefs, numSubdomains, numProcsPerSubdomain,
		  tAfter-tBefore,
		  assemblePNinit:get_avg_total_time_ms(),
		  assemblePNapply:get_avg_total_time_ms())
output:write(s)
print(s)

if verbosity >= 1 then
	WriteGridFunctionToVTK(u, "Solution")
end

-- check
print( "domainDecompInfo:get_num_subdomains(): " .. domainDecompInfo:get_num_subdomains())
