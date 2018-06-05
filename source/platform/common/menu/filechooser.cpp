#include <dirent.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <platform/common/menu/filechooser.h>

#include "platform/common/menu/filechooser.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"

#define FLAG_DIRECTORY  1
#define FLAG_SUSPENDED  2
#define FLAG_ROM        4
#define FLAG_SPECIAL    8

FileChooser::FileChooser(std::function<void(bool, const std::string&)> finished, const std::string& directory, const std::vector<std::string>& extensions) {
    this->finished = finished;
    this->directory = directory;
    this->extensions = extensions;

    // If the path is to a file, open the parent directory.
    DIR* dir = nullptr;
    while((dir = opendir(this->directory.c_str())) == nullptr) {
        std::string::size_type slash = this->directory.rfind('/', this->directory.length());
        if(slash == std::string::npos) {
            this->directory = "/";
            break;
        }

        this->directory = this->directory.substr(0, slash);
    }

    if(dir != nullptr) {
        closedir(dir);
    }

    if(this->directory.length() == 0 || this->directory[this->directory.length() - 1] != '/') {
        this->directory += '/';
    }

    this->refreshContents();
}

bool FileChooser::processInput(UIKey key, u32 width, u32 height) {
    filesPerPage = height - 1;

    if(key == UI_KEY_A) {
        FileEntry& entry = files[selection];

        if(entry.flags & FLAG_DIRECTORY) {
            if((entry.flags & FLAG_SPECIAL) == FLAG_SPECIAL && entry.name == "..") {
                navigateBack();
            } else {
                directory += entry.name + "/";
                selection = 1;
            }

            refreshContents();

            return true;
        } else {
            if(finished != nullptr) {
                std::string path;
                if((entry.flags & FLAG_SPECIAL) == FLAG_SPECIAL && entry.name == "<use this directory>") {
                    path = directory;
                } else {
                    path = directory + entry.name;
                }

                finished(true, path);
            }

            menuPop();
        }
    } else if(key == UI_KEY_B) {
        if(directory == "/") {
            if(finished != nullptr) {
                finished(false, "");
            }

            menuPop();
            return false;
        }

        navigateBack();
        refreshContents();

        return true;
    } else if(key == UI_KEY_UP) {
        if(selection > 0) {
            selection--;
            updateScrollUp();

            return true;
        }
    } else if(key == UI_KEY_DOWN) {
        if(selection < files.size() - 1) {
            selection++;
            updateScrollDown();

            return true;
        }
    } else if(key == UI_KEY_RIGHT) {
        selection += filesPerPage / 2;
        if(selection >= files.size()) {
            selection = files.size() - 1;
        }

        updateScrollDown();

        return true;
    } else if(key == UI_KEY_LEFT) {
        if(selection > filesPerPage / 2) {
            selection -= filesPerPage / 2;
        } else {
            selection = 0;
        }

        updateScrollUp();

        return true;
    }

    return false;
}

void FileChooser::draw(u32 width, u32 height) {
    std::string currDirName;
    if(currDirName.length() > width) {
        currDirName = directory.substr(0, width);
    } else {
        currDirName = directory;
    }

    uiPrint("%s", currDirName.c_str());
    if(currDirName.length() < width) {
        uiPrint("\n");
    }

    for(u32 i = scrollY; i < scrollY + filesPerPage && i < files.size(); i++) {
        FileEntry& entry = files[i];

        if(i == selection) {
            uiSetLineHighlighted(true);
            uiPrint("* ");
        } else if(i == scrollY && i != 0) {
            uiPrint("^ ");
        } else if(i == scrollY + filesPerPage - 1 && scrollY + filesPerPage - 1 != files.size() - 1) {
            uiPrint("v ");
        } else {
            uiPrint("  ");
        }

        if(entry.flags & FLAG_DIRECTORY) {
            uiSetTextColor(TEXT_COLOR_YELLOW);
        } else if(entry.flags & FLAG_SUSPENDED) {
            uiSetTextColor(TEXT_COLOR_PURPLE);
        }

        std::string fileName = entry.name;
        if(entry.flags & FLAG_DIRECTORY) {
            fileName += "/";
        }

        u32 fileMax = width - 2;
        if(fileName.length() > fileMax) {
            fileName = fileName.substr(0, fileMax);
        }

        uiPrint("%s", fileName.c_str());
        if(fileName.length() < fileMax) {
            uiPrint("\n");
        }

        if(entry.flags & (FLAG_DIRECTORY | FLAG_SUSPENDED)) {
            uiSetTextColor(TEXT_COLOR_NONE);
        }

        if(i == selection) {
            uiSetLineHighlighted(false);
        }
    }
}

