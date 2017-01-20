/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include "precompiled.h"

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Output.h"
#include "Action.h"
#include "JSONWriter.h"
#include "Configuration.h"
#include "ScriptEnginePython.h"

namespace CodeGenerator
{
    namespace GlobalConfiguration
    {
        using namespace llvm;

        // Globally-accessible settings (access outside of main.cpp via global variables in configuration.h)
        static cl::OptionCategory CodeGeneratorCategory("AZ Code Generator");

        // Access via g_enableVerboseOutput elsewhere (extern in configuration.h)
        bool g_enableVerboseOutput;
        static cl::opt<bool, true> verboseOutput("v", cl::desc("Output verbose debug information"), cl::cat(CodeGeneratorCategory), cl::location(g_enableVerboseOutput));
    }

    namespace Configuration
    {
        using namespace llvm;

        enum FrontEnd
        {
            FE_Clang,
            FE_JSON
        };

        static cl::opt<FrontEnd> frontEnd(cl::desc("Choose frontend:"), cl::values(clEnumValN(FE_Clang, "Clang", "Clang Compiler"), clEnumValN(FE_JSON, "JSON", "Raw JSON Input"), clEnumValEnd), cl::init(FE_Clang));

        // Code parsing settings
        static cl::OptionCategory codeParserCategory("Code Parsing Options");
        static cl::opt<std::string> inputPath("input-path", cl::desc("Absolute path to input folder, all input-file paths must be relative to this folder"), cl::cat(codeParserCategory), cl::Required);
        static cl::opt<std::string> outputPath("output-path", cl::desc("Absolute path to output folder"), cl::cat(codeParserCategory), cl::Required);
        static cl::list<std::string> inputFiles("input-file", cl::desc("Path to input file relative to the input-path"), cl::cat(codeParserCategory), cl::OneOrMore);

        static cl::list<std::string> forceIncludes("force-include", cl::desc("List of headers to force include into clang."), cl::cat(codeParserCategory), cl::ZeroOrMore);
        static cl::list<std::string> includePaths("include-path", cl::desc("Header include path"), cl::cat(codeParserCategory), cl::ZeroOrMore);
        static cl::list<std::string> preprocessorDefinitions("define", cl::desc("Preprocessor definition"), cl::cat(codeParserCategory), cl::ZeroOrMore);

        static cl::opt<std::string> clangSettingsFile("clang-settings-file", cl::desc("Path to file that contains clang configuration settings"), cl::cat(codeParserCategory), cl::Optional);
        static cl::opt<std::string> intermediateFile("intermediate-file", cl::desc("Path to a place to store the JSON AST from clang parsing"), cl::cat(codeParserCategory), cl::Optional);
        static cl::opt<std::string> resourceDir("resource-dir", cl::desc("Path to the resource directory to provide to clang"), cl::cat(codeParserCategory), cl::Optional);
    }
}

// Utility to wrap the program's exit, used to perform cleanup.
int Exit(unsigned int exitCode)
{
    return exitCode;
}

using namespace CodeGenerator;

