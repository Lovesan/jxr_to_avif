// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __JXR_DATA_HPP__
#define __JXR_DATA_HPP__

#include <cstring>
#include <string>
#include <sstream>
#include <stdexcept>
#include "jxr_sys_helpers.h"
#include "jxr_data.h"

namespace JxrToAvif
{
    class JxrLoaderThreadState
    {
    public:
        JxrLoaderThreadState()
            : _loaded(false)
        {
            const auto hr = jxr_init_loader_thread();

            if (hr < 0)
            {
                std::stringstream s;
                const auto errorDesc = jxr_get_error_description(hr);
                s << "Failed to initialize JXR loader thread: " << errorDesc;
                jxr_free_error_description(errorDesc);
                throw std::runtime_error(s.str());
            }

            _loaded = true;
        }

        JxrLoaderThreadState(const JxrLoaderThreadState&) = delete;

        JxrLoaderThreadState(JxrLoaderThreadState&&) = delete;

        ~JxrLoaderThreadState()
        {
            if (_loaded)
                jxr_deinit_loader_thread();
        }

        JxrLoaderThreadState& operator=(const JxrLoaderThreadState&) = delete;

        JxrLoaderThreadState& operator=(JxrLoaderThreadState&&) = delete;
    private:
        bool _loaded;
    };

    class JxrData
    {
    public:
        explicit JxrData(const std::wstring& filename) noexcept(false)
            : _data{}
        {
            const auto hr = jxr_load_data(filename.c_str(), &_data);

            if (hr < 0)
            {
                std::string s("Failed to get image data: ");
                const auto errorDesc = jxr_get_error_description(hr);
                s.append(errorDesc);
                jxr_free_error_description(errorDesc);
                throw std::runtime_error(s);
            }
        }

        JxrData(const JxrData&) = delete;

        JxrData(JxrData&& rhs) noexcept(true)
            : _data(rhs._data)
        {
            memset(&rhs._data, 0, sizeof(jxr_data));
        }

        ~JxrData() noexcept(true)
        {
            jxr_free_data(&_data);
        }

        JxrData& operator=(const JxrData&) = delete;

        JxrData& operator=(JxrData&& rhs) noexcept(true)
        {
            if (this != &rhs)
            {
                _data = rhs._data;
                memset(&rhs._data, 0, sizeof(jxr_data));
            }
            return *this;
        }

        [[nodiscard]] const jxr_data& Get() const
        {
            return _data;
        }

    private:
        jxr_data _data;
    };
}

#endif // __JXR_DATA_HPP__
