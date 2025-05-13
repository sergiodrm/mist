#include "ShaderCompiler.h"
#include "Utils/FileSystem.h"


#include "shaderc/shaderc.hpp"
#include "Core/Logger.h"
#include "Utils/TimeUtils.h"

#define SHADER_BINARY_FILE_DIRECTORY "shaderbin"
#define SHADER_BINARY_FILE_EXTENSION ".mist.spv"

//#define SHADER_DUMP_INFO
#ifdef SHADER_DUMP_INFO
#define shaderlabel "[shaders] "
#define shaderlog(fmt) loginfo(shaderlabel fmt)
#define shaderlogf(fmt, ...) logfinfo(shaderlabel fmt, __VA_ARGS__)
#else
#define shaderlog(fmt) DUMMY_MACRO
#define shaderlogf(fmt, ...) DUMMY_MACRO
#endif

Mist::CIntVar CVar_ForceShaderRecompilation("r_forceShaderCompilation", 0);


namespace render
{
    namespace shader_compiler
    {
        template <uint32_t Size>
        void GenerateSpvFileName(char(&outFilepath)[Size], const char* srcFile, const CompilationOptions& options)
        {
            sprintf_s(outFilepath, Size, "%s.[%s]", srcFile, options.entryPoint);
            if (!options.macroDefinitionArray.empty())
            {
                strcat_s(outFilepath, Size, ".[");
                for (uint32_t i = 0; i < (uint32_t)options.macroDefinitionArray.size(); ++i)
                {
                    char buff[256];
                    sprintf_s(buff, "%s(%s).", options.macroDefinitionArray[i].macro.CStr(), options.macroDefinitionArray[i].value.CStr());
                    strcat_s(outFilepath, Size, buff);
                }
                strcat_s(outFilepath, Size, "]");
            }

            size_t h = std::hash<std::string>()(outFilepath);
            Mist::FileSystem::GetDirectoryFromFilepath(srcFile, outFilepath, Size);
            sprintf_s(outFilepath, "%s%s/%llu%s", outFilepath, SHADER_BINARY_FILE_DIRECTORY, h, SHADER_BINARY_FILE_EXTENSION);

            //strcat_s(outFilepath, Size, SHADER_BINARY_FILE_EXTENSION);
        }

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

        bool CheckShaderFileExtension(const char* filepath, ShaderType type)
        {
            // TODO
            return true;
        }

        bool ReadSpvBinaryFromFile(const char* binaryFilepath, uint32_t** binaryData, size_t* binaryCount)
        {
            check(binaryFilepath && *binaryFilepath && binaryData && binaryCount);
            //PROFILE_SCOPE_LOG(SpvShader, "Read spv binary from file");
            // Binary file is created after last file modification, valid binary
            FILE* f;
            errno_t err = fopen_s(&f, binaryFilepath, "rb");
            if (err || !f)
            {
                logferror("Failed to open file: %s\n", binaryFilepath);
                check(false && "Failed to open shader compiled file");
            }
            fseek(f, 0L, SEEK_END);
            size_t numbytes = ftell(f);
            fseek(f, 0L, SEEK_SET);
            *binaryCount = numbytes / sizeof(uint32_t);
            *binaryData = (uint32_t*)malloc(numbytes);
            size_t contentRead = fread_s(*binaryData, numbytes, 1, numbytes, f);
            check(contentRead == numbytes);
            fclose(f);
            return true;
        }

        bool WriteCompiledBinaryToFile(const char* binaryFilepath, uint32_t* binaryData, size_t binaryCount)
        {
            PROFILE_SCOPE_LOGF(WriteCompiledBinaryToFile, "Write file with binary spv shader %s", binaryFilepath);
            check(binaryFilepath && *binaryFilepath && binaryData && binaryCount);
            char dir[256];
            Mist::FileSystem::GetDirectoryFromFilepath(binaryFilepath, dir, Mist::CountOf(dir));
            Mist::FileSystem::Mkdir(dir);
            FILE* f;
            errno_t err = fopen_s(&f, binaryFilepath, "wb");
            check(!err && f && "Failed to write shader compiled file");
            size_t writtenCount = fwrite(binaryData, sizeof(uint32_t), binaryCount, f);
            check(writtenCount == binaryCount);
            fclose(f);
            shaderlogf("Shader binary compiled written: %s [%u bytes]\n", binaryFilepath, writtenCount * sizeof(uint32_t));
            return true;
        }

