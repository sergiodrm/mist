#pragma once

#include <unordered_map>
#include <string>

namespace Mist
{
	class CVar
	{
	public:
		enum class CVarType { Int, Float, Bool, String };
	private:
		char Name[64];
		CVarType Type;
	protected:
		union uValue
		{
			int IntValue = 0;
			float FloatValue;
			bool BoolValue;
			char StringValue[256];
		};
		uValue Value;
		uValue DefaultValue;
	public:
		CVar(const char* name, CVarType type);

		inline const char* GetName() const { return Name; }
		inline CVarType GetType() const { return Type; }
		void Reset() { memcpy_s(&Value, sizeof(uValue), &DefaultValue, sizeof(uValue)); }
	};

	class CIntVar : public CVar
	{
	public:
		CIntVar(const char* name, int32_t defaultValue);
		int Get() const { return Value.IntValue; }
		int GetDefault() const { return DefaultValue.IntValue; }
		void Set(int value) { Value.IntValue = value; }
	};

	class CFloatVar : public CVar
	{
	public:
		CFloatVar(const char* name, float defaultValue);
		float Get() const { return Value.FloatValue; }
		float GetDefault() const { return DefaultValue.FloatValue; }
		void Set(float value) { Value.FloatValue = value; }
	};

	class CBoolVar : public CVar
	{
	public:
		CBoolVar(const char* name, bool defaultValue);
		bool Get() const { return Value.BoolValue; }
		bool GetDefault() const { return DefaultValue.BoolValue; }
		void Set(bool value) { Value.BoolValue = value; }
	};

	class CStrVar : public CVar
	{
	public:
		CStrVar(const char* name, const char* defaultValue);
		const char* Get() const { return Value.StringValue; }
		const char* GetDefault() const { return DefaultValue.StringValue; }
		void Set(const char* value) { strcpy_s(Value.StringValue, value); }
	};

	CVar* FindCVar(const char* name);
	bool SetCVar(const char* name, const char* strValue);
	void PrintCVarList();
	bool ExecCommand_CVar(const char* cmd);

	class CmdParser
	{
	public:
		CmdParser() = default;

		bool Parse(int argc, const char* const * argv);
		bool HasOption(const char* key) const;
		bool GetString(const char* key, const char** str) const;
		
	private:
		std::unordered_map<std::string, std::string> m_options;
	};
}
