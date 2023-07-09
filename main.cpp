#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <Windows.h>
#include <array>
#include <filesystem>
#include <Shldisp.h>
#include <unordered_set>
#include "aixlog.hpp"
#include "CLI11.hpp"

#undef ERROR
#undef max

using namespace std;
namespace fs = experimental::filesystem;

struct Resource
{
    uint16_t id;
    LPSTR rs_type;
};

Resource UPDATE{0x3333, RT_RCDATA};
Resource VERSION{0x3334, RT_STRING};

constexpr uint8_t MAX_BACKUP_PATH = 5;
constexpr DWORD MAX_LEGTH_WITH_NUL = MAX_PATH + 1;


constexpr CHAR GUARD_NAME[] = "Guard";
constexpr CHAR GUARD_UPDATE_NAME[] = "GuardUpdate";

fs::path X86_PATH;
fs::path GUARD_PATH;
fs::path GUARD_BACKUP_TEMP_PATH;
fs::path GUARD_BACKUP_PATH;
fs::path UPDATE_FILE = "update.zip";
fs::path VERSION_FILE = "version";

bool unzip(LPCWCH zipFile, const OLECHAR *destination)
{
    DWORD strlen = 0;
    HRESULT hResult;
    IShellDispatch *pISD;
    Folder *pToFolder = nullptr;
    Folder *pFromFolder = nullptr;
    FolderItems *pFolderItems = nullptr;

    VARIANT vDir, vFile, vOpt;
    BSTR strptr1, strptr2;
    CoInitialize(nullptr);

    bool bReturn = false;

    hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void **) &pISD);

    if (FAILED(hResult))
    {
        return bReturn;
    }

    VariantInit(&vOpt);
    vOpt.vt = VT_I4;
    vOpt.lVal = 16 + 4; // Do not display a progress dialog box ~ This will not work properly!

    strptr1 = SysAllocString(zipFile);
    strptr2 = SysAllocString(destination);

    VariantInit(&vFile);
    vFile.vt = VT_BSTR;
    vFile.bstrVal = strptr1;
    hResult = pISD->NameSpace(vFile, &pFromFolder);

    VariantInit(&vDir);
    vDir.vt = VT_BSTR;
    vDir.bstrVal = strptr2;

    hResult = pISD->NameSpace(vDir, &pToFolder);

    if (S_OK == hResult)
    {
        hResult = pFromFolder->Items(&pFolderItems);
        if (SUCCEEDED(hResult))
        {
            long lCount = 0;
            pFolderItems->get_Count(&lCount);
            IDispatch *pDispatch = nullptr;
            pFolderItems->QueryInterface(IID_IDispatch, (void **) &pDispatch);
            VARIANT vtDispatch;
            VariantInit(&vtDispatch);
            vtDispatch.vt = VT_DISPATCH;
            vtDispatch.pdispVal = pDispatch;

            hResult = pToFolder->CopyHere(vtDispatch, vOpt);
            if (hResult != S_OK)
                return false;

            FolderItems *pToFolderItems;
            hResult = pToFolder->Items(&pToFolderItems);

            if (S_OK == hResult)
            {
                long lCount2 = 0;

                hResult = pToFolderItems->get_Count(&lCount2);
                if (S_OK != hResult)
                {
                    pFolderItems->Release();
                    pToFolderItems->Release();
                    SysFreeString(strptr1);
                    SysFreeString(strptr2);
                    pISD->Release();
                    CoUninitialize();
                    return false;
                }
                bReturn = true;
            }

            pFolderItems->Release();
            pToFolderItems->Release();
        }

        pToFolder->Release();
        pFromFolder->Release();
    }

    SysFreeString(strptr1);
    SysFreeString(strptr2);
    pISD->Release();

    CoUninitialize();
    return bReturn;
}

