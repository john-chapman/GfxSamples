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

namespace Rule_texture
{

bool OnInit()
{
}

bool OnModify(DataProcessor::File* _file_)
{
	return true;
}

bool OnDelete(DataProcessor::File* _file_)
{
	return true;
}


} // namespace Rule_texture

/*******************************************************************************

                                DataProcessor

*******************************************************************************/

// PUBLIC

bool DataProcessor::ProcessModel(File* _file_, FileSystem::FileAction _action)
{
	switch (_action)
	{
		case FileSystem::FileAction_Created:
		case FileSystem::FileAction_Modified:
			break;
		case FileSystem::FileAction_Deleted:
			break;
		default:
			break;
	};

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

		if (command->m_isDirty)
		{
		 // command already dirty (e.g. rule version changed)
			m_commandQueue.push_back(command);

		} else
		{
			for (auto input : command->m_inputs)
			{
				auto f = m_files.find(input);
				APT_ASSERT(f != m_files.end()); // \todo if all inputs are missing then remove the command, if some inputs are missing mark as error
				auto file = f->second;

			 // dirty if input is newer
				if (file->m_timeLastModified > command->m_timeLastExecuted)
				{
					command->m_isDirty = true;
					break;
				}
			}

			for (auto output : command->m_outputs)
			{
				auto f = m_files.find(output);
		 
			 // dirty if output is missing
				if (f == m_files.end())
				{
					command->m_isDirty = true;
					break;
				}

				auto file = f->second;
			
			 // dirty if output is older
				if (file->m_timeLastModified < command->m_timeLastExecuted)
				{
					command->m_isDirty = true;
					break;
				}
			}

			if (command->m_isDirty)
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
	File* f = findOrAddFile(_path);
	
	if (f->m_extension == ".model") 
	{
		ProcessModel(f, _action);
		return;
	}
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
		ret->m_path             = _path;
		ret->m_name             = FileSystem::GetFileName(_path).c_str();
		ret->m_extension        = FileSystem::FindExtension(_path);
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
				command_->m_inputs.push_back(StringHash(line.c_str()));
			case 'O':
				command_->m_outputs.push_back(StringHash(line.c_str()));
			default:
				break;
		};
	}
	command_->m_isDirty = ruleVersion < command_->m_rule->m_version;
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
	for (auto pathHash : _command->m_inputs)
	{
		auto f = findFile(pathHash);
		APT_ASSERT(f);
		dep.appendf("I %s\n", f->m_path.c_str());
	}
	 
 // outputs
	for (auto pathHash : _command->m_outputs)
	{
		auto f = findFile(pathHash);
		APT_ASSERT(f);
		dep.appendf("O %s\n", f->m_path.c_str());
	}

	apt::File f;
	f.setData(dep.c_str(), dep.getLength());
	apt::File::Write(f, _command->m_depPath.c_str());
}

PathStr DataProcessor::getDepFilePath(const char* _rawPath)
{
	return PathStr("_temp/%s.dep", _rawPath + sizeof("_raw/"));
}

void DataProcessor::initRules()
{
	{	auto rule       = new Rule;
		rule->m_name    = "texture";
		rule->m_pattern = "*.texture.*";
		rule->m_version = 0;
		rule->OnInit    = &Rule_texture::OnInit;
		rule->OnModify  = &Rule_texture::OnModify;
		rule->OnDelete  = &Rule_texture::OnDelete;

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
	APT_VERIFY(FileSystem::ListFiles(fileList.data(), (int)fileList.size(), "_temp/", { "*.dep" }, true) < (int)fileList.size());

	for (auto& path : fileList)
	{
		StringHash pathHash(path.c_str());
		Command* command = m_commandPool.alloc();
		command->m_depPath = path;
		addCommand(command);
		readDepFile(command);
	}
}


void DataProcessor::scanRaw()
{
	eastl::vector<PathStr> fileList;
	fileList.resize(4096);
	APT_VERIFY(FileSystem::ListFiles(fileList.data(), (int)fileList.size(), "_raw/", { "*.*" }, true) < (int)fileList.size());

	for (auto& path : fileList)
	{
		StringHash pathHash(path.c_str());
		File* file = findOrAddFile(path.c_str());
	}
}