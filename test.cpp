#include "resource.h"
#include <Wincrypt.h>
#include <random>
#include <wininet.h>

#undef ERROR

struct AsyncInfo
{
    HANDLE hEvent;
    HINTERNET hUrl;
    error_type err;
};

void CALLBACK DownloadCallback(HINTERNET hInternet,
                               DWORD_PTR dwContext,
                               DWORD dwInternetStatus,
                               LPVOID lpvStatusInformation,
                               [[maybe_unused]] DWORD dwStatusInformationLength)
{

    if (dwInternetStatus != INTERNET_STATUS_REQUEST_COMPLETE)
    {
        return;
    }

    auto pResult = reinterpret_cast<INTERNET_ASYNC_RESULT *>(lpvStatusInformation);
    auto monitor = reinterpret_cast<AsyncInfo *>(dwContext);
    if (pResult->dwError != S_OK)
    {
        LOG(ERROR) << "winint callback failed: " << pResult->dwError << endl;
        monitor->err = CustomException::inner;
        goto clean;
    }
    monitor->hUrl = reinterpret_cast<HINTERNET>(pResult->dwResult);

    clean:
    if (!SetEvent(monitor->hEvent))
    {
        LOG(ERROR) << "set event when download failed: " << GetLastError() << endl;
        exit(CustomException::inner);
    }

}

void wait_the_response(AsyncInfo async_info)
{
    switch (WaitForSingleObject(async_info.hEvent, resource::Download::get_timeout()))
    {
        case WAIT_OBJECT_0:
            if (async_info.err != CustomException::success)
            {
                throw CustomException{async_info.err};
            }
            break;
        case WAIT_TIMEOUT:
            LOG(ERROR) << "the server response is so slowly failed" << endl;
            throw CustomException{CustomException::time_out};
        case WAIT_FAILED:
            LOG(ERROR) << "wait response failed: " << GetLastError() << endl;
            throw CustomException{CustomException::inner};
    }
}

void write_file(void *exeBuf, uint32_t dwSize, const fs::path &path)
{
    ofstream file(path, ios::out | ios::binary);
    if (!file.is_open())
    {
        LOG(ERROR) << "open file: " << path << " failed" << endl;
        throw runtime_error("");
    }

    file.write(static_cast<char *>(exeBuf), dwSize);
    if (file.bad())
    {
        LOG(ERROR) << "write to file: " << path <<
                   " form resource file failed" << endl;
        file.close();
    }
}

namespace resource
{
    constexpr uint8_t MD5_LEN = 16;
    constexpr char HEX_CHARSET[] = "0123456789ABCDEF";

    ResourceInfo i386{0x3312, "i386_dir", RT_STRING,
                      [](VOID *exeBuf, DWORD dwSize) -> bool
                      {
                          i386.field = string(reinterpret_cast<LPCCH>(exeBuf));
                          return true;
                      },
                      [](HANDLE hUpdate) -> bool
                      {
                          return i386.update_resource(hUpdate,
                                                      i386.field.data(),
                                                      i386.field.length() + 1);
                      }};

    ResourceInfo amd64{0x3311, "amd64_dir", RT_STRING,
                       [](VOID *exeBuf, DWORD dwSize) -> bool
                       {
                           amd64.field = string(reinterpret_cast<LPCCH>(exeBuf));
                           return true;
                       },
                       [](HANDLE hUpdate) -> bool
                       {
                           return amd64.update_resource(hUpdate,
                                                        amd64.field.data(),
                                                        amd64.field.length() + 1);
                       }};

    ResourceInfo version{0x3310, "version", RT_STRING,
                         [](VOID *exeBuf, DWORD dwSize) -> bool
                         {
                             auto path = path_manager::g_path_manager->get_update_version_path();
                             try
                             {
                                 write_file(exeBuf, dwSize - 1, path);
                             } catch (const std::runtime_error &e)
                             {
                                 return false;
                             }

                             ofstream file(path, ios::out);
                             if (!file.is_open())
                             {
                                 LOG(ERROR) << "open file: " << path << " failed" << endl;
                                 return false;
                             }

                             file.write(static_cast<char *>(exeBuf), dwSize);
                             if (file.bad())
                             {
                                 LOG(ERROR) << "write to file: " << path <<
                                            " form resource file failed" << endl;
                                 return false;
                             }
                             file.close();
                             return true;
                         },
                         [](HANDLE hUpdate) -> bool
                         {
                             return version.update_resource(hUpdate,
                                                            version.field.data(),
                                                            version.field.length() + 1);
                         }};

