#include "DataProcessor.h"

#include "Material.h"
#include "Model.h"
#include "Scene.h"

#include <apt/log.h>
#include <apt/StringHash.h>
#include <apt/TextParser.h>

#include <cstring>

#ifdef APT_DEBUG
	#undef APT_STRICT_ASSERT
	#define APT_STRICT_ASSERT(e) APT_ASSERT(e)
#endif

using namespace frm;
using namespace apt;

// apt::FileSystem::FindExtension returns a ptr to the *last* occurrence of '.' but we want the *first*.
static const char* FindExtension(const char* _path)
{
	return strchr(_path, '.');
}

static const char* kRootPaths[DataProcessor::Root_Count] =
{
	"_raw/",   // Root_Raw
	"_temp/",  // Root_Temp
	"_bin/",   // Root_Bin
};

static const char* RemoveRoot(DataProcessor::RootType _root, const char* _path)
{
	const char* root = kRootPaths[_root];
	auto rootLen = strlen(root);
	if (strstr(_path, root))
	{
		_path += rootLen;
	}
	return _path;
}

static PathStr MakePath(DataProcessor::RootType _root, const char* _path)
{
	return PathStr("%s%s", kRootPaths[_root], _path);
}

namespace Rule_texture
{

void Init(DataProcessor::Command* _commandList_, size_t _count)
{
}

void Execute(DataProcessor::Command* _commandList_, size_t _count)
{
	for (size_t i = 0; i < _count; ++i) 
	{
		auto& command = _commandList_[i];

		command.m_timeLastExecuted = Time::GetDateTime();

		APT_ASSERT(command.m_state == DataProcessor::CommandState_Dirty); // \todo executing a non-dirty command?

		if (DataProcessor::NeedsClean(command))
		{
			for (auto fileOut : command.m_filesOut)
			{
				FileSystem::Delete(fileOut->m_path.c_str());
			}
		}
		
		/*auto outPath =
		command.m_filesOut.clear();
		command.m_filesOut.

		command.m_state = DataProcessor::CommandState_Ok;*/
	}
}

} // namespace Rule_texture

/*******************************************************************************

                             DataProcessor::Path

*******************************************************************************/
DataProcessor::Path::Path(const char* _path)
{
	m_pathOffset = 0;
	for (auto rootPath : kRootPaths)
	{
		auto rootLen = strlen(rootPath);
		if (strncmp(rootPath, _path, m_pathOffset) == 0)
		{
			m_pathOffset = rootLen;
			break;
		}
	}

	auto pathEnd = strrchr(_path + m_pathOffset, '/');
	if (pathEnd)
	{
		m_nameOffset = pathEnd - _path + 1;
	} else
	{
		m_pathOffset = ~0;
	}

	auto nameEnd = strchr(_path + m_nameOffset, '.');
	if (nameEnd)
	{
		m_typeOffset = nameEnd - _path + 1;
	}

	auto typeEnd = strchr(_path + m_typeOffset, '.');
	if (typeEnd)
	{
		m_extensionOffset = typeEnd - _path + 1;
	}

	set(_path);
}

DataProcessor::Path::Path(RootType _root, const char* _path, const char* _name, const char* _type, const char* _extension)
{
	m_pathOffset = strlen(kRootPaths[_root]);
	append(kRootPaths[_root]);
	append(_path);
	bool needPathSeparator = _path[strlen(_path) - 1] != '/';
	if (needPathSeparator)
	{
		append("/");
	}
	
	m_nameOffset = m_pathOffset + strlen(_path) + (needPathSeparator ? 1 : 0);
	append(_name);

	if (_type)
	{ 
		m_typeOffset = m_nameOffset + strlen(_name) + 1;
		appendf(".%s", _type);
	}

	if (_extension)
	{
		m_extensionOffset = m_typeOffset + strlen(_type) + 1;
		appendf(".%s", _extension);
	}
}

String<64> DataProcessor::Path::getRoot() const
{
	if (m_pathOffset == 0)
	{
		return "";
	}
	String<64> ret;
	ret.set(begin(), m_pathOffset);
	return ret;
}

String<64> DataProcessor::Path::getPath() const
{
	if (m_pathOffset == ~0)
	{
		return "";
	}
	String<64> ret;
	ret.set(begin() + m_pathOffset, m_nameOffset - m_pathOffset);
	return ret;
}

