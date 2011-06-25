function DumpTable(t, level)
	if (not level) then
		level = 0
	end
	
	for k, v in pairs(t) do
		if (k and v and type(v) == "table") then
			io.write(string.rep("\t",level))
			print("Table[" .. k .. "]")
			DumpTable(v, level + 1)
		else
			io.write(string.rep("\t",level))
			if (type(v) == "boolean") then
				if (v) then v = "true" else v = "false" end
			elseif (type(v) == "function") then
				v = "function"
			end
			print(k .. " = " .. v)
		end
	end
end

local SectionTable =
{
	["[global]"] = {},
	["[includes]"] = {},
	["[object]"] = {},
	["[properties]"] = {},
	["[methods]"] = {},
	["[backend]"] = {},
}

local GlobalKeywordTable =
{
	["module"] = 1,
	["header"] = 1,
	["codefilename"] = 1,
	["parentheadername"] = 1,
}

local ObjectKeywordTable =
{
	["name"] = 1,
	["friendlyname"] = 1,
	["description"] = 1,
	["usesview"] = 1,
	["parentclass"] = 1,
}

local PropertyKeywordTable =
{
	["property"] = 1,
	["getvalue"] = 1,
	["setvalue"] = 1,
	["enum"] = 1,
	["beginembeddedcode"] = 1,
	["endembeddedcode"] = 1,
}

local MethodKeywordTable =
{
	["param"] = 1,
	["return"] = 1,
}

local BackendKeywordTable =
{
	["parentclass"] = 1,
	["initcode"] = 1,
	["usepviewevents"] = 1,
}


function ReadIDL(path)
	local file = io.open(path)
	
	if (not file) then
		return nil
	end
	
	local lines = {}
	local line = file:read()
	
	while (line) do
		table.insert(lines, line)
		line = file:read()
	end
	
	file:close()
	
	return lines
end


function ParseIntoSections(lineData)
	local sectionName = ""
	local sectionTable = SectionTable
	
	for i = 1, #lineData do
		if (SectionTable[lineData[i]]) then
			sectionName = lineData[i]
		else
			table.insert(sectionTable[sectionName], lineData[i])
		end
	end
	
	return sectionTable
end


function ParsePairSection(sectionData, sectionName)
	local outTable = {}
	
	for i = 1, #sectionData do
		local k, v = sectionData[i]:match('%s-(%w+)%s-=%s-(.+)')
		
		-- something might be wrong in the [global] section. Check
		-- for a blank line or a comment before complaining
		if (not k) then
			if (sectionData[i]:match('[^%s]+') and sectionData[i]:sub(1,1) ~= "#") then
				print("Unrecognized code in line " .. i ..
					" of the " .. sectionName ..
					" section. Aborting.\nError: '" .. sectionData[i] .. "'")
				return nil
			end
		else
			outTable[k] = v
		end
	end
	
	return outTable
end


function ParseIncludeSection(sectionData)
	-- This function doesn't do much except strip out blank lines
	-- and do error checking
	local outTable = {}
	
	for i = 1, #sectionData do
		local k = sectionData[i]:match('%s-(["<]%w+%.%w+[">])%s-')
		
		-- something might be wrong in the [includes] section. Check
		-- for a blank line or a comment before complaining
		if (not k) then
			if (sectionData[i]:match('[^%s]+') and sectionData[i]:sub(1,1) ~= "#") then
				print("Unrecognized code in line " .. i ..
					" of the includes section. Aborting.\nError: '" ..
					sectionData[i] .. "'")
				return nil
			end
		else
			table.insert(outTable, k)
		end
	end
	
	return outTable
end


