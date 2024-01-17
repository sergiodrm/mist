// src file for vkmmc project 
#include "Debug.h"
#include <stacktrace>

void vkmmc_debug::PrintCallstack(size_t count, size_t offset)
{
	vkmmc::Log(vkmmc::LogLevel::Debug, "==========================================\n");
	vkmmc::Log(vkmmc::LogLevel::Debug, "\tCALLSTACK\n");
	vkmmc::Log(vkmmc::LogLevel::Debug, "==========================================\n");
	std::stacktrace trace = std::stacktrace::current();
	for (const std::stacktrace_entry& entry : trace)
	{
		if (*entry.description().c_str())
			vkmmc::Logf(vkmmc::LogLevel::Debug, "> %s (%zd)\n", entry.description().c_str(), entry.source_line());
	}
	vkmmc::Log(vkmmc::LogLevel::Debug, "==========================================\n\n");
}
