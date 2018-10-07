/*	Simple version of a data processing system.
	- File objects are allocated once and are never deleted. Files are identified by a hash of the full path (NB could use a simpler index system like MC).
	- File actions (detected by the file watcher) are dispatched to matching Rules which generate Commands.
	- Commands depend on files: some are input dependencies (build when the input changes), some are output dependencies (build when the output is missing).
	  Every file in _raw/ maps to a command, however commands may have many input/output dependencies.
	- Dependencies are stored in a .dep file in the _temp dir.

	On init:
	- Scan the _temp dir for .dep files, generate commands.
	- Scan the _raw/_bin dirs, generate file instances. Files in _raw should generate commands (if they weren't already generated from the .dep scan). 
	- Loop over all Command instances and check the state of the dependencies.

	\todo
	- Currently files in _raw can only map to 1 command because we reference them by the dep file hash. In theory they could map to one command per rule
	  instead (separate queue per rule).
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

	struct File
	{
		apt::String<255> m_path;             // full path
		apt::String<64>  m_name;             // name
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

	struct Command
	{
		Rule*                          m_rule;
		apt::DateTime                  m_timeLastExecuted;
		bool                           m_isDirty;
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