String<64> DataProcessor::Path::getName() const
{
	if (m_nameOffset == ~0)
	{
		return "";
	}
	String<64> ret;
	ret.set(begin() + m_nameOffset, m_typeOffset - m_nameOffset - 1);
	return ret;
}

String<16> DataProcessor::Path::getType() const
{
	if (m_typeOffset == ~0)
	{
		return "";
	}
	String<16> ret;
	ret.set(begin() + m_typeOffset, m_extensionOffset - m_typeOffset - 1);
	return ret;
}

String<16> DataProcessor::Path::getExtension() const
{
	if (m_extensionOffset == ~0)
	{
		return "";
	}
	String<16> ret;
	ret.set(begin() + m_extensionOffset, getLength() - m_extensionOffset);
	return ret;
}

/*******************************************************************************

                                DataProcessor

*******************************************************************************/

// PUBLIC

bool DataProcessor::NeedsClean(const Command& _command)
{
	for (auto& fileIn : _command.m_filesIn)
	{
		if (fileIn->m_state != FileState_Missing)
		{
			return false;
		}
	}
	return true;
}

DataProcessor::DataProcessor()
	: m_filePool(4096)
	, m_commandPool(4096)
{
	s_inst = this;
	FileSystem::BeginNotifications("SceneGraph/_raw",  &DispatchNotifications);
	FileSystem::BeginNotifications("SceneGraph/_temp", &DispatchNotifications);
	FileSystem::BeginNotifications("SceneGraph/_bin",  &DispatchNotifications);

	initRules();

	{	APT_AUTOTIMER("Scanning _temp");
		scanTemp();
	}
	{	APT_AUTOTIMER("Scanning _raw");
		scanRaw();
	}

	m_commandQueue.reserve(4096);
	for (auto it : m_commands) 
	{
		auto command = it.second;

		if (command->m_state == CommandState_Dirty)
		{
		 // command already dirty (e.g. rule version changed)
			m_commandQueue.push_back(command);

		} else
		{
			for (auto fileIn : command->m_filesIn)
			{
				APT_ASSERT(fileIn->m_state != FileState_Missing); // \todo if all inputs are missing then remove the command, if some inputs are missing mark as error

			 // dirty if input is newer
				if (fileIn->m_timeLastModified > command->m_timeLastExecuted)
				{
					command->m_state = CommandState_Dirty;
					break;
				}
			}

			for (auto fileOut : command->m_filesOut)
			{
			 // dirty if output is missing
				if (fileOut->m_state == FileState_Missing)
				{
					command->m_state = CommandState_Dirty;
					break;
				}
			
			 // dirty if output is older
				if (fileOut->m_timeLastModified < command->m_timeLastExecuted)
				{
					command->m_state = CommandState_Dirty;
					break;
				}
			}

			if (command->m_state == CommandState_Dirty)
			{
				m_commandQueue.push_back(command);
			}
		}
	}
}

DataProcessor::~DataProcessor()
{
	FileSystem::EndNotifications("SceneGraph/_raw");
	FileSystem::EndNotifications("SceneGraph/_temp");
	FileSystem::EndNotifications("SceneGraph/_bin");
}

DataProcessor::File* DataProcessor::findFile(StringHash _pathHash)
{
	auto it = m_files.find(_pathHash);
	if (it != m_files.end()) 
	{
		return it->second;
	}
	return nullptr;
}

// PRIVATE

DataProcessor* DataProcessor::s_inst = nullptr;

void DataProcessor::dispatchNotifications(const char* _path, FileSystem::FileAction _action)
{
	File* file = findOrAddFile(_path);
}

DataProcessor::File* DataProcessor::findOrAddFile(const char* _path)
{
	StringHash pathHash(_path);
	File* ret = nullptr; 
	auto it   = m_files.find(pathHash);
	if (it == m_files.end()) 
	{
		ret                     = m_filePool.alloc();
		m_files[pathHash]       = ret;
		ret->m_path             = Path(_path);
		ret->m_timeLastModified = FileSystem::GetTimeModified(_path);
		++m_fileCount;

	} else 
	{
		ret = it->second;
	}

	return ret;
}

void DataProcessor::addCommand(Command* _command)
{
	auto pathHash = StringHash(_command->m_depPath.c_str());
	APT_STRICT_ASSERT(m_commands.find(pathHash) == m_commands.end());
	m_commands[pathHash] = _command;
	++m_commandCount;
}

DataProcessor::Rule* DataProcessor::findRule(const char* _name)
{
	for (auto rule : m_rules)
	{
		if (rule->m_name == _name)
		{
			return rule;
		}
	}
	return nullptr;
}

