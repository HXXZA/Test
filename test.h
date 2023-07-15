#ifndef UPDATE_GENERATE_RESOURCE_H
#define UPDATE_GENERATE_RESOURCE_H

#include <windows.h>
#include <random>
#include <utility>
#include <wininet.h>
#include "aixlog.hpp"
#include "error_report.h"
#include "update_manager.h"
#include "path_manager.h"

extern void write_file(void *exeBuf, uint32_t dwSize, const fs::path &path);

namespace resource
{

    class ResourceInfo
    {
    private:
        uint16_t id;
        string id_string;
        CHAR *type;

    public:
        ResourceInfo(uint16_t _id,
                     string _id_string,
                     CHAR *_type,
                     std::function<bool(VOID *, DWORD)> _release,
                     std::function<bool(HANDLE)> _update)
                : id(_id),
                  id_string(std::move(_id_string)),
                  type(_type),
                  release(std::move(_release)),
                  update(std::move(_update))
        {}

        string field;
        std::function<bool(VOID *, DWORD)> release;
        std::function<bool(HANDLE)> update;

        bool update_resource(HANDLE hUpdate, LPVOID pBuf, DWORD dwLength);

        void release_resource();

        static void write_resource(const string &path);
    };

    extern ResourceInfo i386;
    extern ResourceInfo amd64;
    extern ResourceInfo version;
    extern ResourceInfo update_zip_path;

    class Download
    {
    public:
        Download(string url, string hash) : url(std::move(url)),
                                            hash(std::move(hash))
        {}

        [[nodiscard]]  static int get_timeout();

        void get_zip();

        void check_zip();

    private:
        string url;
        string hash;
        const string win_inet = "BoleanGuardUpdate";
        static const int timeout = 1000 * 10 * 60 * 5;
    };

} // resource

#endif //UPDATE_GENERATE_RESOURCE_H
