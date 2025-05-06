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
        };

        struct CompilationOptions
        {

        };

        CompiledBinary Compile(const char* filepath, ShaderType shaderType, const CompilationOptions* additionalOptions = nullptr);
        void FreeBinary(CompiledBinary& binary);
    }
}