void FileChooser::updateScrollDown() {
    if(filesPerPage < files.size()) {
        if(selection == files.size() - 1) {
            scrollY = selection - filesPerPage + 1;
        } else if(selection >= scrollY + filesPerPage - 1) {
            scrollY = selection - filesPerPage + 2;
        }
    }
}

void FileChooser::updateScrollUp() {
    if(selection == 0) {
        scrollY = 0;
    } else if(selection <= scrollY) {
        scrollY = selection - 1;
    }
}

void FileChooser::navigateBack() {
    if(directory != "/") {
        initialSelection = directory.substr(0, directory.rfind('/', directory.length() - 2));

        directory = initialSelection + "/";
        selection = 1;
    }
}

bool FileChooser::compareFiles(FileEntry &a, FileEntry &b) {
    bool specialA = a.flags & FLAG_SPECIAL;
    bool specialB = b.flags & FLAG_SPECIAL;

    if(specialA && !specialB) {
        return true;
    } else if(!specialA && specialB) {
        return false;
    } else {
        bool dirA = a.flags & FLAG_DIRECTORY;
        bool dirB = b.flags & FLAG_DIRECTORY;

        if(dirA && !dirB) {
            return true;
        } else if(!dirA && dirB) {
            return false;
        } else {
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        }
    }
}

void FileChooser::refreshContents() {
    files.clear();

    if(directory != "/") {
        files.push_back({"..", FLAG_SPECIAL | FLAG_DIRECTORY});
    }

    if(extensions.empty()) {
        files.push_back({"<use this directory>", FLAG_SPECIAL});
    }

    DIR* dir = opendir(directory.c_str());
    if(dir != nullptr) {
        std::vector<std::string> unmatchedStates;

        // Read file list
        dirent* entry;
        while((entry = readdir(dir)) != nullptr) {
            std::string fullName = entry->d_name;

            if(fullName == "." || fullName == "..") {
                continue;
            }

            std::string name;
            std::string extension;

            std::string::size_type dotPos = fullName.rfind('.');
            if(dotPos != std::string::npos) {
                name = fullName.substr(0, dotPos);
                extension = fullName.substr(dotPos + 1);
            } else {
                name = fullName;
            }

            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            if((entry->d_type & DT_DIR) || std::find(extensions.begin(), extensions.end(), extension) != extensions.end()) {
                u8 flags = 0;

                if(entry->d_type & DT_DIR) {
                    flags |= FLAG_DIRECTORY;
                } else if(extension == "cgb" || extension == "gbc" || extension == "gb" || extension == "sgb") {
                    flags |= FLAG_ROM;

                    // Check for suspend state
                    if(!unmatchedStates.empty()) {
                        for(u32 i = 0; i < unmatchedStates.size(); i++) {
                            if(name == unmatchedStates[i]) {
                                flags |= FLAG_SUSPENDED;

                                unmatchedStates.erase(unmatchedStates.begin() + i);
                                break;
                            }
                        }
                    }
                }

                files.push_back({fullName, flags});
            } else if(extension == "yss" && !(entry->d_type & DT_DIR)) {
                bool matched = false;

                for(FileEntry& otherEntry : files) {
                    if(otherEntry.flags & FLAG_ROM) {
                        std::string otherName;

                        std::string::size_type otherDotPos = otherEntry.name.find('.');
                        if(otherDotPos != std::string::npos) {
                            otherName = otherEntry.name.substr(0, otherDotPos);
                        } else {
                            otherName = otherEntry.name;
                        }

                        if(otherName == name) {
                            otherEntry.flags |= FLAG_SUSPENDED;

                            matched = true;
                            break;
                        }
                    }
                }

                if(!matched) {
                    unmatchedStates.push_back(name);
                }
            }
        }

        closedir(dir);
    }

    std::sort(files.begin(), files.end(), compareFiles);

    if(selection >= files.size()) {
        selection = 0;
    }

    if(!initialSelection.empty()) {
        for(u32 i = 0; i < files.size(); i++) {
            if(initialSelection == files[i].name) {
                selection = i;
                break;
            }
        }

        initialSelection = "";
    }

    scrollY = 0;
    updateScrollDown();
}