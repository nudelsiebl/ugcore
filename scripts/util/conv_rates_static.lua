util = util or {}
util.rates = util.rates or {}
util.rates.static = util.rates.static or {}

ug_load_script("util/persistence.lua")

--------------------------------------------------------------------------------
-- Std Functions (used as defaults)
--------------------------------------------------------------------------------

function util.rates.static.StdPrepareInitialGuess(u, lev, minLev, maxLev,
															domainDisc, solver)

	if lev > minLev then	
		Prolongate(u[lev], u[lev-1]);
		write(">> Solution interpolated as start value from coarser level.\n")
	else
		u[lev]:set(0.0)
		write(">> Solution set to zero on coarsest level.\n")	
	end		
end


function util.rates.static.StdComputeLinearSolution(u, domainDisc, solver)

	-- create operator from discretization
	local A = AssembledLinearOperator(domainDisc)
	local b = u:clone()
	write(">> Algebra created.\n")
	
	-- 1. init operator
	domainDisc:assemble_linear(A, b)
	write(">> Matrix and Rhs assembled.\n")
	
	-- 2. set dirichlet values in start iterate
	domainDisc:adjust_solution(u)
	write(">> Inital guess for solution prepared.\n")
	
	-- 3. init solver for linear Operator
	solver:init(A, u)
	write(">> Linear Solver initialized.\n")
	
	-- 4. apply solver
	if solver:apply_return_defect(u, b) ~= true then
		write(">> Linear solver failed. Aborting."); exit();
	end
end

function util.rates.static.StdComputeNonLinearSolution(u, domainDisc, solver)

	solver:init(AssembledOperator(domainDisc, u:grid_level()))
	if solver:apply(u) == false then
		 print (">> Newton solver apply failed."); exit();
	end
	write(">> Newton Solver done.\n")
end

function util.rates.static.StdMaxLevelPadding(p)
	return math.floor(p/2)
end
--------------------------------------------------------------------------------
-- util.rates.static.compute (main-function)
--------------------------------------------------------------------------------

