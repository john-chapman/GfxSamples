/*	Simple data processing system.
	- Files in _raw and _temp map to build commands. Commands are therefore identified by the provoking file path.
	- Commands have input and output dependencies. A command is dirty if any of its inputs is newer than the last time the command was executed,
	  or if any of its outputs is missing. The provoking file is also an input dependency.
	- Missing inputs should also trigger a command execution - either the dependency is no longer required, or the command will be in an error state.
	  If *all* inputs are missing then the command should be removed and all outputs deleted.
	- Commands cache state in .dep files in the _temp dir. These are simple text files which should be fast to read (no JSON). A DB might be a better
	  solution (see \todo). .dep files are *not* dependencies themselves.
	- All files (except .dep files) are tracked by the system.
	- Files and commands need to reference each other (by pointer). It is therefore simpler to *never* free file or command instances to avoid the
	  headache of updating all the references. The downside of this is that the memory footprint will grow over time, which is a problem for long-lived
	  instances of DataProcessor.

	\todo
	- Currently files in _raw can only map to 1 command because we reference them by the dep file hash. In theory they could map to one command per rule
	  instead (separate queue per rule).
	- Replace .dep files with a DB to eliminate file access overhead (faster startup).
*/
#pragma once

#include <frm/core/def.h>

#include <apt/FileSystem.h>
#include <apt/Pool.h>
#include <apt/String.h>
#include <apt/StringHash.h>
#include <apt/Time.h>

#include <EASTL/vector.h>
#include <EASTL/hash_map.h>

class Model;

class DataProcessor
{
public:
	struct File;
	struct Rule;
	struct Command;

	enum RootType_
	{
		Root_Raw,
		Root_Temp,
		Root_Bin,

		Root_Count
	};
	typedef int RootType;

	struct File
	{
		RootType         m_root;
		apt::String<255> m_path;             // full path (including root)
		apt::String<64>  m_name;             // name (excluding path and extension)
		apt::String<32>  m_extension;        // extension (e.g. .model.json)
		apt::DateTime    m_timeLastModified;
	};

	struct Rule
	{
		apt::String<32> m_name;
		apt::String<32> m_pattern; 
		int             m_version                      = -1;
		bool            (*OnInit)()                    = nullptr;  // called once on init after the file scan is complete
		bool            (*OnModify)(File* _file_)      = nullptr;  // called once when a matching file is created or modified
		bool            (*OnDelete)(File* _file_)      = nullptr;  // called once when a matching file is deleted
		void            CreateCommand(File* _file_);
	};


	enum CommandState
	{
		CommandState_Ok,       // Execution succeeded, nothing to do.
		CommandState_Warning,  // Execution succeeded but with a warning.
		CommandState_Error,    // Execution failed.
		CommandState_Dirty,    // Requires execution.

		CommandState_Count
	};
	typedef apt::uint8 CommandState;

	struct Command
	{
		Rule*                          m_rule;
		apt::DateTime                  m_timeLastExecuted;
		CommandState                   m_state;
		apt::PathStr                   m_depPath;
		eastl::vector<apt::StringHash> m_inputs;
		eastl::vector<apt::StringHash> m_outputs;
	};

	static bool ProcessModel(File* _file_, apt::FileSystem::FileAction _action);

	DataProcessor();
	~DataProcessor();

	File* findFile(apt::StringHash _pathHash);

private:
	static DataProcessor* s_inst; // \hack static callbacks need to access an instance

	static void DispatchNotifications(const char* _path, apt::FileSystem::FileAction _action) { s_inst->dispatchNotifications(_path, _action); }
	
	struct NullHash 
	{
		apt::StringHash operator()(apt::StringHash _val) const { return _val; } 
	};
	template <typename K, typename V>
	struct HashMap: public eastl::hash_map<K, V, NullHash>
	{
	};

	eastl::vector<Rule*>               m_rules;

	apt::Pool<File>                    m_filePool;
	HashMap<apt::StringHash, File*>    m_files;            // key is the path hash
	size_t                             m_fileCount = 0;

	apt::Pool<Command>                 m_commandPool;
	HashMap<apt::StringHash, Command*> m_commands;
	size_t                             m_commandCount = 0; // key is the dep file path hash
	eastl::vector<Command*>            m_commandQueue;
	

	void         dispatchNotifications(const char* _path, apt::FileSystem::FileAction _action);

	File*        findOrAddFile(const char* _path);
	void         addCommand(Command* _command);

	Rule*        findRule(const char* _name);
	void         createCommands(const File* _file_, apt::StringHash _pathHash);

	void         readDepFile(Command* command_);
	void         writeDepFile(const Command* _command);
	apt::PathStr getDepFilePath(const char* _rawPath);

	void         initRules();
	void         scanTemp();
	void         scanRaw();
};