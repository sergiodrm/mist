#include "ShaderCompiler.h"
#include "Utils/FileSystem.h"


#include "shaderc/shaderc.hpp"
#include "Core/Logger.h"

namespace render
{
    namespace shader_compiler
    {
        class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
        {
        public:
            ShaderIncluder() = default;
            virtual ~ShaderIncluder() = default;

            virtual shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource, size_t includeDepth) override
            {
                shaderc_include_result* result = _new shaderc_include_result;

                Mist::cAssetPath path(requestedSource);
                char* content = nullptr;
                size_t contentSize;
                check(Mist::FileSystem::ReadFile(path.c_str(), &content, contentSize));
                check(content);
                result->content = content;
                result->content_length = contentSize;
                result->user_data = nullptr;
                result->source_name_length = path.GetSize() + 1;
                char* sourceName = _new char[result->source_name_length];
                strcpy_s(sourceName, result->source_name_length, path.c_str());
                result->source_name = sourceName;
                logfok("Read include shader: %s (%d bytes) from %s\n", sourceName, result->source_name_length, requestingSource);
                return result;
            }

            virtual void ReleaseInclude(shaderc_include_result* data) override
            {
                if (data)
                {
                    delete data->content;
                    delete data->source_name;
                    delete data;
                }
            }
        };

        const char* GetShaderStatusStr(shaderc_compilation_status status)
        {
            switch (status)
            {
            case shaderc_compilation_status_success: return "success";
            case shaderc_compilation_status_invalid_stage: return "invalid_stage";
            case shaderc_compilation_status_compilation_error: return "compilation_error";
            case shaderc_compilation_status_internal_error: return "internal_error";
            case shaderc_compilation_status_null_result_object: return "null_result_object";
            case shaderc_compilation_status_invalid_assembly: return "invalid_assembly";
            case shaderc_compilation_status_validation_error: return "validation_error";
            case shaderc_compilation_status_transformation_error: return "transformation_error";
            case shaderc_compilation_status_configuration_error: return "configurarion_error";
            }
            return "unknown";
        }

        template<typename T>
        bool HandleError(const shaderc::CompilationResult<T>& res, const char* compilationStage)
        {
            bool ret = true;
            shaderc_compilation_status status = res.GetCompilationStatus();
            Mist::LogLevel level = status == shaderc_compilation_status_success ? Mist::LogLevel::Ok : Mist::LogLevel::Error;
            if (status != shaderc_compilation_status_success)
            {
                logferror("=== Shader compilation error ===\n%s\n\n", res.GetErrorMessage().c_str());
                ret = false;
            }
            Mist::Logf(level, "-- Shader %s result (%s): %d warnings, %d errors --\n", compilationStage, GetShaderStatusStr(status), res.GetNumWarnings(), res.GetNumErrors());
            return ret;
        }

        static shaderc_shader_kind GetShaderType(ShaderType type)
        {
            switch (type)
            {
            case ShaderType_Vertex: return shaderc_glsl_vertex_shader;
            case ShaderType_Fragment: return shaderc_glsl_fragment_shader;
            case ShaderType_Compute: return shaderc_glsl_compute_shader;
            default: check(false); return shaderc_glsl_infer_from_source;
            }
        }

        CompiledBinary Compile(const char* filepath, ShaderType shaderType, const CompilationOptions* additionalOptions)
        {
            char* source;
            size_t s;
            Mist::cAssetPath path(filepath);
            check(Mist::FileSystem::ReadFile(path, &source, s));

            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            const char* entryPoint = "main";

            options.SetIncluder(std::make_unique<ShaderIncluder>());

#if 0
            if (additionalOptions)
            {
                for (uint32_t i = 0; i < (size_t)additionalOptions->MacroDefinitionArray.size(); ++i)
                {
                    check(additionalOptions->MacroDefinitionArray[i].Macro[0]);
                    if (additionalOptions->MacroDefinitionArray[i].Value[0])
                        options.AddMacroDefinition(additionalOptions->MacroDefinitionArray[i].Macro.CStr(), additionalOptions->MacroDefinitionArray[i].Value.CStr());
                    else
                        options.AddMacroDefinition(additionalOptions->MacroDefinitionArray[i].Macro.CStr());
                }
                if (additionalOptions->GenerateDebugInfo)
                    options.SetGenerateDebugInfo();
                if (*additionalOptions->EntryPointName)
                    entryPoint = compileOptions->EntryPointName;
        }
            else
#endif // 0
            {
                options.SetGenerateDebugInfo();
                //options.SetOptimizationLevel(shaderc_optimization_level_size);
            }

            shaderc_shader_kind kind = GetShaderType(shaderType);
            shaderc::PreprocessedSourceCompilationResult prepRes = compiler.PreprocessGlsl(source, s, kind, path, options);
            if (!HandleError(prepRes, "preprocess"))
                return CompiledBinary();

            Mist::tString preprocessSource{ prepRes.cbegin(), prepRes.cend() };
#ifdef SHADER_DUMP_PREPROCESS_RESULT
            Mist::Logf(Mist::LogLevel::Debug, "Preprocessed source:\n\n%s\n\n", preprocessSource.c_str());
#endif
            shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(preprocessSource.c_str(), preprocessSource.size(), kind, path, entryPoint, options);
            if (!HandleError(result, "assembly"))
                return CompiledBinary();
            Mist::tString assemble{ result.begin(), result.end() };
            shaderc::SpvCompilationResult spv = compiler.AssembleToSpv(assemble.c_str(), assemble.size());
            if (!HandleError(spv, "assembly to spv"))
                return CompiledBinary();

            CompiledBinary bin;
            bin.binaryCount = (spv.cend() - spv.cbegin());
            bin.binary = (uint32_t*)malloc(bin.binaryCount * sizeof(uint32_t));
            memcpy_s(bin.binary, bin.binaryCount * sizeof(uint32_t), spv.cbegin(), bin.binaryCount * sizeof(uint32_t));
            return bin;
        }

        void FreeBinary(CompiledBinary& binary)
        {
            if (binary.binary)
            {
                delete[] binary.binary;
                binary.binary = nullptr;
                binary.binaryCount = 0;
            }
        }
    }
}