    ResourceInfo update_zip_path{0x3313, "update_zip", RT_RCDATA,
                                 [](VOID *exeBuf, DWORD dwSize) -> bool
                                 {
                                     auto path = path_manager::g_path_manager->get_update_zip_path();
                                     ofstream file(path, ios::binary | ios::out);
                                     if (!file.is_open())
                                     {
                                         LOG(ERROR) << "open file: " << path << " failed" << endl;
                                         return false;
                                     }

                                     file.write(static_cast<char *>(exeBuf), dwSize);
                                     if (file.bad())
                                     {
                                         LOG(ERROR) << "write to file: " << path <<
                                                    " form resource file failed" << endl;
                                         return false;
                                     }
                                     file.close();
                                     return true;;
                                 },
                                 [](HANDLE hUpdate) -> bool
                                 {
                                     ifstream file(path_manager::g_path_manager->update_zip_file_name);
                                     if (!file.is_open())
                                     {
                                         LOG(ERROR) << "open file: "
                                                    << path_manager::g_path_manager->update_zip_file_name
                                                    << "for add resources failed" << endl;
                                         return false;
                                     }

                                     vector<unsigned char> contents((istreambuf_iterator<char>(file)),
                                                                    istreambuf_iterator<char>());
                                     if (file.bad())
                                     {
                                         LOG(ERROR) << "read file: "
                                                    << path_manager::g_path_manager->update_zip_file_name
                                                    << "to buffer for add resouces failed" << endl;
                                         return false;
                                     }

                                     return i386.update_resource(hUpdate,
                                                                 contents.data(),
                                                                 contents.size());
                                 }};

    void Download::get_zip()
    {
        LOG(INFO) << "start to download zip" << endl;
        HINTERNET hInternet = nullptr;
        HANDLE hEvent = nullptr;
        HINTERNET hUrl = nullptr;
        auto err = CustomException::success;
        auto async_info = AsyncInfo{nullptr, nullptr, CustomException::success};
        std::ofstream file;

        hInternet = InternetOpen(win_inet.data(),
                                 INTERNET_OPEN_TYPE_DIRECT,
                                 nullptr,
                                 nullptr,
                                 INTERNET_FLAG_ASYNC);
        if (nullptr == hInternet)
        {
            LOG(ERROR) << "open internet instance failed: " << GetLastError() << endl;
            err = CustomException::inner;
            goto clean;
        }

        if (INTERNET_INVALID_STATUS_CALLBACK == InternetSetStatusCallbackA(hInternet, DownloadCallback))
        {
            LOG(ERROR) << "set download call back failed: " << GetLastError() << endl;
            err = CustomException::inner;
            goto clean;
        }

        hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!hEvent)
        {
            LOG(ERROR) << "create async for download failed: " << GetLastError() << endl;
            err = CustomException::inner;
            goto clean;
        }

        async_info.hEvent = hEvent;
        hUrl = InternetOpenUrl(hInternet,
                               url.data(),
                               nullptr,
                               0,
                               INTERNET_FLAG_RELOAD,
                               reinterpret_cast<DWORD_PTR>(&async_info));

        if (nullptr == hUrl)
        {
            if (ERROR_IO_PENDING != GetLastError())
            {
                LOG(ERROR) << "open internet url instance failed: " << GetLastError() << endl;
                err = CustomException::inner;
                goto clean;
            }
        }

        try
        {
            wait_the_response(async_info);
        }
        catch (CustomException &e)
        {
            err = e.code();
            goto clean;
        }

        const int bufferSize = 4096;
        char buffer[bufferSize];
        DWORD bytesRead = 0;
        hUrl = async_info.hUrl;
        hEvent = async_info.hEvent;

