local FRM_ROOT = "../extern/GfxSampleFramework/"
local APT_ROOT = FRM_ROOT .. "extern/ApplicationTools/"

dofile(APT_ROOT .. "build/ApplicationTools_premake.lua")
dofile(FRM_ROOT .. "build/GfxSampleFramework_premake.lua")

workspace "GfxSamples"
	location(_ACTION)
	platforms { "Win64" }
	language "C++"
	cppdialect "C++11"
	flags { "StaticRuntime" }
	filter { "platforms:Win64" }
		system "windows"
		architecture "x86_64"
	filter {}

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
		ApplicationTools_ProjectExternal(APT_ROOT)
	group ""
	group "libs"
		GfxSampleFramework_Project(
			FRM_ROOT,
			FRM_ROOT .. "/lib",
			"../bin"
			)
	group ""

	local projList = dofile("projects.lua")
	for name,fileList in pairs(projList) do
		project(tostring(name))
			kind "ConsoleApp"
			targetdir "../bin"

				ApplicationTools_Link()
				GfxSampleFramework_Link()

				files(fileList)
				files({
					"../src/_sample.cpp",
					})

				local projDir = "$(ProjectDir)../../"
				local dataDir = projDir .. "data/"
				local binDir  = projDir .. "bin/"
				filter { "action:vs*" }
					postbuildcommands({
						"if not exist \"" .. dataDir .. tostring(name) .. "\" mkdir \"" .. dataDir .. tostring(name) .. "\"",
						"if not exist \"" .. binDir .. tostring(name) .. "\" mklink /j \"" .. binDir .. tostring(name) .. "\" \"" .. dataDir .. tostring(name) .. "\"",
						"if not exist \"" .. binDir .. "common2\" mklink /j \"" .. binDir .. "common2\" \"" .. dataDir .. "common\"",
						})
	end
