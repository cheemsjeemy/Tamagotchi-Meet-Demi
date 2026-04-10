#ifndef SONGS_H
#define SONGS_H

#include <string>
#include <vector>
#include <map>

struct Note {
    int pitch;      // MIDI note number (0-127). 0 = rest/silence
    float duration; // Beat fraction (1.0 = quarter note, 0.5 = eighth)
};

struct Song {
    int bpm;
    std::vector<Note> notes;
};

inline const std::map<std::string, Song> song_library = {
    {"ABC Song", {120, {
        {65, 1.0}, {0, 1.0}, {67, 1.0}, {0, 1.0},
        {65, 2.0}, {0, 2.0}
    }}},
    {"TheApple_Code", {120, {
        {60, 2.0}, {62, 1.0}, {64, 2.0}, {67, 3.0}
    }}}
};

#endif