        file.open(path_manager::g_path_manager->update_zip_file_name, std::ios::binary);
        if (!file.is_open())
        {
            LOG(ERROR) << "create download file :"
                       << path_manager::g_path_manager->update_zip_file_name
                       << "failed"
                       << endl;
            err = CustomException::inner;
        }
        while (true)
        {
            if (!InternetReadFile(hUrl, buffer, bufferSize, &bytesRead))
            {
                if (ERROR_IO_PENDING == GetLastError())
                {
                    try
                    {
                        wait_the_response(async_info);
                    }
                    catch (CustomException &e)
                    {
                        err = e.code();
                        goto clean;
                    }
                    ResetEvent(async_info.hEvent);
                }
                else
                {
                    LOG(ERROR) << "InternetReadFile failed: " << GetLastError() << endl;
                    err = CustomException::inner;
                    goto clean;
                    break;
                }
            }
            else
                if (bytesRead == 0)
                {
                    break;
                }
                else
                {
                    file.write(buffer, bytesRead);
                    if (file.bad())
                    {
                        LOG(ERROR) << "create download file : " << path_manager::g_path_manager->update_zip_file_name
                                   << "failed" << endl;
                        err = CustomException::inner;
                        goto clean;
                    }
                }
        }


        clean:

        if (nullptr == hUrl)
        {
            InternetCloseHandle(hUrl);
        }

        if (nullptr == hInternet)
        {
            InternetCloseHandle(hInternet);
        }

        if (hEvent)
        {
            CloseHandle(hEvent);
        }

        if (file.is_open())
        {
            file.close();
        }

        if (err != CustomException::success)
        {
            throw CustomException{err};
        }