function ParsePropertySection(sectionData)
	local outTable = {}
	
	local propName = nil
	local readEmbeddedCode = false
	local embeddedCode = ""
	local embeddedName = ""
	local embeddedNameCode = ""
	for i = 1, #sectionData do
		if (sectionData[i]:sub(1,1) == "#") then
			-- It's a commented line. Do nothing.
		
		elseif (sectionData[i]:match("%s-[bB]egin[eE]mbedded[cC]ode")) then
			if (not readEmbeddedCode) then
				print("BeginEmbeddedCode follows a non-embedded Get/SetValue in line " ..
					i .. ". Aborting.")
				return nil
			end
		
		elseif (readEmbeddedCode) then
			local endEmbedded = sectionData[i]:match('%s-[eE]nd[eE]mbedded[cC]ode%s-')
			if (endEmbedded) then
				readEmbeddedCode = false
				outTable[propName][embeddedName] = "embedded"
				outTable[propName][embeddedNameCode] = embeddedCode
			else
				embeddedCode = embeddedCode .. sectionData[i] .. "\n"
			end
		
		elseif (sectionData[i]:match('%s-[pP]roperty%s+')) then
			local returnType, defaultValue, propDesc = nil
			returnType, defaultValue, propName=
				sectionData[i]:match('%s-[pP]roperty%s+([^%)]+)%(%s-([^%)]+)%)%s+(%w+)')
			propDesc = sectionData[i]:match(':%s-(.*)')
			
			local missingVar = nil
			if (not returnType) then
				missingVar = "return type"
			elseif (not defaultValue) then
				missingVar = "default value"
			elseif (not propName) then
				missingVar = "property name"
			end
			
			if (missingVar) then
				print("Couldn't find " .. missingVar .. " in properties line " ..
					i .. ". Aborting.")
				return nil
			end
			
			outTable[propName] = {}
			outTable[propName].returnType = returnType
			outTable[propName].defaultValue = defaultValue
			outTable[propName].propDesc = propDesc
			
			if (returnType == "enum") then
				outTable[propName].enums = {}
			end
			
		elseif (sectionData[i]:match('%s-[gG]et[vV]alue%s-')) then
			if (not propName) then
				print("GetValue line " .. i .. " does not follow a Property line. Aborting.")
				return nil
			end
			
			local getName, inType = 
				sectionData[i]:match('%s-[gG]et[vV]alue%s-:%s-([%w_]+)%(([%w_]+)')
			local inCast = sectionData[i]:match('%(.-%->([%w_]+)')
			
			if ((not getName) and (not inType)) then
				print("Badly formed GetValue line in properties line " .. i ..
					". Aborting")
				return nil
			end
			
			outTable[propName].getValue = {}
			outTable[propName].getValue.name = getName
			outTable[propName].getValue.inType = inType
			outTable[propName].getValue.castAs = inCast
			if (string.lower(inType) == "embedded") then
				readEmbeddedCode = true
				embeddedCode = ""
				embeddedName = "getValue"
				embeddedNameCode = "getValueCode"
			end
			
		elseif (sectionData[i]:match('%s-[sS]et[vV]alue%s-')) then
			if (not propName) then
				print("SetValue line " .. i .. " does not follow a Property line. Aborting.")
				return nil
			end
			
			local setName, outType = 
				sectionData[i]:match('%s-[sS]et[vV]alue%s-:%s-([%w_]+)%(([%w_]+)')
			local outCast = sectionData[i]:match('%(.-%->([%w_]+)')
			
			if ((not setName) and (not outType)) then
				print("Badly formed SetValue line in properties line " .. i ..
					". Aborting")
				return nil
			end
			
			outTable[propName].setValue = {}
			outTable[propName].setValue.name = setName
			outTable[propName].setValue.outType = outType
			outTable[propName].setValue.castAs = outCast
			if (string.lower(outType) == "embedded") then
				readEmbeddedCode = true
				embeddedCode = ""
				embeddedName = "setValue"
				embeddedNameCode = "setValueCode"
			end
			
		elseif (sectionData[i]:match('%s-[eE]num%s-')) then
			if (not propName) then
				print("Enum line " .. i .. " does not follow a Property line. Aborting.")
				return nil
			end
			
			local enumName, enumValue = sectionData[i]:match('%s-[eE]num%s-:%s-(%b"")%s-,([%w_]+)')
			
			if (not enumName) then
				enumName, enumValue = sectionData[i]:match('%s-[eE]num%s-:%s-([%w_]+)%s-,([%w_]+)')
			end
			
			if ((not enumName) or (not enumValue)) then
				local errName = nil
				if (enumName) then
					errName = "name"
				else
					errName = "value"
				end
				
				print("Missing enum " .. errName .. " in line " .. i .. " of the Properties section for property " ..
					propName ..	". Aborting")
				return nil
			end
			
			if (not outTable[propName].enums) then
				print("Enumerated value definition in Properties line " .. i .. "for non-enumerated property" ..
						propName .. ". Aborting")
				return nil
			end
			
			
			table.insert(outTable[propName].enums, { enumName, enumValue })
		else
		
			-- something might be wrong in the [global] section. Check
			-- for a blank line before complaining
			if (sectionData[i]:match('[^%s]+')) then
				print("Unrecognized code in line " .. i ..
					" of the Property section. Aborting.\nError: '" ..
					sectionData[i] .. "'")
				return nil
			end
		end
		
	end	-- end the for loop which reads the section
	
	return outTable
