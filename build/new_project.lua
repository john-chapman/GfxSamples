require "io"
require "os"
require "string"

function err(_msg)
	print("Error: " .. _msg)
	os.exit(-1)
end

-- validate the project name
local projName = arg[1] or ""
if projName == "" then
	err("No project name specified!")
end
projName = string.gsub(projName, "%s", "_") -- sanitize whitespace
if not string.match(projName, "^%a") then -- must start with a letter
	err("'" .. projName .. "' is not a valid name! (Must be C variable compliant).")
end

-- Name of the application bas class ("AppSample" or "AppSample3d")
local appBase = arg[2] or "AppSample"

-- add to projects.lua
local projList = dofile("build/projects.lua") or {}
if projList[projName] ~= nil then
	err("\"" .. projName .. "\" already exists!")
end

projList[projName] = { "../src/" .. projName .. "/**" }
local projFile = io.open("build/projects.lua", "w")
if projFile == nil then
	os.exit(-1)
end
projFile:write("return\n{\n")
for k, v in pairs(projList) do
	projFile:write("\t" .. tostring(k) .. " =\n\t{\n")
	for i, w in ipairs(v) do
		projFile:write("\t\t\"" .. tostring(w) .. "\",\n")
	end
	projFile:write("\t},\n\n")
end
projFile:write("}")
projFile:close()

-- generate main source files
function makeSrc(_projName, _appBase, _ext)
	local srcDir = "src\\" .. _projName .. "\\"

 -- don't overwrite an existing file
	local srcFile = io.open(srcDir .. _projName .. _ext, "r")
	if srcFile ~= nil then
		srcFile:close()
		return
	end

 -- load the skeleton file
	local skeletonFile = io.open("src/_skeleton" .. _ext, "r")
	local src = skeletonFile:read("*all")
	skeletonFile:close()
	src = string.gsub(src, "_skeleton", _projName) -- substitute _skeleton for the project name
	src = string.gsub(src, "AppSample", _appBase)  -- substitute AppSample for the application base class name
	
 -- write the src file (make the src dir if it doesn't exist)
	os.execute("if not exist " .. srcDir .. " mkdir " .. srcDir)
	srcFile = io.open(srcDir .. _projName .. _ext, "w")
	srcFile:write(src)
	srcFile:close()
end
makeSrc(projName, appBase, ".h")
makeSrc(projName, appBase, ".cpp")