bool set_path()
{
    array<CHAR, MAX_LEGTH_WITH_NUL> buf{};

    if (GetTempPath(MAX_PATH, buf.data()) == 0)
    {
        LOG(ERROR) << "get temp path failed: " << GetLastError() << endl;
        return false;
    }
    GUARD_BACKUP_TEMP_PATH = fs::path(buf.data()) / string(GUARD_NAME);

    auto program86Path = getenv("ProgramFiles(x86)");
    if (nullptr == program86Path)
    {
        LOG(ERROR) << "get program86Path failed" << endl;
        return false;
    }
    X86_PATH = fs::path(program86Path);
    GUARD_PATH = X86_PATH / GUARD_NAME;

    if (!(fs::exists(GUARD_PATH) && fs::is_directory(GUARD_PATH)))
    {
        LOG(ERROR) << "guard have not been installed yet" << endl;
        return false;
    }

    std::string version;
    try
    {
        ifstream version_file(GUARD_PATH / VERSION_FILE);
        if (!version_file)
        {
            LOG(WARNING) << "open version file failed" << endl;
        }
        else
        {
            getline(version_file, version);
            version_file.close();
        }
        if (version.empty())
            version = "Legacy";

        GUARD_BACKUP_PATH = GUARD_BACKUP_TEMP_PATH / version;
        fs::copy(GUARD_PATH, GUARD_BACKUP_PATH,
                 fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error &e)
    {
        LOG(ERROR) << "back up to path " << GUARD_BACKUP_PATH << " failed: " << e.what() << ": " << e.code();
        return false;
    }

    UPDATE_FILE = GUARD_BACKUP_TEMP_PATH / UPDATE_FILE;
    VERSION_FILE = GUARD_BACKUP_TEMP_PATH / VERSION_FILE;
    return true;
}

bool update_init()
{
    array<CHAR, MAX_LEGTH_WITH_NUL> buf{};

    if (GetTempPath(MAX_PATH, buf.data()) == 0)
    {
        LOG(ERROR) << "get temp path failed: " << GetLastError() << endl;
        return false;
    }
    GUARD_BACKUP_TEMP_PATH = fs::path(buf.data()) / string(GUARD_NAME);

    try
    {
        fs::create_directories(GUARD_BACKUP_TEMP_PATH);
    }
    catch (const fs::filesystem_error &e)
    {
        LOG(ERROR) << "create directory: " << GUARD_BACKUP_TEMP_PATH << " failed: " << e.what() << ": " << e.code();
        return false;
    }

    LOG(INFO) << "create directory: " << GUARD_BACKUP_TEMP_PATH << " successfully" << endl;

    auto program86Path = getenv("ProgramFiles(x86)");
    if (nullptr == program86Path)
    {
        LOG(ERROR) << "get program86Path failed" << endl;
        return false;
    }
    X86_PATH = fs::path(program86Path);
    GUARD_PATH = X86_PATH / GUARD_NAME;


    if (!(fs::exists(GUARD_PATH) && fs::is_directory(GUARD_PATH)))
    {
        LOG(ERROR) << "guard have not been installed yet" << endl;
        return false;
    }

    std::string version;
    try
    {
        ifstream version_file(GUARD_PATH / VERSION_FILE);
        if (!version_file)
        {
            LOG(WARNING) << "open version file failed" << endl;
        }
        else
        {
            getline(version_file, version);
            version_file.close();
        }
        if (version.empty())
            version = "Legacy";

        GUARD_BACKUP_PATH = GUARD_BACKUP_TEMP_PATH / version;
        fs::copy(GUARD_PATH, GUARD_BACKUP_PATH,
                 fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error &e)
    {
        LOG(ERROR) << "back up to path " << GUARD_BACKUP_PATH << " failed: " << e.what() << ": " << e.code();
        return false;
    }

    UPDATE_FILE = GUARD_BACKUP_TEMP_PATH / UPDATE_FILE;
    VERSION_FILE = GUARD_BACKUP_TEMP_PATH / VERSION_FILE;
    return true;
}

bool release_package(Resource rs, const experimental::filesystem::path &path)
{
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(rs.id), rs.rs_type);
    if (nullptr == hRes)
    {
        LOG(ERROR) << "find id: " << rs.id << "in resource file failed: " << GetLastError() << endl;
        return false;
    }

    HGLOBAL hResLoad = LoadResource(nullptr, hRes);
    if (nullptr == hResLoad)
    {
        LOG(ERROR) << "load id: " << rs.id << " form resource file failed: " << GetLastError() << endl;
        return false;
    }

    DWORD dwSize = SizeofResource(nullptr, hRes);
    if (dwSize == 0)
    {
        LOG(ERROR) << "get id: " << rs.id << " size in resource file failed: " << GetLastError() << endl;
        return false;
    }

    VOID *exeBuf = LockResource(hResLoad);
    if (nullptr == exeBuf)
    {
        LOG(ERROR) << "get id: " << rs.id << " address in resource file failed: " << GetLastError() << endl;
        return false;
    }

    ofstream file(path, ios::binary | ios::out);
    if (file.fail())
    {
        LOG(ERROR) << "open file: " << path << "failed" << endl;
        return false;
    }

    file.write(static_cast<char *>(exeBuf), dwSize);
    if (file.fail())
    {
        LOG(ERROR) << "write to file: " << path << "form resource file failed" << endl;
        return false;
    }
    file.close();

    return true;
}

void clean_update(bool clean_all)
{
    LOG(INFO) << "starting clean update dir";
    fs::remove(UPDATE_FILE);
    fs::remove(VERSION_FILE);

    if (clean_all)
    {
        if (!set_path())
        {
            cerr << "set dir var failed" << endl;
        }

        for (const auto &entry: fs::directory_iterator(GUARD_PATH))
        {
            fs::remove_all(entry.path());
        }
        cout << "clean all success" << endl;
    }
}


bool start_update()
{
    try
    {
        fs::copy(GUARD_PATH, GUARD_BACKUP_TEMP_PATH,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "copy guard to " << GUARD_BACKUP_TEMP_PATH << " failed: " << e.what() << " :" << e.code() << endl;
        return false;
    }

//    auto guard_start_path = GUARD_PATH / L"GuardApp.exe";
//    auto start_info = STARTUPINFOW{};
//    start_info.cb = sizeof(STARTUPINFOW);
//    start_info.wShowWindow = SW_HIDE;
//    start_info.dwFlags = STARTF_USESHOWWINDOW;
//    PROCESS_INFORMATION process_info{};
//
//    if (!CreateProcessW(nullptr, guard_start_path.generic_wstring().data(), nullptr, nullptr, FALSE, 0, nullptr,
//                        nullptr, &start_info,
//                        &process_info))
//    {
//        cerr << "create process info failed: " << GetLastError() << endl;
//        return false;
//    }

    return true;
}

bool roll_back_update(const string &version)
{
    if (!set_path())
    {
        cerr << "set dir var failed" << endl;
    }

    try
    {
        fs::remove_all(GUARD_PATH);
        fs::copy(GUARD_BACKUP_TEMP_PATH / version, X86_PATH,
                 fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);
        fs::rename(GUARD_BACKUP_TEMP_PATH / version, "GUARD");
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "failed: " << e.what() << " :" << e.code() << endl;
        return false;
    }
    return true;
};

bool update_guard()
{
    if (!update_init())
    {
        LOG(ERROR) << "update init successfully" << endl;
        goto clean;
    }

    if (release_package(UPDATE, UPDATE_FILE))
    {
        if (!unzip(UPDATE_FILE.c_str(), X86_PATH.c_str()))
        {
            LOG(ERROR) << "release update file failed" << endl;
            goto clean;
        }
    }
    else
        goto clean;

    LOG(ERROR) << "release update file successfully" << endl;
    if (!release_package(VERSION, VERSION_FILE))
    {
        LOG(ERROR) << "release version file failed" << endl;
        goto clean;
    }

//
//    if (!start_update())
//    {
//
//    }

    clean_update(false);
    return true;

    clean:
    clean_update(false);
    return false;
}

bool write_resource(const string &exe_path, const string &zip_path, const string &version)
{
    auto tmp_path = string(GUARD_UPDATE_NAME) + ".exe";
    fs::remove(tmp_path);

    try
    {
        fs::copy_file(exe_path, tmp_path);
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << "copy self: " << tmp_path << " failed: " << e.what() << " :" << e.code() << endl;
        return false;
    }

    ifstream in(zip_path, ios::binary);
    if (!in)
    {
        cerr << "open " << zip_path << " failed" << endl;
        return false;
    }

    vector<uint8_t> data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    if (!in)
    {
        cerr << "read " << zip_path << "incorrectly" << endl;
        return false;
    }
    in.close();

    HANDLE hUpdate = BeginUpdateResource(tmp_path.data(), FALSE);
    if (nullptr == hUpdate)
    {
        cerr << "start to write resource failed: " << GetLastError() << endl;
        return false;
    }

    if (!UpdateResource(hUpdate,
                        RT_RCDATA,
                        MAKEINTRESOURCE(UPDATE.id),
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                        data.data(),
                        data.size() + 1))
    {
        cerr << "write update zip failed: " << GetLastError() << endl;
        return false;
    }

    if (!UpdateResource(hUpdate,
                        RT_STRING,
                        MAKEINTRESOURCE(VERSION.id),
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPVOID) version.data(),
                        version.length() + 1))
    {
        cerr << "Failed to update string resource\n";
        return false;
    }

    if (!EndUpdateResource(hUpdate, FALSE))
    {
        cerr << "write zip to exe file failed: " << GetLastError() << endl;
        return false;
    }

    return true;
}


int main(int argc, char **argv)
{
    AixLog::Log::init(
            {
                    make_shared<AixLog::SinkFile>(AixLog::Severity::trace, "update.log",
                                                  "cout: %Y-%m-%d %H-%M-%S.#ms [#severity] #message"),
            });

    CLI::App app{"Guard update tools"};
    bool update{false};
    string path, version;

    try
    {
        CLI::App *add_cli = app.add_subcommand("add", "add resource to the tool")
                ->group("")
                ->callback([&argv, &path, &version]()
                           {
                               cout << "start writing resource ..." << endl;
                               if (!write_resource(argv[0], path, version))
                               {
                                   return 1;
                               }
                               cout << "write resource successfully" << endl;
                               return 0;
                           });

        app.add_subcommand("clean", "clean the cache of old version")
                ->callback([]()
                           {
                               clean_update(true);
                           });

        CLI::App *rollback_cli = app.add_subcommand("back", "roll back to the specific version")
                ->callback([&version]()
                           {
                               cout << "start roll back ..." << endl;
                               if (!roll_back_update(version))
                               {
                                   return 1;
                               }
                               cout << "roll back successfully" << endl;
                               return 0;
                           });

        app.add_subcommand("update", "update the guard")
                ->callback([]()
                           {
                               LOG(INFO) << "start to update" << endl;
                               if (!update_guard())
                               {
                                   return 1;
                               }
                               return 0;
                           });

        app.require_subcommand();


        add_cli->add_option("-a,--add", path, "add resource to tool")
                ->option_text("FILE")
                ->group("")
                ->required()
                ->check([](const string &path)
                        {
                            if (!fs::exists(path))
                            {
                                return "update file is not exist";
                            }
                            return "";
                        });

        add_cli->add_option("-v,--version", version, "add version info")
                ->option_text("VERSION")
                ->group("")
                ->required();

        rollback_cli->add_option("-v,--version", version, "add version info")
                ->option_text("VERSION")
                ->required();

        app.parse(argc, argv);
    } catch (const CLI::Error &e)
    {
        cerr << e.get_name() << ": " << e.what() << endl;
        cout << app.help() << endl;
        return 1;
    }

    return 0;
}