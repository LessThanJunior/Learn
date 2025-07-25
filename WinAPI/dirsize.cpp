#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <format>
#include <iomanip>
#include <cstdint>

class Directory;

/*

std::wstring parseExtension(const std::wstring& path){
    std::wstring fileName = path;
    int pos = fileName.find(L'.');
    return fileName.substr(pos);
}
std::wstring parseFileName(const std::wstring& path){
    std::wstring fileName = path;
    int pos = fileName.find(L'.');
    return fileName.substr(0, pos);
}

*/


std::string formatBytes(int rank){
    static const std::array<std::string, 5> ranks{"B", "KB", "MB", "GB", "TB"};
    if(rank >= 0 && rank <= 5) return ranks[rank];
    return "";
}

class FileSystem{
    std::vector<Directory> directories;
    std::vector<Directory> files;
};

class File{
    std::vector<char> buffer;
    std::wstring fileName;
    std::wstring filePath;
    std::wstring extension;
    uint64_t fileSize = 0;
    HANDLE hFile;

    void parseNameAndExtension() {
        size_t pos = filePath.find_last_of(L"\\/");
        std::wstring fullName = (pos != std::wstring::npos) ? filePath.substr(pos + 1) : filePath;

        size_t dot = fullName.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            fileName = fullName.substr(0, dot);
            extension = fullName.substr(dot + 1);
        } else {
            fileName = fullName;
            extension = L"";
        }
    }
public:
    File(){}
    ~File(){
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }

    File(const std::wstring& path) : filePath(path) {
        parseNameAndExtension();

        hFile = CreateFileW(
            filePath.c_str(), 
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Error opening file: " << GetLastError() << std::endl;
            return;
        }

        constexpr DWORD BUFFER_SIZE = 4096;
        buffer.resize(BUFFER_SIZE);
        DWORD bytesRead = 0;

        while (ReadFile(hFile, buffer.data(), BUFFER_SIZE, &bytesRead, nullptr) && bytesRead > 0) {
            fileSize += bytesRead;
        }

        if (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_HANDLE_EOF) {
            std::cerr << "Error reading file: " << GetLastError() << std::endl;
            return;
        }
    }

    constexpr std::wstring getFilePath() const                  {return filePath;}
    constexpr std::wstring getFileName() const                  {return fileName;}
    constexpr std::wstring getExtension() const                 {return extension;}
    constexpr uint64_t getFileSize() const                      {return fileSize;}
    constexpr HANDLE getHandleFile() const                      {return hFile;}
    constexpr void setFileName(const std::wstring& fileName)    {this->fileName = fileName;}
    constexpr void setFilePath(const std::wstring& filePath)    {this->filePath = filePath;}
    constexpr void setExtension(const std::wstring& extension)  {this->extension = extension;}
    constexpr void setFileSize(uint64_t fileSize)               {this->fileSize = fileSize;}
    constexpr void setHandleFile(const HANDLE& hFile)           {this->hFile = hFile;}
};

class Directory{
    std::vector<File> files;
    std::vector<Directory> dirs;
    std::wstring filePath;
public:

    Directory() {}

    Directory(const std::wstring& path) {
        auto temp_files = getFilesFromDirectory(path);

        Directory* dir = new Directory();
        dir->setFiles(std::move(temp_files));
        dir->filePath = path;
        dirs.push_back(*dir);

        DirectoryScan(path);
    }

    std::vector<File> getFiles() const {return files;}
    std::vector<Directory> getDirectories(){return dirs;}

private:
    void DirectoryScan(const std::wstring& path){
        std::wstring searchPath = path;
        if (!searchPath.empty() && searchPath.back() != L'\\') {
            searchPath += L'\\';
        }
        searchPath += L'*'; // шаблон добавляем здесь

        int lengthPath = searchPath.length();
        WIN32_FIND_DATAW ffd;
        HANDLE handle = FindFirstFileW(searchPath.c_str(), &ffd);
        if (handle == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Ошибка: не удалось открыть " << searchPath << std::endl;
            return;
        }

        do {

            if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
            && wcscmp(ffd.cFileName, L".")
            && wcscmp(ffd.cFileName, L"..")){
                
                std::wstring directorySearchTemplate = searchPath.substr(0, lengthPath-1) + ffd.cFileName;

                Directory* dir = new Directory();
                dir->files = getFilesFromDirectory(directorySearchTemplate);
                dir->filePath = directorySearchTemplate + L'\\';
                dirs.push_back(*dir);
                DirectoryScan(directorySearchTemplate);
            }
        } while (FindNextFileW(handle, &ffd));
        
        FindClose(handle);
        //return *this;
    }

    std::vector<File> getFilesFromDirectory(const std::wstring& path){

        std::wstring searchPath = path;
        if (!searchPath.empty() && searchPath.back() != L'\\') {
            searchPath += L'\\';
        }
        searchPath += L'*'; // шаблон добавляем здесь

        int lengthPath = searchPath.length();
        WIN32_FIND_DATAW ffd;
        HANDLE handle = FindFirstFileW(searchPath.c_str(), &ffd);
        if (handle == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Ошибка: не удалось открыть " << searchPath << std::endl;

        }

        std::vector<File> temp_files;

        do {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ULARGE_INTEGER filesize;
                filesize.LowPart = ffd.nFileSizeLow;
                filesize.HighPart = ffd.nFileSizeHigh;

                File file;
                std::wstring fileName = ffd.cFileName;

                //std::wcout << "SearchPath: " << searchPath.substr(0, lengthPath-1) + ffd.cFileName << "\n\n";
                file.setFileName(ffd.cFileName);
                file.setFilePath(searchPath.substr(0, lengthPath-1) + ffd.cFileName);
                file.setFileSize(filesize.QuadPart);
                temp_files.push_back(file);
            }
        } while (FindNextFileW(handle, &ffd));
        
        FindClose(handle);
        return temp_files;
    }

    void setFiles(const std::vector<File>&& files){ this->files = files;}
};

int wmain(int argc, wchar_t* argv[]) {
    if(argc == 2){
        std::wstring wstr = argv[1];
        Directory directory(wstr);

        auto dirs = directory.getDirectories();
        std::wcout << dirs.size() << "\n";
        for(const auto& dir : dirs){
            for(const auto& file : dir.getFiles()){
                int rank = 0;
                uint64_t totalBytes = file.getFileSize();

                if (totalBytes > 0) {
                    rank = static_cast<int>(std::log(totalBytes) / std::log(1024));
                    if (rank > 4) rank = 4;
                }
                
                double bytesFormatted = totalBytes / std::pow(1024, rank);
                std::string byteType = formatBytes(rank);

                std::wcout  << "File: " << file.getFilePath()   << "\t"
                            << "Size: " << std::setprecision(3) << bytesFormatted
                            << std::wstring(byteType.begin(), byteType.end()) << "\n";
            }
        }
    }

    return 0;
}