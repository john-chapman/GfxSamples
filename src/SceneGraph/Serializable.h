#pragma once

#include <apt/log.h>
#include <apt/Serializer.h>
#include <apt/String.h>
#include <apt/StringHash.h>

template <typename tType>
class Serializable
{
	static const char*           kClassName;
	static const apt::StringHash kClassNameHash;
	static const int             kClassVersion;

protected:

	static bool SerializeAndValidateClassName(apt::Serializer& _serializer_)
	{
		String<32> className = kClassName;
		if (_serializer_.getMode() == apt::Serializer::Mode_Read) 
		{
			if (!_serializer_.value(className, "_class")) 
			{
				APT_LOG_ERR("Failed to serialize _class (%s)", kClassName);
				return false;
			}
			if (className != kClassName) 
			{
				APT_LOG_ERR("Invalid _class, expected %s but found %s", kClassName, className.c_str());
				return false;
			}

			return true;

		} else 
		{
			return _serializer_.value(className, "_class");
		}
	}

	static bool SerializeAndValidateClassVersion(apt::Serializer& _serializer_)
	{
		int classVersion = kClassVersion;
		if (_serializer_.getMode() == apt::Serializer::Mode_Read)
		{
			if (!_serializer_.value(classVersion, "_version")) 
			{
				APT_LOG_ERR("Failed to serialize _version (%s)", kClassName);
				return false;
			}
			if (classVersion != kClassVersion) 
			{
				APT_LOG_ERR("Invalue _version, expected %d but found %d (%s)", kClassVersion, classVersion, kClassName);
				return false;
			}
			return true;

		} else 
		{
			return _serializer_.value(classVersion, "_version");
		}
	}

public:
	
	static const char*      GetClassName()     { return kClassName; }
	static apt::StringHash  GetClassNameHash() { return kClassNameHash; }
	static int              GetClassVersion()  { return kClassVersion; }
};

#define SERIALIZABLE_DEFINE(_class, _version) \
	const char* Serializable<_class>::kClassName = #_class; \
	const apt::StringHash Serializable<_class>::kClassNameHash = apt::StringHash(#_class); \
	const int Serializable<_class>::kClassVersion = _version