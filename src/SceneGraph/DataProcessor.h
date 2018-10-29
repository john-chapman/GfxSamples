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
	- Creating/modifying/deleting a tracked file basically causes all dependent commands to be marked as dirty and pushed into the command queue. 
	- Queued commands should be sorted by rule (in rule priority order) to easily support batching of commands.


	\todo
	- Currently files in _raw can only map to 1 command because we reference them by the dep file hash. In theory they could map to one command per rule
	  instead (separate queue per rule).
	- Replace .dep files with a DB to eliminate file access overhead (faster startup).

	Use Cases
	=========

	Startup:
		- Init rules.
		- Scan _temp for .dep files (or read DB), generate commands and files. Don't update the file status here, just init the object.
		- Scan roots for all other files, update file status (attributes etc). Push any new files from _raw into a temporary 'new' list.
		- For each file in the 'new' list, add new commands.
		- For each command, push into the queue if dirty.

	File created:
		- Find/add the file and init it's status.
		- If from _raw then find or add command, mark as dirty and push into the command queue.

	File modified:
		- Mark all input-dependent commands as dirty and push into the command queue.

	File deleted:
		- Update all input-dependent commands, mark as dirty and push into the command queue.
		- Update all output-dependent commands, mark as dirty and push into the command queue.
		- If it's a .dep file, mark as dirty and push into the command queue.

	Command execution:
		- Check that input files aren't marked as missing - if *all* are missing then clean the command, else mark as error.

	Command clean:
		- Delete outputs from disk.

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

	enum FileState_
	{
		FileState_Ok,
		FileState_Missing,

		FileState_Count
	};
	typedef int FileState;
	
	enum CommandState_
	{
		CommandState_Ok,       // Execution succeeded.
		CommandState_Warning,  // Execution succeeded but with a warning.
		CommandState_Error,    // Execution failed.
		CommandState_Dirty,    // Requires execution.

		CommandState_Count
	};
	typedef int CommandState;

	struct Path: public apt::String<255>
	{
		Path(): apt::String<255>() {}
		Path(const char* _path);
		Path(RootType _root, const char* _path, const char* _name, const char* _type, const char* _extension);

		apt::String<64> getRoot() const;
		apt::String<64> getPath() const;
		apt::String<64> getName() const;
		apt::String<16> getType() const;
		apt::String<16> getExtension() const;

		apt::uint8 m_pathOffset      = ~0;  // Start of the path (first char after the root).
		apt::uint8 m_nameOffset      = ~0;  // File name.
		apt::uint8 m_typeOffset      = ~0;  // 'Type' part of the extension.
		apt::uint8 m_extensionOffset = ~0;  // Extension.
	};

	struct File
	{
		RootType                m_root;
		Path                    m_path;
		apt::DateTime           m_timeLastModified;
		FileState               m_state;
		
		eastl::vector<Command*> m_commandsIn;       // Commands which reference this file as an input.
		eastl::vector<Command*> m_commandsOut;      // Commands which reference this file as an output (should only be 1).
	};

	struct Rule
	{
		apt::String<32> m_name;
		apt::String<32> m_pattern;
		int             m_version                                        = -1;
		void           (*Init)   (Command* _commandList_, size_t _count) = nullptr;
		void           (*Execute)(Command* _commandList_, size_t _count) = nullptr;
	};

	struct Command
	{
		Rule*                m_rule;
		apt::DateTime        m_timeLastExecuted;
		CommandState         m_state;
		Path                 m_depPath;
		eastl::vector<File*> m_filesIn;            // Input files.
		eastl::vector<File*> m_filesOut;           // Output files.
	};

	static bool NeedsClean(const Command& _command);

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
	Path         getDepFilePath(const char* _rawPath);

	void         initRules();
	void         scanTemp();
	void         scanRaw();
};