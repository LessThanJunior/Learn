#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <format>
#include <iomanip>
#include <cstdint>
#include <algorithm>

class Directory;

std::wstring formatBytes(int rank){
    static const std::array<std::wstring, 5> ranks{L"B", L"KB", L"MB", L"GB", L"TB"};
    if(rank >= 0 && rank <= 5) return ranks[rank];
    return L"";
}

int getRankOfBytes(uint64_t totalBytes){
    int rank = 0;

    if (totalBytes > 0) {
        rank = static_cast<int>(std::log(totalBytes) / std::log(1024));
        if (rank > 4) rank = 4;
    }
    return rank;
}

std::wstring pathToDirectory(const std::wstring& path){
    std::wstring searchPath = path;
    if (!searchPath.empty() && searchPath.back() != L'\\') {
        searchPath += L'\\';
    }
    searchPath += L'*';
    return searchPath;
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

        int pathLength = pathToDirectory(path).length() - 1;
        dir->filePath = pathToDirectory(path).substr(0, pathLength);
        dirs.push_back(*dir);

        DirectoryScan(path);
    }

    std::vector<File> getFiles() const              {return files;}
    std::vector<Directory> getDirectories() const   {return dirs;}
    std::wstring getFilePath() const                {return filePath;}

private:
    void DirectoryScan(const std::wstring& path){
        std::wstring searchPath = pathToDirectory(path);

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
        std::wstring searchPath = pathToDirectory(path);

        int lengthPath = searchPath.length();
        WIN32_FIND_DATAW ffd;
        HANDLE handle = FindFirstFileW(searchPath.c_str(), &ffd);
        if (handle == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Ошибка: не удалось открыть " << searchPath << std::endl;
            throw std::wstring(L"Not found file or directory");
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
        Directory directory;

        try{
            directory = Directory(wstr);
        } catch(std::wstring e){
            std::wcout << e;
            return -1; 
        }
        
        auto dirs = directory.getDirectories();

        uint64_t totalBytesDirectories = 0;
        const std::size_t size = dirs.size();

        std::vector<std::pair<uint64_t, std::wstring>> pairDirectoryBytes;

        for(const auto& dir : dirs){
            uint64_t totalBytesDirectory = 0;

            for(const auto& file : dir.getFiles()){   
                uint64_t totalBytes = file.getFileSize();
                int rank = getRankOfBytes(totalBytes);

                double bytesFormatted = totalBytes / std::pow(1024, rank);
                std::wstring byteType = formatBytes(rank);
                /*
                std::wcout  << L"File: " << file.getFilePath()   << "\t"
                            << L"Size: " << std::setprecision(3) << bytesFormatted
                            << std::wstring(byteType.begin(), byteType.end()) << "\n";
                */
                totalBytesDirectory += totalBytes;
            }
            pairDirectoryBytes.push_back(std::pair(totalBytesDirectory, dir.getFilePath()));
            totalBytesDirectories += totalBytesDirectory;
        }

        int rankByDirectories = getRankOfBytes(totalBytesDirectories);
        double totalBytesDirectoriesFormatted = totalBytesDirectories / std::pow(1024, rankByDirectories);
        std::wstring byteType = formatBytes(rankByDirectories);

        std::wcout  << L"Directory size with subdirectories: " << std::setprecision(3) << totalBytesDirectoriesFormatted
                    << byteType << L"\n";

        std::wcout << "\n\n";

        std::sort(pairDirectoryBytes.begin(), pairDirectoryBytes.end(),
                [](const std::pair<uint64_t, std::wstring>& pairFirst, const std::pair<uint64_t, std::wstring>& pairSecond){
                    return pairFirst.first > pairSecond.first;
                });

        for (const auto& [bytes, path] : pairDirectoryBytes){
            double percent = bytes / (double)totalBytesDirectories * 100;
            int rank = getRankOfBytes(bytes);
            double totalBytesFormatted = bytes / std::pow(1024, rank);
            std::wstring byteType = formatBytes(rank);
            std::wcout  << L"Directory path: \"" << path << L"\"    "
                        << L"Percent of total: " << std::fixed << std::setprecision(3) << percent << L"%    "
                        << L"Size: " << totalBytesFormatted << byteType << "\n";
        }
        
    }

    else
        std::wcout << "usage: dirsize.exe \"your_path_to_directory\"";

    return 0;
}