        bool ContainsNewerFileInIncludes_Recursive(const char* filepath, const CompilationOptions* options)
        {
            Mist::cAssetPath assetPath(filepath);
            char binaryFilepath[1024];
            GenerateSpvFileName(binaryFilepath, assetPath, *options);

            if (Mist::FileSystem::IsFileNewerThanOther(assetPath, binaryFilepath))
                return true;

            size_t contentSize;
            char* content;
            check(Mist::FileSystem::ReadTextFile(assetPath, &content, contentSize));
            char* it = content;
            bool containsNewerFile = false;
            while (it = strstr(it, "#include"))
            {
                // #include length
                size_t tokenLength = 8;
                it += tokenLength;
                // After include always at least one space
                check(*it == ' ');
                // Looking for open token " or <
                while (*it == ' ') ++it;
                check((*it == '"' || *it == '<') && "Unexpected token. Expected '\"' or '<' after include preprocessor instruction.");
                // Determinate matching close token
                char closeToken = (*it == '"') ? '"' : '>';
                ++it;
                // After open token we have the path
                char* dependencyPath = it;
                // Find close token
                while (*it && *it != '\r' && *it != '\n' && *it != closeToken) ++it;
                check(*it == closeToken && "Unexpected close token. Expected '\"' or '>' to close include preprocessor instruction.");
                // In close token, terminate string dependencyPath
                *it = 0;
                ++it;
                // Build complete asset path and register it.
                // Keep building dependencies inside current dependency.
                if (ContainsNewerFileInIncludes_Recursive(dependencyPath, options))
                {
                    containsNewerFile = true;
                    logfwarn("Shader dependency newer than compiled binary (%s)\n", dependencyPath);
                    break;
                }
            }
            delete[] content;
            return containsNewerFile;
        }

        bool ShouldRecompileShaderFile(const char* filepath, const CompilationOptions* compileOptions)
        {
            check(filepath && *filepath && compileOptions);
            Mist::cAssetPath assetPath(filepath);
            PROFILE_SCOPE_LOGF(ShouldRecompileShaderFile, "Build shader dependency (%s)", assetPath);

            if (CVar_ForceShaderRecompilation.Get())
                return true;

            return ContainsNewerFileInIncludes_Recursive(filepath, compileOptions);
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

        CompiledBinary BuildShader(const char* filepath, ShaderType type, const CompilationOptions* additionalOptions)
        {
            Mist::cAssetPath assetPath(filepath);
            PROFILE_SCOPE_LOGF(ProcessShaderFile, "Shader file process (%s)", assetPath);
            shaderlogf("Compiling shader: [%s]\n", assetPath);

            CompilationOptions defaultOptions;
            if (!additionalOptions)
                additionalOptions = &defaultOptions;

            check(!strcmp(additionalOptions->entryPoint, "main") && "Set shader entry point not supported yet.");
            char binaryFilepath[1024];
            GenerateSpvFileName(binaryFilepath, assetPath, *additionalOptions);

            check(CheckShaderFileExtension(assetPath, type));

            CompiledBinary bin;
            if (!ShouldRecompileShaderFile(assetPath, additionalOptions))
            {
                shaderlogf("Loading shader binary from compiled file: %s.spv\n", filepath);
                check(ReadSpvBinaryFromFile(binaryFilepath, &bin.binary, &bin.binaryCount));
            }
            else
            {
                PROFILE_SCOPE_LOG(ShaderCompilation, "Compile shader");
                if (CVar_ForceShaderRecompilation.Get())
                    logfwarn("Force shader recompilation: %s\n", assetPath);
                else
                    logfwarn("Compiled binary not found or shader source is newer (%s)\n", assetPath);
                bin = shader_compiler::Compile(assetPath, type, additionalOptions);
                if (!bin.IsCompilationSucceed())
                {
                    logferror("Shader compilation failed (%s)\n", filepath);
                    if (Mist::FileSystem::FileExists(binaryFilepath))
                    {
                        // Try to load the last compiled binary.
                        logferror("Loading last compiled binary: %s\n", binaryFilepath);
                        if (!ReadSpvBinaryFromFile(binaryFilepath, &bin.binary, &bin.binaryCount))
                        {
                            logferror("Failed to load last compiled binary: %s\n", binaryFilepath);
                            return CompiledBinary::FailureBinary();
                        }
                    }
                    else
                    {
                        logferror("Compiled binary not found: %s\n", binaryFilepath);
                        return CompiledBinary::FailureBinary();
                    }
                }
                else
                {
                    // if compilation is succeeded, generate a compiled file.
                    check(WriteCompiledBinaryToFile(binaryFilepath, bin.binary, bin.binaryCount));
                }
            }

            // At this point we must have a valid binary.
            check(bin.IsCompilationSucceed());

            shaderlogf("Shader compiled successfully (%s)\n", assetPath);
            return bin;
        }
    }
}