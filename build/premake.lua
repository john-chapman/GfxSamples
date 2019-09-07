local FRM_ROOT = "../extern/GfxSampleFramework/"
local APT_ROOT = FRM_ROOT .. "extern/ApplicationTools/"

dofile(APT_ROOT .. "build/ApplicationTools_premake.lua")
dofile(FRM_ROOT .. "build/GfxSampleFramework_premake.lua")

workspace "GfxSamples"
	location(_ACTION)
	platforms { "Win64" }
	language "C++"
	cppdialect "C++11"
	filter { "platforms:Win64" }
		system "windows"
		architecture "x86_64"
	filter {}

	rtti "Off"
	exceptionhandling "Off"
	staticruntime "On"

	configurations { "Debug", "Release" }
	filter { "configurations:Debug" }
		targetsuffix "_debug"
		symbols "On"
		optimize "Off"
	filter {}
	filter { "configurations:Release" }
		symbols "Off"
		optimize "Full"
	filter {}
	
	group "libs"
		--ApplicationTools_ProjectExternal(APT_ROOT)
		ApplicationTools_Project(
			APT_ROOT,
			APT_ROOT .. "lib"
			)
	group ""
	group "libs"
		GfxSampleFramework_Project(
			FRM_ROOT,
			FRM_ROOT .. "lib",
			"../bin"
			)
	group ""

 -- symlink to common sample data in bin/sample_common (bin/common already points to GfxSampleFramework/data/common)
	local projDir = "$(ProjectDir)..\\..\\"
	local dataDir = projDir .. "data\\"
	local binDir  = projDir .. "bin\\"
	filter { "action:vs*" }
		postbuildcommands({
			"if not exist \"" .. binDir .. "sample_common\" mklink /j \"" .. binDir .. "sample_common\" \"" .. dataDir .. "common\"",
			})
	filter{}

 -- create projects
	local projList = dofile("projects.lua")
	for name,fileList in pairs(projList) do
		project(tostring(name))
			kind "ConsoleApp"
			targetdir "../bin"
				ApplicationTools_Link()
				GfxSampleFramework_Link()

				vpaths({ ["*"] = { "../src/" .. tostring(name) .. "/**" } })
				files(fileList)
				files("../src/_sample.cpp")

				local projDataDir = dataDir .. tostring(name)
				local projBinDir  = binDir  .. tostring(name)
				filter { "action:vs*" }
					postbuildcommands({
					  -- make the project data dir
						"if not exist " .. projDataDir .. " mkdir \"" .. projDataDir .. "\"",

					  -- make link to project data dir in bin
						"if not exist " .. projBinDir .. " mklink /j \"" .. projBinDir .. "\" \"" .. projDataDir .. "\"",
						})
				filter {}
	end
