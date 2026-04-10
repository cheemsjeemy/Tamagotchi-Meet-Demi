#ifndef MUSIC_MENU_H
#define MUSIC_MENU_H

#include <string>
#include <vector>

struct MusicMenuItem {
    const char* name;
    const char* songKey;
};

inline const std::vector<MusicMenuItem> musicMenuItems = {
    {"ABC Song", "ABC Song"},
    {"The Apple Code", "TheApple_Code"},
    {"Neon Drive", "Neon_Drive"}
};

#endif