end


function ParseMethodSection(sectionData)
	local outTable = {}
	
	local methodName = nil
	local readEmbeddedCode = false
	local embeddedCode = ""
	
	for i = 1, #sectionData do
		
		if (readEmbeddedCode) then
			local endEmbedded = sectionData[i]:match('%s-[eE]nd[eE]mbedded[cC]ode%s-')
			if (endEmbedded) then
				readEmbeddedCode = false
				outTable[methodName].embeddedCode = embeddedCode
			else
				embeddedCode = embeddedCode .. sectionData[i] .. "\n"
			end
		elseif (sectionData[i]:sub(1,1) == "#") then
			-- It's a commented line. Do nothing.
			
		elseif (sectionData[i]:match('%s-[mM]ethod%s+')) then
			methodName = sectionData[i]:match('%s-[mM]ethod%s+([%w_]+)')
			
			outTable[methodName] = {}
			outTable[methodName].params = {}
			outTable[methodName].returnvals = {}
			outTable[methodName].callName = methodName
			
		elseif (sectionData[i]:match('%s-[pP]aram%s+')) then
			
			if (not methodName) then
				print("Param line " .. i .. " does not follow a Method line. Aborting.")
				return nil
			end
			
			local paramType, paramName =
				sectionData[i]:match('%s-[pP]aram%s+([%w_]+)%s+([%w_]+)')
			
			local inCast = sectionData[i]:match('%(([&%*%w_]+)%)')
			
			local paramData = {}
			paramData.paramType = paramType
			paramData.paramName = paramName
			paramData.inCast = inCast
			
			table.insert(outTable[methodName].params, paramData)
			
		elseif (sectionData[i]:match('%s-[rR]eturn%s+')) then
			
			if (not methodName) then
				print("Return line " .. i .. " does not follow a Method line. Aborting.")
				return nil
			end
			
			local returnType, paramName, outType =
				sectionData[i]:match('%s-[rR]eturn%s+([%w_]+)%s+([%w_]+)')
			
			local outCast = sectionData[i]:match('%(([&%*%w_]+)%)')
			
			local returnData = {}
			returnData.returnType = returnType
			returnData.paramName = paramName
			returnData.outCast = outCast
			
			table.insert(outTable[methodName].returnvals, returnData)
		
		elseif (sectionData[i]:match('%s-[cC]all[nN]ame%s-([%w_]+)')) then
			
			if (not methodName) then
				print("CallName line " .. i .. " does not follow a Method line. Aborting.")
				return nil
			end
			
			outTable[methodName].callName = sectionData[i]:match('%s-[cC]all[nN]ame%s-([%w_]+)')
			
		elseif (sectionData[i]:match('%s-([bB]egin[eE]mbedded[cC]ode)')) then
			readEmbeddedCode = true
			embeddedCode = ""
		else
			-- something might be wrong in the [global] section. Check
			-- for a blank line before complaining
			if (sectionData[i]:match('[^%s]+')) then
				local outmsg = "Unrecognized code in line " .. i ..
					" of the Method section"
				if (methodName) then
					outmsg = outmsg .. " in declaration of method " .. methodName
				end
				outmsg = outmsg .. ". Aborting.\nError: '" .. sectionData[i] .. "'"
				print(outmsg)
				return nil
			end
		end
	end
	
	return outTable
end


function ParseSections(sectionData)
	local outTable = {}
	
	outTable.global = ParsePairSection(sectionData["[global]"], "global")
	outTable.includes = ParseIncludeSection(sectionData["[includes]"])
	outTable.object = ParsePairSection(sectionData["[object]"], "object")
	outTable.properties = ParsePropertySection(sectionData["[properties]"])
	outTable.methods = ParseMethodSection(sectionData["[methods]"])
	outTable.backend = ParsePairSection(sectionData["[backend]"], "backend")
	
	return outTable
end


function ParsePObjFile(path)
	local fileData = ReadIDL(path)
	
	if (not fileData) then
		print("Couldn't open file '" .. path .. "'.")
		return nil
	end
	
	local sectionTable = ParseIntoSections(fileData)
	fileData = nil
	
	local defTable = ParseSections(sectionTable)
	if (not defTable) then
		return 1
	end
	
	return defTable
end