void DataProcessor::readDepFile(Command* command_)
{
	apt::File f;
	if (!apt::File::Read(f, command_->m_depPath.c_str())) {
		APT_LOG_ERR("Missing dependency file \"%s\"", command_->m_depPath.c_str());
		return;
	}
	TextParser tp = f.getData();

	PathStr line;
	char    lineType;
	auto ReadLine = [&]()
		{
			if (tp.isNull())
			{
				return false;
			}
			const char* beg = tp;
			tp.skipLine();
			line.set(beg + 2, tp - beg);
			lineType = *beg;
			return true;
		};

	int ruleVersion = -1;
	while (ReadLine())
	{
		switch (lineType)
		{
			case 'R':
				APT_VERIFY(command_->m_rule = findRule(line.c_str()));
				break;
			case 'V':
				ruleVersion = (int)strtol(line.c_str(), nullptr, 0);
				break;
			case 'T':
				command_->m_timeLastExecuted = DateTime(line.c_str());
				break;
			case 'I':
			{
				auto file = findOrAddFile(line.c_str());
				command_->m_filesIn.push_back(file);
				break;
			}
			case 'O':
			{
				auto file = findOrAddFile(line.c_str());
				command_->m_filesOut.push_back(file);
				break;
			}
			default:
				break;
		};
	}
	if (ruleVersion < command_->m_rule->m_version)
	{
		command_->m_state = CommandState_Dirty;
	}
}

void DataProcessor::writeDepFile(const Command* _command)
{
	String<256> dep;

 // rule name
	dep.appendf("R %s\n", _command->m_rule->m_name.c_str());

 // rule version
	dep.appendf("V %d\n", _command->m_rule->m_version);

 // last exec time
	dep.appendf("T %s\n", _command->m_timeLastExecuted.asString());
	
 // inputs
	for (auto fileIn : _command->m_filesIn)
	{
		dep.appendf("I %s\n", fileIn->m_path.c_str());
	}
	 
 // outputs
	for (auto fileOut : _command->m_filesOut)
	{
		dep.appendf("O %s\n", fileOut->m_path.c_str());
	}

	apt::File f;
	f.setData(dep.c_str(), dep.getLength());
	apt::File::Write(f, _command->m_depPath.c_str());
}

DataProcessor::Path DataProcessor::getDepFilePath(const char* _rawPath)
{
	Path ret;
	ret.setf("%s%s.dep", kRootPaths[Root_Temp], RemoveRoot(Root_Raw, _rawPath));
	return ret;
}

void DataProcessor::initRules()
{
	{	auto rule       = new Rule;
		rule->m_name    = "texture";
		rule->m_pattern = "*.texture.*";
		rule->m_version = 0;
		rule->Init      = &Rule_texture::Init;
		rule->Execute   = &Rule_texture::Execute;

		m_rules.push_back(rule);
	}
}

void DataProcessor::createCommands(const File* _file, StringHash _pathHash)
{
	auto it = m_commands.find(_pathHash);
	if (it == m_commands.end())
	{
		for (auto rule : m_rules)
		{
			if (!FileSystem::Matches(rule->m_pattern.c_str(), _file->m_path.c_str()))
			{
				continue;
			}

			Command* command   = m_commandPool.alloc();
			command->m_rule    = rule;
			command->m_depPath = getDepFilePath(_file->m_path.c_str());
// \todo
			addCommand(command);
			break;
		}
	}
}

void DataProcessor::scanTemp()
{
	eastl::vector<PathStr> fileList;
	fileList.resize(4096);
	APT_VERIFY(FileSystem::ListFiles(fileList.data(), (int)fileList.size(), kRootPaths[Root_Temp], { "*.dep" }, true) < (int)fileList.size());

	for (auto& path : fileList)
	{
		Command* command = m_commandPool.alloc();
		command->m_depPath = Path(path.c_str());
		addCommand(command);
		readDepFile(command);
	}
}


void DataProcessor::scanRaw()
{
	eastl::vector<PathStr> fileList;
	fileList.resize(4096);
	APT_VERIFY(FileSystem::ListFiles(fileList.data(), (int)fileList.size(), kRootPaths[Root_Raw], { "*.*" }, true) < (int)fileList.size());

	for (auto& path : fileList)
	{
		StringHash pathHash(path.c_str());
		File* file = findOrAddFile(path.c_str());
	}
}