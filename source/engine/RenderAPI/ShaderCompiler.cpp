#include "ShaderCompiler.h"
#include "Utils/FileSystem.h"


#include "shaderc/shaderc.hpp"
#include "Core/Logger.h"
#include "Utils/TimeUtils.h"
//#include <spirv-cross-main/spirv_glsl.hpp>
#include <spirv_cross/spirv_glsl.hpp>


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
            bool res = false;
            if (filepath && *filepath)
            {
                const char* desiredExt;
                switch (type)
                {
                case ShaderType_Vertex: 
                    desiredExt = ".vert";
                    break;
                case ShaderType_Fragment: 
                    desiredExt = ".frag"; 
                    break;
                case ShaderType_Compute: 
                    desiredExt = ".comp";
                    break;
                default:
                    return res;
                }
                size_t filepathLen = strlen(filepath);
                size_t desiredExtLen = strlen(desiredExt);
                if (filepathLen > desiredExtLen)
                {
                    size_t ext = filepathLen - desiredExtLen;
                    res = !strcmp(&filepath[ext], desiredExt);
                }
            }
            return res;
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

            return ContainsNewerFileInIncludes_Recursive(filepath, compileOptions);
        }

        CompiledBinary Compile(const char* filepath, ShaderType shaderType, const CompilationOptions* additionalOptions)
        {
            PROFILE_SCOPE_LOGF(Compile, "Compile shader (%s)", filepath);
            char* source;
            size_t s;
            Mist::cAssetPath path(filepath);
            check(Mist::FileSystem::ReadFile(path, &source, s));

            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            const char* entryPoint = "main";

            options.SetIncluder(std::make_unique<ShaderIncluder>());

#if 1
            if (additionalOptions)
            {
                for (uint32_t i = 0; i < (uint32_t)additionalOptions->macroDefinitionArray.size(); ++i)
                {
                    check(additionalOptions->macroDefinitionArray[i].macro[0]);
                    if (additionalOptions->macroDefinitionArray[i].value[0])
                        options.AddMacroDefinition(additionalOptions->macroDefinitionArray[i].macro.CStr(), additionalOptions->macroDefinitionArray[i].value.CStr());
                    else
                        options.AddMacroDefinition(additionalOptions->macroDefinitionArray[i].macro.CStr());
                }
                if (additionalOptions->generateDebugInfo)
                    options.SetGenerateDebugInfo();
                if (*additionalOptions->entryPoint)
                    entryPoint = additionalOptions->entryPoint;
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

            Mist::String preprocessSource(prepRes.cbegin());
#ifdef SHADER_DUMP_PREPROCESS_RESULT
            Mist::Logf(Mist::LogLevel::Debug, "Preprocessed source:\n\n%s\n\n", preprocessSource.c_str());
#endif
            shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(preprocessSource.c_str(), preprocessSource.getLength(), kind, path, entryPoint, options);
            if (!HandleError(result, "assembly"))
                return CompiledBinary();
            Mist::String assemble(result.cbegin());
            check(assemble.getLength() + 1 == assemble.getCapacity());
            shaderc::SpvCompilationResult spv = compiler.AssembleToSpv(assemble.c_str(), assemble.getCapacity());
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

        CompiledBinary BuildShader(const char* filepath, ShaderType type, const CompilationOptions* additionalOptions, bool forceCompilation)
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
            if (!forceCompilation && !ShouldRecompileShaderFile(assetPath, additionalOptions))
            {
                shaderlogf("Loading shader binary from compiled file: %s.spv\n", filepath);
                check(ReadSpvBinaryFromFile(binaryFilepath, &bin.binary, &bin.binaryCount));
            }
            else
            {
                PROFILE_SCOPE_LOG(ShaderCompilation, "Compile shader");
                if (forceCompilation)
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

        bool BuildShaderParams(const CompiledBinary& bin, ShaderType stage, ShaderReflectionProperties& outProperties)
        {
            auto processSpirvResource = [](ShaderReflectionProperties& properties, 
                const spirv_cross::CompilerGLSL& compiler, 
                const spirv_cross::Resource& resource, 
                ShaderType stage, 
                ResourceType resourceType)
                {
                    ShaderPropertyDescription bufferInfo;
                    bufferInfo.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

                    // spirv cross get naming heuristic for uniform buffers. 
                    // (https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide#a-note-on-names)
                    // case: layout (set, binding) uniform UBO {} u_ubo;
                    // To get the name of the struct UBO use resource.name.c_str()
                    // To get the name of the instance use compiler.get_name(resource.id)
                    //bufferInfo.Name = resource.name.c_str();
                    bufferInfo.name = compiler.get_name(resource.id);
                    if (resourceType != ResourceType_TextureSRV && resourceType != ResourceType_TextureUAV)
                    {
                        const spirv_cross::SPIRType& bufferType = compiler.get_type(resource.base_type_id);
                        bufferInfo.size = compiler.get_declared_struct_size(bufferType);
                        bufferInfo.arrayCount = 1;
                        check(bufferType.array.size() == 0);
                    }
                    else
                    {
                        const spirv_cross::SPIRType& type = compiler.get_type(resource.type_id);
                        bufferInfo.size = 0;
                        bufferInfo.arrayCount = type.array.size() > 0 ? type.array[0] : 1;
                    }
                    bufferInfo.type = resourceType;
                    bufferInfo.stage = stage;

                    uint32_t setIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                    ShaderPropertySetDescription* setDesc = nullptr;
                    for (uint32_t i = 0; i < (uint32_t)properties.params.size(); ++i)
                    {
                        if (properties.params[i].setIndex == setIndex)
                        {
                            setDesc = &properties.params[i];
                            break;
                        }
                    }
                    if (!setDesc)
                    {
                        properties.params.push_back(ShaderPropertySetDescription());
                        setDesc = &properties.params.back();
                        setDesc->setIndex = setIndex;
                    }

                    check(setDesc && setDesc->setIndex == setIndex);
                    if (setDesc->params.size() < bufferInfo.binding + 1)
                        setDesc->params.resize(bufferInfo.binding + 1);
                    setDesc->params[bufferInfo.binding] = bufferInfo;

#ifdef MIST_SHADER_REFLECTION_LOG
                    logfdebug("> %s [ShaderStage: %s; Name:%s; Set: %u; Binding: %u; Size: %u; ArrayCount: %u;]\n", vkutils::GetVulkanDescriptorTypeName(descriptorType),
                        vkutils::GetVulkanShaderStageName(shaderStage), bufferInfo.Name.c_str(), setIndex, bufferInfo.Binding, bufferInfo.Size, bufferInfo.ArrayCount);
#endif // MIST_SHADER_REFLECTION_LOG

                    return setIndex;
                };

            check(bin.binary && bin.binaryCount && "Invalid binary source.");
            spirv_cross::CompilerGLSL compiler(bin.binary, bin.binaryCount);
            spirv_cross::ShaderResources resources = compiler.get_shader_resources();

#ifdef MIST_SHADER_REFLECTION_LOG
            logdebug("Processing shader reflection...\n");
#endif // MIST_SHADER_REFLECTION_LOG


            for (const spirv_cross::Resource& resource : resources.uniform_buffers)
                processSpirvResource(outProperties, compiler, resource, stage, ResourceType_ConstantBuffer);

            for (const spirv_cross::Resource& resource : resources.storage_buffers)
                processSpirvResource(outProperties, compiler, resource, stage, ResourceType_BufferUAV);

            for (const spirv_cross::Resource& resource : resources.sampled_images)
                processSpirvResource(outProperties, compiler, resource, stage, ResourceType_TextureSRV);

            for (const spirv_cross::Resource& resource : resources.storage_images)
                processSpirvResource(outProperties, compiler, resource, stage, ResourceType_TextureUAV);

            for (const spirv_cross::Resource& resource : resources.push_constant_buffers)
            {
                check(!outProperties.pushConstantMap.contains(stage));
                ShaderPushConstantDescription desc;
                desc.name = resource.name.c_str();
                desc.offset = compiler.get_decoration(resource.id, spv::DecorationOffset);
                const spirv_cross::SPIRType& type = compiler.get_type(resource.type_id);
                desc.size = (uint32_t)compiler.get_declared_struct_size(type);
                desc.stage = stage;
                outProperties.pushConstantMap[stage] = desc;
#ifdef MIST_SHADER_REFLECTION_LOG
                logfdebug("> PUSH_CONSTANT [ShaderStage: %s; Name: %s; Offset: %zd; Size: %zd]\n",
                    vkutils::GetVulkanShaderStageName(shaderStage), info.Name.c_str(), info.Offset, info.Size);
#endif // MIST_SHADER_REFLECTION_LOG

            }

            if (stage & ShaderType_Vertex)
            {
                for (const auto& resource : resources.stage_inputs)
                {
                    spirv_cross::SPIRType type = compiler.get_type(resource.type_id);

                    VertexInputAttribute attribute;
                    attribute.location = compiler.get_decoration(resource.id, spv::DecorationLocation);
                    attribute.count = type.vecsize;
                    attribute.size = type.width;
                    logfinfo("location: %d; size: %d; count: %d;\n", attribute.location, attribute.size, attribute.count);
                    outProperties.inputLayout.attributes.push_back(attribute);
                }
            }

#ifdef MIST_SHADER_REFLECTION_LOG
            logdebug("End shader reflection.\n");
#endif // MIST_SHADER_REFLECTION_LOG
            return true;
        }
    }
}