int main(int argc, char** argv)
{
    llvm::cl::ParseCommandLineOptions(argc, argv);

    // Avoid capturing command line for now, we need to be able to pass it back properly when we do capture it
    {
        Output::ScopedCapture outputCapture;

        const auto& inputFiles = Configuration::inputFiles;

        if (inputFiles.empty())
        {
            Output::Error("No files to process, input-list file provided could not be read or was empty");
            return Exit(ErrorCodes::EmptyInputList);
        }

        auto processFileCount = inputFiles.size();

        if (GlobalConfiguration::verboseOutput)
        {
            Output::Print("%d files to process\n", inputFiles.size());
        }

        // Clang flow
        GlobalConfiguration::ClangSettings settings;
        clang::FileManager fileManager({"."});
        if (Configuration::frontEnd == Configuration::FE_Clang)
        {
            if (!Configuration::clangSettingsFile.empty())
            {
                settings.Load(Configuration::clangSettingsFile);
            }
            else
            {
                // Platform-agnostic default settings
                settings += "-std=c++11"; // Enable C++11 support
                settings += "-w";  // Inhibit all warning messages.
                settings += "-DAZ_CODE_GENERATOR"; // Used to toggle code gen macros
                settings += "-fsyntax-only"; // Do a syntax-only pass
                //TODO: If we ignore includes we do not get base class information.
                //settings += "-fignore-includes";

                // Platform-specific default settings
#ifdef AZCG_PLATFORM_WINDOWS
                // Windows compiler defines
                settings += "-DWIN64";
                settings += "-D_WIN32";
                settings += "-D_WIN64";
                settings += "-D_M_X64";
                settings += "-D_M_AMD64";
                settings += "-D_MSC_VER=1800";
                settings += "-D_MSC_FULL_VER=180031101";
                settings += "-D_SIZE_T_DEFINED";

                // Microsoft compatibility in clang
                settings += "-fms-extensions";
                settings += "-fms-compatibility"; // Fixes issues with windows headers (Particularly types like size_t)
                settings += "-fdelayed-template-parsing"; // Fixes issues with windows headers (Particularly things like operator new)
                settings += "-fdiagnostics-format=msvc"; // Changes the output to be MSVC/Error List friendly

                // Compile to windows 64bit target
                settings += "--target=x86_64-pc-windows-msvc";
#endif
#ifdef AZCG_PLATFORM_DARWIN
                settings += "-stdlib=libc++";

                // Necessary paths to find all the C++ libraries, headers, and frameworks
                settings += "-isysroot/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain";
                if (!Configuration::resourceDir.empty())
                {
                    settings += "-Xclang";
                    settings += "-resource-dir";
                    settings += "-Xclang";
                    settings += Configuration::resourceDir;
                }
                settings += "-I/usr/include";

                // Compile to darwin 64bit target
                settings += "--target=x86_64-apple-macosx";
#endif
                settings += "-x";
                settings += "c++"; // Treat the file as working with C++
            }

            if (GlobalConfiguration::verboseOutput)
            {
                Output::Print("Base Parameter Set:");
                for (const auto& setting : settings.Args())
                {
                    Output::Print("%s ", setting.c_str());
                }
            }

            for (auto& include : Configuration::includePaths)
            {
                // Generate parameters for include paths
                std::string setting = "-I";
                setting.append(include);

                if (GlobalConfiguration::verboseOutput)
                {
                    Output::Print("Adding Include Path: %s\n", setting.c_str());
                }

                settings += setting;
            }

            for (auto& preprocessorDefine : Configuration::preprocessorDefinitions)
            {
                // Generate parameters for preprocessor defines
                std::string setting = "-D";
                setting.append(preprocessorDefine);

                if (GlobalConfiguration::verboseOutput)
                {
                    Output::Print("Adding Preprocessor Define: %s\n", setting.c_str());
                }

                settings += setting;
            }

            for (auto& forceInclude : Configuration::forceIncludes)
            {
                // Generate parameters for header files included by force
                std::string setting = "-include";
                setting.append(forceInclude);

                if (GlobalConfiguration::verboseOutput)
                {
                    Output::Print("Adding Header File Included By Force: %s\n", setting.c_str());
                }

                settings += setting;
            }
            fileManager.Retain();
        }

        // Get python set up and ready to go
        std::unique_ptr<PythonScripting::PythonScriptEngine> scriptEngine = std::make_unique<PythonScripting::PythonScriptEngine>(argv[0]);
        unsigned int scriptEngineInit = scriptEngine->Initialize();
        if (scriptEngineInit != 0)
        {
            return Exit(scriptEngineInit);
        }

        std::string inputPath = Configuration::inputPath;
        std::replace(inputPath.begin(), inputPath.end(), '\\', '/');
        if (inputPath[inputPath.length() - 1] != '/')
        {
            inputPath.append("/");
        }

        std::string outputPath = Configuration::outputPath;
        std::replace(outputPath.begin(), outputPath.end(), '\\', '/');
        if (outputPath[outputPath.length() - 1] != '/')
        {
            outputPath.append("/");
        }

        if (GlobalConfiguration::verboseOutput)
        {
            Output::Print("Processing files in %s:\n", Configuration::inputPath.c_str());
        }
        for (unsigned long processFileIndex = 0; processFileIndex < processFileCount; ++processFileIndex)
        {
            const auto& inputFileName = inputFiles[processFileIndex];

            if (GlobalConfiguration::verboseOutput)
            {
                Output::Print("Processing %s\n", inputFileName.c_str());
            }

            std::string inputFilePath = inputPath + inputFileName;
            std::replace(inputFilePath.begin(), inputFilePath.end(), '\\', '/');

            std::string jsonString = "";

            // Use Clang to parse into a json intermediate
            if (Configuration::frontEnd == Configuration::FE_Clang)
            {
                settings += inputFilePath.c_str();

                JSONWriter<rapidjson::StringBuffer> writer("");

                writer.Begin();
                {
                    clang::tooling::ToolInvocation invocation(settings.Args(), new Action(&writer), &fileManager);
                    if (!invocation.run())
                    {
                        writer.End();
                        return Exit(ErrorCodes::ClangToolInvokationFailure);
                    }
                }
                writer.End();

                 // Remove the filename arg to allow for the next one to be added.
                settings.Args().pop_back();

                // Tease out the json object we created during clang processing
                const rapidjson::StringBuffer& stringBuffer = writer.GetStringBuffer();
                jsonString = stringBuffer.GetString();
            }
            else if (Configuration::frontEnd == Configuration::FE_JSON)
            {
                // Use the raw json string that should be contained in the input file
                std::ifstream inputFile(inputFilePath);
                if (inputFile.is_open())
                {
                    std::stringstream fileStream;
                    fileStream << inputFile.rdbuf();
                    jsonString = fileStream.str();
                    inputFile.close();
                }
            }

            if (!Configuration::intermediateFile.empty())
            {
                std::ofstream outputFile(Configuration::intermediateFile);
                if (outputFile.is_open())
                {
                    outputFile << jsonString;
                    outputFile.close();
                }
            }

            if (scriptEngine->HasScripts())
            {
                // Invoke scripting with the relevant data for the current file
                unsigned int invokeResult = scriptEngine->Invoke(jsonString.c_str(), inputFileName.c_str(), inputPath.c_str(), outputPath.c_str());
                if (invokeResult != 0)
                {
                    return Exit(invokeResult);
                }
            }
        }
    }
    return Exit(0);
}