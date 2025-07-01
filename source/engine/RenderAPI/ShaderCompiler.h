#pragma once

#include "Core/Types.h"
#include "Types.h"
#include "Device.h"

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

            inline bool operator ==(const CompileMacroDefinition& other) const
            {
                return macro == other.macro
                    && value == other.value;
            }

            inline bool operator!=(const CompileMacroDefinition& other) const { return !(*this == other); }
        };

        struct CompilationOptions
        {
            bool generateDebugInfo;
            char entryPoint[32];
            Mist::tDynArray<CompileMacroDefinition> macroDefinitionArray;

            CompilationOptions()
                : generateDebugInfo(true), entryPoint{"main"}
            { }

            void PushMacroDefinition(const char* macro)
            {
                macroDefinitionArray.push_back(CompileMacroDefinition(macro));
            }
            void PushMacroDefinition(const char* macro, const char* value)
            {
                macroDefinitionArray.push_back(CompileMacroDefinition(macro, value));
            }
            void PushMacroDefinition(const char* macro, int value)
            {
                macroDefinitionArray.push_back(CompileMacroDefinition(macro, value));
            }
            void PushMacroDefinition(const char* macro, float value)
            {
                macroDefinitionArray.push_back(CompileMacroDefinition(macro, value));
            }

            inline bool operator ==(const CompilationOptions& other) const
            {
                return !strcmp(entryPoint, other.entryPoint)
                    && render::utils::EqualArrays(macroDefinitionArray.data(), (uint32_t)macroDefinitionArray.size(),
                        other.macroDefinitionArray.data(), (uint32_t)other.macroDefinitionArray.size());
            }

            inline bool operator!=(const CompilationOptions& other) const { return !(*this == other); }
        };

        enum AttributeType
        {
			AttributeType_Unknown,
			AttributeType_Boolean,
			AttributeType_SByte,
			AttributeType_UByte,
			AttributeType_Short,
			AttributeType_UShort,
			AttributeType_Int,
			AttributeType_UInt,
			AttributeType_Int64,
			AttributeType_UInt64,
			AttributeType_AtomicCounter,
			AttributeType_Half,
			AttributeType_Float,
			AttributeType_Double,
        };

        struct VertexInputAttribute
        {
            AttributeType type = AttributeType_Unknown;
            size_t size = 0;
            size_t count = 0;
            size_t location = 0;
        };

        struct VertexInputLayout
        {
            Mist::tDynArray<VertexInputAttribute> attributes;
        };

        struct ShaderPropertyDescription
        {
            ResourceType type = ResourceType_None;
            uint64_t size = 0;
            uint32_t binding = 0;
            uint32_t arrayCount = 0;
            ShaderType stage = ShaderType_None;
            std::string name;
        };

        struct ShaderPropertySetDescription
        {
            Mist::tDynArray<ShaderPropertyDescription> params;
            uint32_t setIndex = 0;
        };

        struct ShaderPushConstantDescription
        {
            std::string name;
            uint32_t offset = 0;
            uint32_t size = 0;
            ShaderType stage = ShaderType_None;
        };

        struct ShaderReflectionProperties
        {
            Mist::tDynArray<ShaderPropertySetDescription> params;
            Mist::tMap<ShaderType, ShaderPushConstantDescription> pushConstantMap;
            VertexInputLayout inputLayout;
        };

        CompiledBinary Compile(const char* filepath, ShaderType shaderType, const CompilationOptions* additionalOptions = nullptr);
        void FreeBinary(CompiledBinary& binary);

        CompiledBinary BuildShader(const char* filepath, ShaderType type, const CompilationOptions* additionalOptions = nullptr, bool forceCompilation = false);
        bool BuildShaderParams(const CompiledBinary& bin, ShaderType type, ShaderReflectionProperties& outProperties);
    }
}