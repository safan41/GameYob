#pragma once

#include "types.h"

#include "menu.h"

#include <functional>
#include <vector>

class FileChooser : public Menu {
public:
    FileChooser(std::function<void(bool, const std::string&)> finished, const std::string& directory, const std::vector<std::string>& extensions, bool canClear);

    bool processInput(UIKey key, u32 width, u32 height);
    void draw(u32 width, u32 height);
private:
    typedef struct {
        std::string name;
        u8 flags;
    } FileEntry;

    void updateScrollDown();
    void updateScrollUp();
    void navigateBack();

    static bool compareFiles(FileEntry &a, FileEntry &b);

    void refreshContents();

    std::function<void(bool, const std::string&)> finished;
    std::string directory;
    std::vector<std::string> extensions;
    bool canClear;

    std::vector<FileEntry> files;

    u32 selection = 0;
    u32 scrollY = 0;
    u32 filesPerPage = 24;

    std::string initialSelection = "";
};
