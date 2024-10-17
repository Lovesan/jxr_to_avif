// Copyright 2024 Dmitry Ignatiev. All rights reserved

#include <cwctype>
#include <iostream>
#include <windows.h>
#include "CommandLineParser.hpp"

#include <algorithm>

namespace JxrToAvif
{
    CommandLineParser::CommandLineParser(int, char**)
        : _argv(nullptr), _argc(0), _speed(DefaultSpeed),
        _helpRequired(false), _useTiling(true), _format(PixelFormatYuv444),
        _depth(12), _outputFile(DefaultOutputFile)
    {
        const auto cmdLine = GetCommandLineW();
        _argv = CommandLineToArgvW(cmdLine, &_argc);
    }

    CommandLineParser::~CommandLineParser()
    {
        if (_argv)
        {
            LocalFree(reinterpret_cast<HLOCAL>(_argv));
            _argv = nullptr;
        }
    }

    bool CommandLineParser::Parse()
    {
        int i = 1;
        bool hasInputFile = false, hasOutputFile = false;

        if (_argc < 1)
            return false;

        while (i < _argc)
        {
            auto arg = std::wstring(_argv[i]);
            if (arg == L"--help")
            {
                _helpRequired = true;
            }
            else if (arg == L"--speed")
            {
                ++i;
                if (i >= _argc)
                {
                    return false;
                }
                arg = std::wstring(_argv[i]);
                try
                {
                    const auto n = std::stoi(arg);
                    if (n < 0 || n > 10)
                        return false;
                    _speed = n;
                }
                catch (std::exception&)
                {
                    return false;
                }
            }
            else if(arg == L"--depth")
            {
                ++i;
                if(i >= _argc)
                {
                    return false;
                }
                arg = std::wstring(_argv[i]);
                try
                {
                    const auto n = std::stoi(arg);
                    if (n != 10 && n != 12)
                        return false;
                    _depth = static_cast<uint8_t>(n);
                }
                catch (std::exception&)
                {
                    return false;
                }
            }
            else if (arg == L"--format")
            {
                ++i;
                if(i >= _argc)
                {
                    return false;
                }
                arg = std::wstring(_argv[i]);
                std::transform(arg.begin(), arg.end(), arg.begin(), std::towlower);
                if(arg == L"rgb")
                {
                    _format = PixelFormatRgb;
                }
                else if(arg == L"yuv444")
                {
                    _format = PixelFormatYuv444;
                }
                else if(arg == L"yuv422")
                {
                    _format = PixelFormatYuv422;
                }
                else if(arg == L"yuv420")
                {
                    _format = PixelFormatYuv420;
                }
                else if(arg == L"yuv400")
                {
                    _format = PixelFormatYuv400;
                }
                else
                {
                    return false;
                }
            }
            else if(arg == L"--without-tiling")
            {
                _useTiling = false;
            }
            else if (hasOutputFile)
            {
                return false;
            }
            else if (hasInputFile)
            {
                hasOutputFile = true;
                _outputFile = arg;
            }
            else
            {
                hasInputFile = true;
                _inputFile = arg;
            }
            ++i;
        }

        return hasInputFile;
    }

    void CommandLineParser::PrintUsage()
    {
        std::cout << "Usage: jxr_to_avif [options] input.jxr [output.avif]\n";
        std::cout << "Options:\n";
        std::cout << "  --help              Print this message.\n";
        std::cout << "  --speed <n>         AVIF encoding speed.\n";
        std::cout << "                      Must be in range of 0 to 10. Defaults to 6.\n";
        std::cout << "  --without-tiling    Do not use tiling.\n";
        std::cout << "                      Tiling means slightly larger file size\n";
        std::cout << "                      but faster encoding and decoding.\n";
        std::cout << "  --depth <n>         Output color depth. May equal 10 or 12.\n";
        std::cout << "                      Defaults to 12 bits.\n";
        std::cout << "  --format            Output pixel format. Defaults to yuv444.\n";
        std::cout << "                      Must be one of:\n";
        std::cout << "                        rgb, yuv444, yuv422, yuv420, yuv400\n";
    }
}
