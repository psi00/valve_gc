#include "stdafx.h"
#include "vpk.h"

#include <cstdio>
#include <cstring>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

namespace Vpk
{

namespace
{

constexpr uint32_t kMagic = 0x55AA1234;

#ifdef _WIN32
int64_t SeekFile(FILE *file, int64_t offset)
{
    return _fseeki64(file, offset, SEEK_SET);
}

int64_t TellFile(FILE *file)
{
    return _ftelli64(file);
}
#else
int64_t SeekFile(FILE *file, int64_t offset)
{
    return fseeko(file, static_cast<off_t>(offset), SEEK_SET);
}

int64_t TellFile(FILE *file)
{
    return static_cast<int64_t>(ftello(file));
}
#endif
constexpr uint32_t kHeaderSizeV1 = 12;
constexpr uint32_t kHeaderSizeV2 = 28;

struct FileCloser
{
    void operator()(FILE *file) const
    {
        if (file)
        {
            fclose(file);
        }
    }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;

FilePtr OpenFile(const std::string &path)
{
    FILE *file = fopen(path.c_str(), "rb");
    return FilePtr{ file };
}

bool ReadAt(FILE *file, int64_t offset, void *dst, size_t size)
{
    if (!file || SeekFile(file, offset) != 0)
    {
        return false;
    }

    return fread(dst, 1, size, file) == size;
}

bool ReadUint32(FILE *file, uint32_t &value)
{
    return fread(&value, sizeof(value), 1, file) == 1;
}

// Reads a NUL terminated string from the directory tree.
bool ReadString(FILE *file, uint64_t end, std::string &out)
{
    out.clear();

    for (;;)
    {
        int64_t position = TellFile(file);
        if (position < 0 || static_cast<uint64_t>(position) >= end)
        {
            return false;
        }

        char c;
        if (fread(&c, 1, 1, file) != 1)
        {
            return false;
        }

        if (!c)
        {
            return true;
        }

        out.push_back(c);
    }
}

struct Entry
{
    uint16_t preloadBytes{};
    uint16_t archiveIndex{};
    uint32_t offset{};
    uint32_t length{};
};

} // namespace

std::string GameDir()
{
    // The GC is loaded into the game process with the working directory set to
    // <game>/bin/<arch>/, so "game/" is two levels up.  Resolve to an absolute
    // path once so later CWD changes do not matter.
    static const char *const kCandidates[] = {
        "../../",
        "../../../",
        "../",
        "",
    };

    static const std::string resolved = [] {
        for (const char *candidate : kCandidates)
        {
#ifdef _WIN32
            char buffer[MAX_PATH];
            if (!GetFullPathNameA(candidate, MAX_PATH, buffer, nullptr))
            {
                continue;
            }

            std::string path{ buffer };
#else
            char buffer[PATH_MAX];
            if (!realpath(candidate[0] ? candidate : ".", buffer))
            {
                continue;
            }

            std::string path{ buffer };
#endif
            if (path.empty() || path.back() != '/' && path.back() != '\\')
            {
                path.push_back('/');
            }

            FILE *probe = fopen((path + "csgo/pak01_dir.vpk").c_str(), "rb");
            if (probe)
            {
                fclose(probe);
                return path;
            }
        }

        return std::string{};
    }();

    return resolved;
}

std::string FindIndex()
{
    std::string gameDir = GameDir();
    if (gameDir.empty())
    {
        return {};
    }

    return gameDir + "csgo/pak01_dir.vpk";
}

bool ExtractFile(const std::string &indexPath, std::string_view entryPath, std::vector<uint8_t> &out)
{
    out.clear();

    FilePtr index = OpenFile(indexPath);
    if (!index)
    {
        return false;
    }

    uint32_t magic = 0, version = 0, treeSize = 0;
    if (!ReadUint32(index.get(), magic) || magic != kMagic)
    {
        return false;
    }

    if (!ReadUint32(index.get(), version) || (version != 1 && version != 2))
    {
        return false;
    }

    if (!ReadUint32(index.get(), treeSize))
    {
        return false;
    }

    uint64_t pos = (version == 1) ? kHeaderSizeV1 : kHeaderSizeV2;
    if (SeekFile(index.get(), static_cast<int64_t>(pos)) != 0)
    {
        return false;
    }

    const uint64_t treeEnd = pos + treeSize;
    const std::string_view dirName = entryPath.substr(0, entryPath.find_last_of('/'));
    const std::string_view fileName = entryPath.substr(entryPath.find_last_of('/') + 1);
    const std::string_view extension = fileName.substr(fileName.find_last_of('.') + 1);
    const std::string_view baseName = fileName.substr(0, fileName.find_last_of('.'));

    std::string currentExtension, currentPath, currentName;
    Entry found{};

    for (;;)
    {
        if (!ReadString(index.get(), treeEnd, currentExtension) || currentExtension.empty())
        {
            break;
        }

        for (;;)
        {
            if (!ReadString(index.get(), treeEnd, currentPath) || currentPath.empty())
            {
                break;
            }

            for (;;)
            {
                if (!ReadString(index.get(), treeEnd, currentName) || currentName.empty())
                {
                    break;
                }

                uint32_t crc = 0;
                Entry entry;
                char terminator = 0;

                if (!ReadUint32(index.get(), crc)
                    || fread(&entry.preloadBytes, sizeof(entry.preloadBytes), 1, index.get()) != 1
                    || fread(&entry.archiveIndex, sizeof(entry.archiveIndex), 1, index.get()) != 1
                    || fread(&entry.offset, sizeof(entry.offset), 1, index.get()) != 1
                    || fread(&entry.length, sizeof(entry.length), 1, index.get()) != 1
                    || fread(&terminator, 1, 1, index.get()) != 1)
                {
                    return false;
                }

                if (terminator != '\0')
                {
                    if (SeekFile(index.get(), TellFile(index.get()) + terminator) != 0)
                    {
                        return false;
                    }
                }

                if (currentPath == dirName
                    && currentName == baseName
                    && currentExtension == extension)
                {
                    found = entry;
                    found.preloadBytes = entry.preloadBytes;
                    goto done;
                }
            }
        }
    }

done:
    if (found.length == 0 && found.preloadBytes == 0)
    {
        return false;
    }

    out.resize(found.preloadBytes + found.length);
    if (found.preloadBytes)
    {
        // preload data lives inline in the directory file, right after the entry
        if (fread(out.data(), 1, found.preloadBytes, index.get()) != found.preloadBytes)
        {
            return false;
        }
    }

    if (found.length)
    {
        std::string archivePath = indexPath;
        const size_t dirPos = archivePath.find("_dir.vpk");
        if (dirPos == std::string::npos)
        {
            return false;
        }

        char suffix[16];
        snprintf(suffix, sizeof(suffix), "_%03u.vpk", found.archiveIndex);
        archivePath.replace(dirPos, strlen("_dir.vpk"), suffix);

        FilePtr archive = OpenFile(archivePath);
        if (!archive)
        {
            return false;
        }

        if (!ReadAt(archive.get(), found.offset, out.data() + found.preloadBytes, found.length))
        {
            return false;
        }
    }

    return true;
}

} // namespace Vpk

