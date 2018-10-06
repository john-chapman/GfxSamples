#include "DataProcessor.h"

#include "Material.h"
#include "Model.h"
#include "Scene.h"

#include <apt/StringHash.h>
#include <apt/TextParser.h>

using namespace frm;
using namespace apt;

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
	m_commandQueue.reserve(4096);
	s_inst = this;
	FileSystem::BeginNotifications("SceneGraph/_raw", &DispatchNotifications);
	FileSystem::BeginNotifications("SceneGraph/_bin", &DispatchNotifications);
}

DataProcessor::~DataProcessor()
{
	FileSystem::EndNotifications("SceneGraph/_raw");
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
	auto it = m_files.find(pathHash);
	if (it == m_files.end()) 
	{
		ret = m_filePool.alloc();
		ret->m_path = _path;
		ret->m_name = FileSystem::GetFileName(_path).c_str();
		ret->m_extension = FileSystem::GetExtension(_path).c_str(); // \todo probably doesn't work, need to get string from the first '.'
		m_files[pathHash] = ret;
		++m_fileCount;

	} else 
	{
		ret = it->second;
	}

	return ret;
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

	while (ReadLine())
	{
		switch (lineType)
		{
			case 'R':
				APT_VERIFY(command_->m_rule = findRule(line.c_str()));
				break;
			case 'T':
				//command_->m_timeLastExecuted.fromTime(line.c_str()); // \todo
				break;
			case 'I':
				command_->m_inputs.push_back(StringHash(line.c_str()));
			case 'O':
				command_->m_outputs.push_back(StringHash(line.c_str()));
			default:
				break;
		};
	}
}

void DataProcessor::writeDepFile(const Command* _command)
{
	String<256> dep;

 // rule name
	dep.appendf("R %s\n", _command->m_rule->m_name.c_str());

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
