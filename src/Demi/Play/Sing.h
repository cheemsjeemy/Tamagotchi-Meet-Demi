#ifndef SONGS_H
#define SONGS_H

#include <string>
#include <vector>
#include <map>

// 1. Define the "Package" for your song data
struct Song {
    int bpm;
    std::vector<int> notes;
};

// 2. Create the Map (The Catalog)
// Key = Song Title, Value = The Song Package
inline const std::map<std::string, Song> song_library = {
    {
        "TheApple_Code", 
        {120, {60, 62, 64, 67}} // BPM is 120, Notes follow
    },
    {
        "Neon_Drive", 
        {145, {72, 70, 72, 75}} // BPM is 145
    }
};

#endif
