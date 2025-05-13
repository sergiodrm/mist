#pragma once

#include "Core/Types.h"
#include "Types.h"

namespace render
{
    namespace shader_compiler
    {
        struct CompiledBinary
        {
            uint32_t* binary = nullptr;
            size_t binaryCount = 0;
            inline bool IsCompilationSucceed() const { return binary && binaryCount; }
            static CompiledBinary FailureBinary() { return { nullptr, 0 }; }
        };

        struct CompileMacroDefinition
        {
            Mist::tFixedString<64> macro;
            Mist::tFixedString<64> value;
            CompileMacroDefinition()
            {
                macro[0] = 0;
                value[0] = 0;
            }
            CompileMacroDefinition(const char* str)
            {
                macro.Set(str);
                value[0] = 0;
            }
            CompileMacroDefinition(const char* _macro, const char* _value)
            {
                macro.Set(_macro);
                value.Set(_value);
            }
            CompileMacroDefinition(const char* _macro, int _value)
            {
                macro.Set(_macro);
                value.Set(std::to_string(_value).c_str());
            }
            CompileMacroDefinition(const char* _macro, float _value)
            {
                macro.Set(_macro);
                value.Set(std::to_string(_value).c_str());
            }
        };

        struct CompilationOptions
        {
            bool generateDebugInfo;
            char entryPoint[32];
            Mist::tDynArray<CompileMacroDefinition> macroDefinitionArray;

            CompilationOptions()
                : generateDebugInfo(true), entryPoint{"main"}
            { }
        };

        CompiledBinary Compile(const char* filepath, ShaderType shaderType, const CompilationOptions* additionalOptions = nullptr);
        void FreeBinary(CompiledBinary& binary);

        CompiledBinary BuildShader(const char* filepath, ShaderType type, const CompilationOptions* additionalOptions = nullptr);
    }
}