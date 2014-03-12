--[[!
-- \defgroup scripts_util_table Table Utility
-- \ingroup scripts_util
-- Utility functions for easy but advanced table access.
-- \{
]]--

table = table or {}

--! appends a table to a table
function table.append(t1, t2)
	if type(t1) ~= "table" then
		print("table.append called on non-table"); exit();
	end
	
	if type(t2) == "table" then
       for _,v in ipairs(t2) do
            t1[#t1+1] = v
       end
    else
       t1[#t1+1] = t2
    end	
end

--! returns shallow-copy of table (i.e., only top level values)
function table.shallowcopy(t)
    local copy
    if type(t) == "table" then
        copy = {}
        for k, v in pairs(t) do
            copy[k] = v
        end
    else -- number, string, boolean, etc
        copy = t
    end
    return copy
end

--! returns shallow-copy of integer-key part of table (i.e., only top level values)
function table.ishallowcopy(t)
    local copy
    if type(t) == "table" then
        copy = {}
        for k, v in ipairs(t) do
            copy[k] = v
        end
    else -- number, string, boolean, etc
        copy = t
    end
    return copy
end


--! returns deep-copy of table (i.e., copies recursive all contained tables as well)
function table.deepcopy(t)
    local copy
    if type(t) == "table" then
        copy = {}
        for k, v in next, t, nil do
            copy[table.deepcopy(k)] = table.deepcopy(v)
        end
        setmetatable(copy, table.deepcopy(getmetatable(t)))
    else -- number, string, boolean, etc
        copy = t
    end
    return copy
end

--! returns deep-copy of integer-key part of table (i.e., copies recursive all contained tables as well)
function table.ideepcopy(t)
    local copy
    if type(t) == "table" then
        copy = {}
        for k, v in ipairs(t) do
            copy[k] = table.deepcopy(v)
        end
    else -- number, string, boolean, etc
        copy = t
    end
    return copy
end

--! returns the smallest integer key of the table (even negative or null if present)
function table.imin(t)
	local min = math.huge
	for k, _ in pairs(t) do
		if type(k) == "number" and math.floor(k)==k then
			min = math.min(min, k)
		end
	end
	return min
end

--! returns the largest integer key of the table (even in non-consecutive arrays)
function table.imax(t)
	local max = -math.huge
	for k, _ in pairs(t) do
		if type(k) == "number" and math.floor(k)==k then
			max = math.max(max, k)
		end
	end
	return max
end

--! returns iterator function iterating through first consecutive integer-key range (even starting at negative or null)
function iipairs(tab)
	local n = table.imin(tab)-1
	local t = tab
	return function (i)
      	 n = n + 1
      	 local v = t[n]
     	 if v then
        	return n, v
      	else
      		return nil
    	end
    end
end


--! checks if a value is contained in a table
function table.contains(t, value)
	for _,v in pairs(t) do
		if v == value then return true end
	end
	return false
end

--! prints the table
function table.print(data, style)

	-- get format
	local heading = style.heading
	local format = style.format or {}
	local vline = style.vline or false
	local hline = style.hline or false
	local forNil = style.forNil or " "

	-- compute row range	
	local minRow = math.huge
	local maxRow = -math.huge
	for _, column in ipairs(data) do
		if type(column) == "table" then
			minRow = math.min(minRow, table.imin(column))
			maxRow = math.max(maxRow, table.imax(column))		
		else 
			print("table.print: expect consecutive lua-table-entries as column")
		end
	end		

	-- compute column width (in data)
	local width = {}
	for col, _ in ipairs(data) do
		width[col] = 0
		for row = minRow, maxRow do
			local s = data[col][row];
			if s ~= nil then
				if type(s) == "number" and format[col] ~= nil then 
					s = string.format(format[col], s)
				end
				width[col] = math.max(width[col], #tostring(s))
			end
		end
	end
	
	-- compute column width (in header)
	-- write header and vline
	if heading ~= nil then
		local linesize = 0
		for col, _ in ipairs(data) do
			local s = 	heading[col]
			if s ~= nil then s = tostring(s)
			else			 s = tostring(forNil) end
		
			width[col] = math.max(width[col], #s)
			write(" ".. string.format("%"..width[col].."s", s) .." ");
			
			linesize = linesize + width[col] + 2;
			if hline == true then
				linesize = linesize + 1;
				write("|");
			end
		end
		write("\n")
		if vline == true then
			write(string.rep("-", linesize), "\n");
		end
	end
	
	-- write data
	for row = minRow, maxRow do
		for col, _ in ipairs(data) do
			local s = data[col][row];
			
			if s ~= nil then 
				if type(s) == "number" and format[col] ~= nil then
					s = string.format(format[col], s)
				else
					s = tostring(s)
				end
			else		
				s = forNil
			end

			write (" ".. string.format("%"..width[col].."s", s) .." ")
			
			if hline == true then write("|"); end
		end
		write("\n");
	end		
end

-- end group scripts_util_table
--[[!
\}
]]--