--[[!
Computes convergence rates for a static problem

In the convergence rate setup the following parameters can be passed:
- (required) CreateDomain()				
			 	function used to create Domain
- (required) CreateApproxSpace(dom, discType, p)		
			 	function used to create ApproximationSpace
- (required) CreateDomainDisc(approxSpace, discType, p)			
			 	function used to create Domain Discretization
- (required) CreateSolver(approxSpace, discType, p)				
				function used to create Solver
- (required) DiscTypes					
				Array containing types, orders and level to be looped
- (optional) ComputeSolution(u, domainDisc, solver)			
				function used to compute solution
- (optional) PrepareInitialGuess		
				function used to prepare Initial Guess
- (optional) ExactSol					
				Array containing exact solution as a function
- (optional) ExactGrad					
				Array containing exact gradients as a function
 
@param ConvRate Setup setup used 
]]--
function util.rates.static.compute(ConvRateSetup)
	
	-- check passed param
	local CRS
	if ConvRateSetup == nil then print("No setup passed."); exit()		
	else CRS = ConvRateSetup end
	
	-- create directories
	local plotPath = CRS.plotPath or "plots/"
	local solPath  = CRS.solPath  or "sol/"
	local dataPath = CRS.dataPath or "data/"

	local gpOptions = CRS.gpOptions or
	{	
		grid = true, 
		logscale = true,
		mtics = true
	 }

	-- check for methods
	local PrepareInitialGuess = CRS.PrepareInitialGuess or util.rates.static.StdPrepareInitialGuess
	local ComputeSolution = 	CRS.ComputeSolution     or util.rates.static.StdComputeLinearSolution
	local CreateApproxSpace = 	CRS.CreateApproxSpace
	local CreateDomainDisc = 	CRS.CreateDomainDisc
	local CreateSolver = 		CRS.CreateSolver
	local CreateDomain = 		CRS.CreateDomain
	local MaxLevelPadding = 	CRS.MaxLevelPadding 	or util.rates.static.StdMaxLevelPadding
	
	if 	CreateApproxSpace == nil or CreateDomainDisc == nil or 
		CreateSolver == nil or CreateDomain == nil then
		print("You must pass: CreateApproxSpace, CreateDomainDisc, CreateSolver, CreateDomain")
		exit()
	end
	
	local DiscTypes = CRS.DiscTypes

	local maxlevel = CRS.maxlevel; if maxlevel == nil then maxlevel = true end
	local prevlevel = CRS.prevlevel; if prevlevel == nil then prevlevel = true end

	local ExactSol = CRS.ExactSol 
 	local ExactGrad = CRS.ExactGrad 
	
	--------------------------------------------------------------------
	--  Loop Discs
	--------------------------------------------------------------------

	local function ensureDir(name)
		if not(DirectoryExists(name)) then CreateDirectory(name) end
	end
	
	ensureDir(dataPath)
	ensureDir(plotPath)
	ensureDir(plotPath.."single/")
	ensureDir(solPath)

	-- compute element size	
	local dom = CreateDomain()
	local numRefs = dom:grid():num_levels() - 1;
	
	-- to store measurement
	local gpData = {};
	local errors = {};
	
	for type = 1,#DiscTypes do
	
		local discType 	= DiscTypes[type].type
		local pmin 		= DiscTypes[type].pmin
		local pmax 		= DiscTypes[type].pmax
		local lmin 		= DiscTypes[type].lmin
		local lmax 		= DiscTypes[type].lmax		
		
		if pmin == nil then pmin = 1 end
		if pmax == nil then pmax = 1 end
		if lmin == nil then lmin = 0 end
		if lmax == nil then lmax = numRefs end
		
		if lmin > lmax then
			print("lmin: "..lmin.." must be less or equal lmax: "..lmax)
			exit()
		end
		
		if lmax > numRefs then
			print("lmax: "..lmax.." must be less or equal numRefs: "..numRefs)
			exit()
		end
	
		errors[discType] = errors[discType] or {}
		
		for p = pmin, pmax do
		
			errors[discType][p] = errors[discType][p] or {}
			
			local maxLev = lmax - MaxLevelPadding(p)
			local minLev = lmin
	
			--------------------------------------------------------------------
			--  Print Setup
			--------------------------------------------------------------------
	
			print("\n")
			print(">> -------------------------------------------------------------------")
			print(">>    Computing solutions and error norms of the following problem")
			print(">> -------------------------------------------------------------------")
			print(">>     dim        = " .. dim)
			print(">>     grid       = " .. gridName)
			print(">>     minLev     = " .. minLev)
			print(">>     maxLev     = " .. maxLev)
			print(">>     type       = " .. discType)
			print(">>     order      = " .. p)
			print("\n")
			
			--------------------------------------------------------------------
			--  Create ApproxSpace, Disc and Solver
			--------------------------------------------------------------------

			print(">> Create ApproximationSpace: "..discType..", "..p)
			local approxSpace = CreateApproxSpace(dom, discType, p)
			
			print(">> Create Domain Disc: "..discType..", "..p)
			local domainDisc = CreateDomainDisc(approxSpace, discType, p)
			
			print(">> Create Solver")
			local solver = CreateSolver(approxSpace, discType, p)
	
			-- get names in approx space
			local FctCmp = approxSpace:names()
			
			--------------------------------------------------------------------
			--  Create Solutions on each level
			--------------------------------------------------------------------
			
			write("\n>> Allocating storage for solution vectors.\n")
			local u = {}
			for lev = minLev, maxLev do
				u[lev] = GridFunction(approxSpace, lev)
			end

			--------------------------------------------------------------------
			--  Compute Solution on each level
			--------------------------------------------------------------------
			
			for lev = minLev, maxLev do
				write("\n>> Computing Level "..lev..".\n")
			
				write(">> Preparing inital guess on level "..lev..".\n")
				PrepareInitialGuess(u, lev, minLev, maxLev, domainDisc, solver)
				
				write(">> Computing solution on level "..lev..".\n")
				ComputeSolution(u[lev], domainDisc, solver)
				write(">> Solver done.\n")
				
				WriteGridFunctionToVTK(u[lev], solPath.."sol_"..discType..p.."_l"..lev)
				write(">> Solution written to: "..solPath.."sol_"..discType..p.."_l"..lev.."\n");	
			end
			
			approxSpace, domainDisc, solver = nil, nil, nil
			collectgarbage()

			--------------------------------------------------------------------
			--  Compute Error Norms on each level
			--------------------------------------------------------------------
			
			-- prepare error measurement			
			local err = errors[discType][p]
			err.h, err.numDoFs, err.level = {}, {}, {}	
			for lev = minLev, maxLev do	
				err.h[lev] = MaxElementDiameter(dom, lev) 
				err.level[lev] = lev
			end	

			-- loop levels and compute error
			for lev = maxLev, minLev, -1 do
				write("\n>> Error Norm values on Level "..lev..".\n")
				
				local quadOrder = p+3
				err.numDoFs[lev] = u[lev]:size()
				write(">> #DoF       on Level "..lev.." is "..err.numDoFs[lev] .."\n");
			
				-- compute for each component
				for _, f in pairs(FctCmp) do

					-- create component
					err[f] = err[f] or {}
							
					-- help fct to create an measurement
					local function createMeas(f, t, n)
						err[f][t] = err[f][t] or {}
						err[f][t][n] = err[f][t][n] or {}
						err[f][t][n].value = err[f][t][n].value or {}						
						return err[f][t][n].value
					end
										
					-- w.r.t exact solution		
					if ExactSol ~= nil and ExactSol[f] ~= nil then 					
						local value = createMeas(f, "exact", "l2")
						value[lev] = L2Error(ExactSol[f], u[lev], f, 0.0, quadOrder)
						write(">> L2 l-exact for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");

						if ExactGrad ~= nil and ExactGrad[f] ~= nil then 					
						local value = createMeas(f, "exact", "h1")
						value[lev] = H1Error(ExactSol[f], ExactGrad[f], u[lev], f, 0.0, quadOrder)
						write(">> H1 l-exact for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");
						end
					end
					
					-- w.r.t max level solution
					if maxlevel and lev < maxLev then
						local value = createMeas(f, "maxlevel", "l2")
						value[lev]	= L2Error(u[maxLev], f, u[lev], f, quadOrder)
						write(">> L2 l-lmax  for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");

						local value = createMeas(f, "maxlevel", "h1")
						value[lev] 	= H1Error(u[maxLev], f, u[lev], f, quadOrder)
						write(">> H1 l-lmax  for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");
					end
				
					-- w.r.t previous level solution
					if prevlevel and lev > minLev then 
						local value = createMeas(f, "prevlevel", "l2")
						value[lev] = L2Error(u[lev], f, u[lev-1], f, quadOrder)
						write(">> L2 l-(l-1) for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");
	
						local value = createMeas(f, "prevlevel", "h1")
						value[lev] = H1Error(u[lev], f, u[lev-1], f, quadOrder)
						write(">> H1 l-(l-1) for "..f.." on Level "..lev.." is "..string.format("%.3e", value[lev]) .."\n");
					end
				end -- end fct
								
			end -- end level

			for lev = minLev, maxLev do u[lev] = nil end
			u = nil
			collectgarbage()
				
			--------------------------------------------------------------------
			--  A custom iterator to loop all valid measurements
			--------------------------------------------------------------------

			local function imeasure(err, FctCmp, Type, Norm)
				local type = _G.type
				if type(err) ~= "table" or type(FctCmp) ~= "table" 
					or type(Type) ~= "table" or type(Norm) ~= "table" then 
					return function() end, nil, nil
				end
			  	local _f,_t,_n = 1,1,0
			  	local function iter(err)
				   	_n = _n + 1
				   	local f,t,n = FctCmp[_f], Type[_t], Norm[_n]
			  		while err[f] == nil or err[f][t] == nil or err[f][t][n] == nil do
				    	_n = _n + 1    	
				  		if _n > #Norm then _n = 1; _t = _t + 1; end
				  		if _t > #Type then _t = 1; _f = _f + 1; end
				  		if _f > #FctCmp then return nil end
				   		f,t,n = FctCmp[_f], Type[_t], Norm[_n]
					end				
			    	return f, t, n , err[f][t][n]
			  	end  
			  	return iter, err, 0
			end

			--------------------------------------------------------------------
			--  Compute Factors and Rates
			--------------------------------------------------------------------

			for _,_,_,meas in imeasure(err, FctCmp,{"exact", "prevlevel", "maxlevel"}, {"l2", "h1"}) do
	
				meas.fac = meas.fac or {}
				meas.rate = meas.rate or {}
				
				local value = meas.value
				local fac = meas.fac
				local rate = meas.rate
				
				for lev, _ in pairs(value) do
					if value[lev] ~= nil and value[lev-1] ~= nil then
						fac[lev] = value[lev-1]/value[lev]
						rate[lev] = math.log(fac[lev]) / math.log(2)
					end
				end
			end	

			--------------------------------------------------------------------
			--  Prepare labels
			--------------------------------------------------------------------

			local head = {l2 = "L2", h1 = "H1",
							exact = "l-exact", maxlevel = "l-lmax", prevlevel = "l-prev"}
						
			local gpType = {	exact = 	"exact",		
								maxlevel = 	"L_{max}",
								prevlevel = "L-1"
							}
		
			local gpNorm = 	{ l2 = "L_2",	h1 = "H^1"}
							
			local gpXLabel ={ DoF = "Anzahl Unbekannte",	h = "h (Gitterweite)"}

			--------------------------------------------------------------------
			--  Write Data to Screen
			--------------------------------------------------------------------

			for _, f in ipairs(FctCmp) do
			
				print("\n>> Statistic for type: "..discType..", order: "..p..", comp: "..f.."\n")			
				
				local values = {err.level, err.h, err.numDoFs}
				local heading = {"L", "h", "#DoFs"}
				local format = {"%d", "%.2e", "%d"}

				for _,t,n,meas in imeasure(err, {f},{"exact", "prevlevel", "maxlevel"}, {"l2", "h1"}) do

					table.append(values, {meas.value, meas.rate}) 
					table.append(heading,{head[n].." "..head[t], "rate"})
					table.append(format, {"%.2e", "%.3f"})
				end
										
				table.print(values, {heading = heading, format = format, 
									 hline = true, vline = true, forNil = "--"})
			end

			--------------------------------------------------------------------
			--  Schedule Data to gnuplot
			--------------------------------------------------------------------
			
			for f,t,n,meas in imeasure(err, FctCmp,{"exact", "prevlevel", "maxlevel"}, {"l2", "h1"}) do
					
				-- write l2 and h1 to data file
				local file = dataPath..table.concat({"error",discType,p,f,head[t],n},"_")..".dat"
				local dataCols = {err.numDoFs, err.h, err[f][t][n].value}
				gnuplot.write_data(file, dataCols)
			
				-- create plot for single run				
				for x, xCol in pairs({DoF = 1, h = 2}) do
					local data = {{label=discType.." $\\mathbb{P}_{"..p.."}$", file=file, style="linespoints", xCol, 3}}
					local label = { x = gpXLabel[x],
									y = "$\\norm{ "..f.."_L - "..f.."_{"..gpType[t].."} }_{ "..gpNorm[n].."}$"}
					
					local file = plotPath.."single/"..table.concat({discType,p,f,head[t],n,x},"_")
					gpData[file] = gpData[file] or {} 				
					gpData[file].label = label
					table.append(gpData[file], data)
					
					for _, g in ipairs({discType, "all"}) do
						local file = plotPath..table.concat({f,g,head[t],n,x}, "_")	
						gpData[file] = gpData[file] or {} 				
						gpData[file].label = label
						table.append(gpData[file], data)
					end
				end	
			end
			
		end -- end loop over p		
	end -- end loop over type
	
	
	--------------------------------------------------------------------
	--  Execute Plot of gnuplot
	--------------------------------------------------------------------
	
	-- create scheduled plots
	for plotFile, data in pairs(gpData) do 
		local opt = table.deepcopy(gpOptions)
		opt.label = data.label
		gnuplot.plot(plotFile..".tex", data, opt)
		gnuplot.plot(plotFile..".pdf", data, opt)
	end
	
	-- save for reuse
	persistence.store(dataPath.."gp-data-files.lua", gpData);	
end


function util.rates.static.replot(gpOptions, file)
	local dataPath = "data/"
	local plotPath = "plots/"
	
	local function ensureDir(name)
		if not(DirectoryExists(name)) then CreateDirectory(name) end
	end
	
	ensureDir(dataPath)
	ensureDir(plotPath)
	ensureDir(plotPath.."single/")
	
	local file = file or dataPath.."gp-data-files.lua"
	gpData = persistence.load(file);
	
	-- create scheduled plots
	for plotFile, data in pairs(gpData) do 
		local opt = table.deepcopy(gpOptions)
		opt.label = data.label
		gnuplot.plot(plotFile..".tex", data, opt)
		gnuplot.plot(plotFile..".pdf", data, opt)
	end
	
end