        LOG(INFO) << "download zip successfully" << endl;
    }

    int Download::get_timeout()
    {
        auto range = static_cast<int>(static_cast<double>(timeout) * 0.2);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(timeout - range, timeout + range);
        return dis(gen);
    }

    void Download::check_zip()
    {
        LOG(INFO) << "start to calculate hash" << endl;

        HCRYPTPROV hProv{0};
        if (CryptAcquireContext(&hProv,
                                nullptr,
                                nullptr,
                                PROV_RSA_FULL,
                                CRYPT_VERIFYCONTEXT) == FALSE)
        {
            LOG(ERROR) << "prepare crypt context failed: " << GetLastError() << endl;
            throw CustomException{CustomException::inner};
        }


        HCRYPTHASH hHash{0};
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash) == FALSE)
        {
            LOG(ERROR) << "create handle of hash: md5 failed: " << GetLastError() << endl;
            CryptReleaseContext(hProv, 0);
            throw CustomException{CustomException::inner};
        }

        ifstream file(path_manager::g_path_manager->update_zip_file_name);
        if (!file.is_open())
        {
            LOG(ERROR) << "open file: " << path_manager::g_path_manager->update_zip_file_name
                       << "for calculate hash failed" << endl;
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw CustomException{CustomException::inner};
        }

        vector<unsigned char> contents((istreambuf_iterator<char>(file)),
                                       istreambuf_iterator<char>());
        if (file.bad())
        {
            LOG(ERROR) << "read file: " << path_manager::g_path_manager->update_zip_file_name
                       << "to buffer for calculate hash failed" << endl;
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw CustomException{CustomException::inner};
        }

        array<BYTE, MD5_LEN> buf{};
        DWORD length = MD5_LEN;
        if (!CryptGetHashParam(hHash, HP_HASHVAL, buf.data(), &length, 0))
        {
            LOG(ERROR) << "calculate file: " << path_manager::g_path_manager->update_zip_file_name
                       << "hash failed"
                       << endl;
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw CustomException{CustomException::inner};
        }

        string calculate_hash;
        for (int i = 0; i < MD5_LEN; i++)
        {
            calculate_hash += HEX_CHARSET[((buf[i] >> 4) & 0xF)];
            calculate_hash += HEX_CHARSET[((buf[i]) & 0x0F)];
        }

        if (calculate_hash != hash)
        {
            LOG(ERROR) << "calculate file: " << path_manager::g_path_manager->update_zip_file_name
                       << "hash is "
                       << calculate_hash
                       << "is different to ini hash: "
                       << hash
                       << endl;
            throw CustomException{CustomException::hash_diff};
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        file.close();
        LOG(INFO) << "calculate hash successfully" << endl;
    }

    bool ResourceInfo::update_resource(HANDLE hUpdate, LPVOID pBuf, DWORD dwLength)
    {
        if (!UpdateResource(hUpdate,
                            this->type,
                            MAKEINTRESOURCE(this->id),
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            pBuf,
                            dwLength))
        {
            LOG(ERROR) << "update resource: " <<
                       this->id_string
                       << " failed :"
                       << GetLastError();

            return false;
        }

        LOG(INFO) << "update resource: " <<
                  this->id_string
                  << " successfully";
        return true;
    }

    void ResourceInfo::write_resource(const string &path)
    {
        LOG(ERROR) << "start to write resource";

        auto tmp_file = path_manager::g_path_manager->update_gen_file_name_prefix.string()
                        + path_manager::g_path_manager->version_file_name.string()
                        + ".exe";
        fs::remove(tmp_file);

        try
        {
            fs::copy_file(path, tmp_file);
        }

        catch (const fs::filesystem_error &e)
        {
            LOG(ERROR) << "copy self to: " << tmp_file << " failed: " << e.what() << " :" << e.code();
            throw CustomException{CustomException::inner};
        }

        ifstream file(path_manager::g_path_manager->update_zip_file_name, ios::binary);
        if (!file.is_open())
        {
            LOG(ERROR) << "open " << path_manager::g_path_manager->update_zip_file_name << " failed";
            throw CustomException{CustomException::inner};
        }

        vector<uint8_t> buf((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        if (file.bad())
        {
            LOG(ERROR) << "read " << path_manager::g_path_manager->update_zip_file_name
                       << "as buf incorrectly";
            throw CustomException{CustomException::inner};
        }
        file.close();

        HANDLE hUpdate = BeginUpdateResource(tmp_file.data(), FALSE);
        if (nullptr == hUpdate)
        {
            LOG(ERROR) << "start to write resource failed: " << GetLastError();
            throw CustomException{CustomException::inner};
        }

        if (i386.update(hUpdate))
        {
            throw CustomException{CustomException::inner};
        }

        if (amd64.update(hUpdate))
        {
            throw CustomException{CustomException::inner};
        }

        if (version.update(hUpdate))
        {
            throw CustomException{CustomException::inner};
        }

        if (update_zip_path.update(hUpdate))
        {
            throw CustomException{CustomException::inner};
        }

        if (!EndUpdateResource(hUpdate, FALSE))
        {
            LOG(ERROR) << "write zip to exe file failed: " << GetLastError();
            throw CustomException{CustomException::inner};
        }
    }


    void ResourceInfo::release_resource()
    {
        HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(this->id), this->type);
        if (nullptr == hRes)
        {
            LOG(ERROR) << "find id: " << this->id_string
                       << "in resource file failed: " << GetLastError();
            throw CustomException{CustomException::inner};
        }

        HGLOBAL hResLoad = LoadResource(nullptr, hRes);
        if (nullptr == hResLoad)
        {
            LOG(ERROR) << "load id: " << this->id_string
                       << " form resource file failed: " << GetLastError() << endl;
            throw CustomException{CustomException::inner};
        }

        DWORD dwSize = SizeofResource(nullptr, hRes);
        if (dwSize == 0)
        {
            LOG(ERROR) << "get_zip id: " << this->id_string
                       << " size in resource file failed: " << GetLastError() << endl;
            throw CustomException{CustomException::inner};
        }

        VOID *exeBuf = LockResource(hResLoad);
        if (nullptr == exeBuf)
        {
            LOG(ERROR) << "get_zip id: " << this->id_string
                       << " address in resource file failed: " << GetLastError() << endl;
            throw CustomException{CustomException::inner};
        }

        if (this->release(exeBuf, dwSize))
        {
            throw CustomException{CustomException::inner};
        }
    }
} // resource
