// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __COMMAND_LINE_PARSER_HPP__
#define __COMMAND_LINE_PARSER_HPP__

#include <cstdint>
#include <string>
#include "PixelFormat.hpp"

namespace JxrToAvif
{
    class CommandLineParser
    {
    public:
        CommandLineParser(int argc, char* argv[]);

        CommandLineParser(const CommandLineParser&) = delete;

        CommandLineParser(CommandLineParser&&) = delete;

        CommandLineParser& operator=(const CommandLineParser&) = delete;

        CommandLineParser&& operator=(CommandLineParser&&) = delete;

        ~CommandLineParser();

        [[nodiscard]] const std::wstring& GetInputFile() const
        {
            return _inputFile;
        }

        [[nodiscard]] const std::wstring& GetOutputFile() const
        {
            return _outputFile;
        }

        [[nodiscard]] int GetSpeed() const
        {
            return _speed;
        }

        [[nodiscard]] bool GetIsHelpRequired() const
        {
            return _helpRequired;
        }

        [[nodiscard]] bool GetIsTilingUsed() const
        {
            return _useTiling;
        }

        [[nodiscard]] uint8_t GetDepth() const
        {
            return _depth;
        }

        [[nodiscard]] PixelFormat GetPixelFormat() const
        {
            return _format;
        }

        bool Parse();

        static void PrintUsage();

    private:
        static constexpr auto DefaultOutputFile = L"output.avif";

        // 6 is default speed of the command line encoder, so it should be a good value?
        static constexpr int DefaultSpeed = 6;

        wchar_t** _argv;
        int _argc;
        int _speed;
        bool _helpRequired;
        bool _useTiling;
        PixelFormat _format;
        uint8_t _depth;
        std::wstring _inputFile;
        std::wstring _outputFile;
    };
}

#endif // __COMMAND_LINE_PARSER_HPP__
