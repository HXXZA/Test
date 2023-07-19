#include "download.h"
#include <random>
#include <wininet.h>
#include "utils.h"
#include <intrin.h>

#undef ERROR


namespace download
{
    constexpr uint64_t MINUTE = 1000 * 10 * 60;
    uint16_t Download::timeout = 0;

    Download::Download(string url, string hash, uint16_t _timeout)
            : hash(std::move(hash))
    {
        timeout = _timeout * MINUTE;
        URL_COMPONENTSA urlComponents = {sizeof(URL_COMPONENTS)};
        urlComponents.dwHostNameLength = 1;
        urlComponents.dwUrlPathLength = 1;

        if (!InternetCrackUrl(url.data(), url.length(), 0, &urlComponents))
        {
            LOG(ERROR) << "parse url failed: " << GetLastError() << endl;
            throw CustomException{ErrorType::url_format_error};
        }

        host_name = string(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
        LOG(INFO) << "get host name: " << host_name << endl;

        url_path = string(urlComponents.lpszUrlPath, urlComponents.dwUrlPathLength);
        LOG(INFO) << "get url path name: " << url_path << endl;
    }

    void Download::get_zip()
    {
        LOG(INFO) << "start to download zip" << endl;

        HINTERNET hInternet;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;
        auto err = ErrorType::success;
        auto async_info = AsyncInfo{nullptr, nullptr, ErrorType::success};
        uint64_t download_length = 0;

        DWORD dwFlags;
        DWORD dwBuffLen = sizeof(DWORD);

        std::ofstream file;
        DWORD bytesRead = 0;
        const int bufferSize = 1024;


        hInternet = InternetOpen(win_inet.data(),
                                 INTERNET_OPEN_TYPE_PRECONFIG,
                                 nullptr,
                                 nullptr,
                                 INTERNET_FLAG_ASYNC);
        if (nullptr == hInternet)
        {
            LOG(ERROR) << "open internet instance failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        if (INTERNET_INVALID_STATUS_CALLBACK == InternetSetStatusCallbackA(hInternet, Download::DownloadCallback))
        {
            LOG(ERROR) << "set download call back failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        async_info.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!async_info.hEvent)
        {
            LOG(ERROR) << "create async event for download failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        hConnect = InternetConnect(hInternet,
                                   host_name.data(),
                                   INTERNET_DEFAULT_HTTPS_PORT,
                                   nullptr, nullptr, INTERNET_SERVICE_HTTP, 0,
                                   reinterpret_cast<DWORD_PTR>(&async_info));
        if (nullptr == hConnect)
        {
            LOG(ERROR) << "create internet connection for download failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        hRequest = HttpOpenRequest(hConnect,
                                   "GET",
                                   url_path.data(),
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   INTERNET_FLAG_HYPERLINK |
                                   INTERNET_FLAG_RELOAD |
                                   INTERNET_FLAG_KEEP_CONNECTION |
                                   INTERNET_FLAG_NO_CACHE_WRITE |
                                   INTERNET_FLAG_PRAGMA_NOCACHE |
                                   INTERNET_FLAG_RESYNCHRONIZE |
                                   INTERNET_FLAG_SECURE,
                                   reinterpret_cast<DWORD_PTR>(&async_info));

        if (nullptr == hRequest)
        {
            LOG(ERROR) << "open http request failed : " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        if (!InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffLen))
        {
            LOG(ERROR) << "get http security flag failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        dwFlags |= SECURITY_FLAG_IGNORE_REVOCATION;

        if (!InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags)))
        {
            LOG(ERROR) << "set http security flag failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        if (!HttpSendRequest(hRequest, nullptr, 0, nullptr, 0))
        {
            if (ERROR_IO_PENDING == GetLastError())
            {
                err = handle_wait_response(async_info);
                if (err != ErrorType::success)
                {
                    goto clean;
                }
            }
            else
            {
                LOG(ERROR) << "send http request failed: " << GetLastError() << endl;
                err = ErrorType::inner_failed;
                goto clean;
            }
        }

        if (!HttpQueryInfo(hRequest,
                           HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                           &dwFlags,
                           &dwBuffLen,
                           nullptr))
        {
            LOG(INFO) << "get http header failed: " << GetLastError() << endl;
        }

        LOG(INFO) << "get http content length: " << dwFlags << endl;

        char buffer[bufferSize];
        file.open(path_manager::g_path_manager->download_zip_path, std::ios::binary);
        if (!file.is_open())
        {
            LOG(ERROR) << "create download file :"
                       << path_manager::g_path_manager->download_zip_path
                       << " failed"
                       << endl;
            err = ErrorType::inner_failed;
        }

//        cout<<88;
        while (true)
        {
            bytesRead = 0;
            if (!InternetReadFile(hRequest, buffer, bufferSize, &bytesRead))
            {
                if (ERROR_IO_PENDING == GetLastError())
                {
                    cout<<"com"<<endl;
                    err = handle_wait_response(async_info);
                    if (err != ErrorType::success)
                    {
                        goto clean;
                    }
                }
                else
                {
                    LOG(ERROR) << "send http request failed: " << GetLastError() << endl;
                    err = ErrorType::inner_failed;
                    goto clean;
                }
            }
            else
            {
                cout<<"bytesRead" << bytesRead<<endl;
                download_length += bytesRead;
                LOG(ERROR) << "get total buf: " << uint64_t(download_length / 1024)
                           << " MB" << endl;

                if (bytesRead == 0)
                {
                    break;
                }
                else
                {
                    file.write(buffer, bytesRead);
                    if (!file.good())
                    {
                        LOG(ERROR) << "create download file : "
                                   << path_manager::g_path_manager->download_zip_path
                                   << "failed" << endl;
                        err = ErrorType::inner_failed;
                        goto clean;
                    }
                }
            }
        }

        clean:

        if (nullptr != hRequest)
        {
            InternetCloseHandle(hRequest);
        }

        if (nullptr != hConnect)
        {
            InternetCloseHandle(hConnect);
        }

        if (nullptr != hInternet)
        {
            InternetCloseHandle(hInternet);
        }

        if (nullptr != async_info.hEvent)
        {
            CloseHandle(async_info.hEvent);
        }

        if (file.is_open())
        {
            file.close();
        }

        if (err != ErrorType::success)
        {
            throw CustomException{err};
        }

        LOG(INFO) << "download zip successfully" << endl;
    }

    VOID CALLBACK Download::DownloadCallback([[maybe_unused]] HINTERNET hInternet,
                                             DWORD_PTR dwContext,
                                             DWORD dwInternetStatus,
                                             LPVOID lpvStatusInformation,
                                             [[maybe_unused]] DWORD dwStatusInformationLength)
    {
        if (dwInternetStatus != INTERNET_STATUS_REQUEST_COMPLETE)
        {
            return;
        }
        cout<<99;
        auto pResult = reinterpret_cast<INTERNET_ASYNC_RESULT *>(lpvStatusInformation);
        auto pMonitor = reinterpret_cast<AsyncInfo *>(dwContext);

        if (pResult->dwError != S_OK)
        {
            LOG(ERROR) << "winlnet callback failed: " << pResult->dwError << endl;
            pMonitor->err = ErrorType::inner_failed;
            goto clean;
        }
//        const int bufferSize = 65535;
//        char buffer[bufferSize];
//        DWORD bytesRead = 0;

//        if (!InternetReadFile(pMonitor->hRequest, buffer, bufferSize, &bytesRead))
//        {
//            if (ERROR_IO_PENDING == GetLastError())
//            {
//                cout << 33;
//            }
//            else
//            {
//                cout << 55;
//            }
//        }

        _ReadWriteBarrier();
        clean:
        if (!SetEvent(pMonitor->hEvent))
        {
            LOG(ERROR) << "set event when download failed: " << GetLastError() << endl;
        }
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
        LOG(INFO) << "start to validate hash" << endl;

        HCRYPTPROV hProv{0};
        HCRYPTHASH hHash{0};
        auto err = ErrorType::success;
        string calculate_hash;
        array<BYTE, Download::md5_len> buf{};
        DWORD length = Download::md5_len;
        vector<unsigned char> contents;

        if (!CryptAcquireContext(&hProv,
                                 nullptr,
                                 nullptr,
                                 PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT)
                )
        {
            LOG(ERROR) << "prepare crypt context failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            LOG(ERROR) << "create handle of hash: md5 failed: " << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        try
        {
            contents = read_file(path_manager::g_path_manager->download_zip_path, true);
        }
        catch (const CustomException &e)
        {
            LOG(ERROR) << "calculate hash" << e.what() << endl;
            err = e.code();
            goto clean;
        }

        if (!CryptHashData(hHash, contents.data(), contents.size(), 0))
        {
            LOG(ERROR) << "create hash data for file: " << path_manager::g_path_manager->download_zip_path
                       << "failed" << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        if (!CryptGetHashParam(hHash, HP_HASHVAL, buf.data(), &length, 0))
        {
            LOG(ERROR) << "calculate file: " << path_manager::g_path_manager->download_zip_path
                       << "hash failed"
                       << GetLastError() << endl;
            err = ErrorType::inner_failed;
            goto clean;
        }

        for (int i = 0; i < length; i++)
        {
            calculate_hash += Download::hex_charset[((buf[i] >> 4))];
            calculate_hash += Download::hex_charset[((buf[i]) & 0x0F)];
        }

        std::transform(hash.begin(), hash.end(), hash.begin(),
                       [](unsigned char c)
                       {
                           return std::toupper(c);
                       }
        );

        if (calculate_hash != hash)
        {
            LOG(ERROR) << "calculate file: " << path_manager::g_path_manager->download_zip_path
                       << "hash is "
                       << calculate_hash
                       << " is different to ini hash: "
                       << hash
                       << endl;
            err = ErrorType::hash_diff;
            goto clean;
        }

        clean:

        if (0 != hHash)
        {
            CryptDestroyHash(hHash);
        }

        if (0 != hProv)
        {
            CryptReleaseContext(hProv, 0);
        }

        if (err != ErrorType::success)
        {
            throw CustomException{err};
        }

        LOG(INFO) << "validate hash successfully" << endl;
    }

    ErrorType Download::handle_wait_response(AsyncInfo &async_info)
    {
        try
        {
            wait_the_response(async_info);
        }
        catch (CustomException &e)
        {
            ResetEvent(async_info.hEvent);
            return e.code();
        }

        ResetEvent(async_info.hEvent);
        return ErrorType::success;
    }

    void Download::wait_the_response(AsyncInfo &async_info)
    {
        switch (WaitForSingleObject(async_info.hEvent, download::Download::get_timeout()))
        {
            case WAIT_OBJECT_0:
                if (async_info.err != ErrorType::success)
                {
                    throw CustomException{async_info.err};
                }
                break;
            case WAIT_TIMEOUT:
                LOG(ERROR) << "the server response is so slowly failed" << endl;
                throw CustomException{ErrorType::timeout};
            case WAIT_FAILED:
                LOG(ERROR) << "wait response failed: " << GetLastError() << endl;
                throw CustomException{ErrorType::inner_failed};
        }
    }
}
// download
