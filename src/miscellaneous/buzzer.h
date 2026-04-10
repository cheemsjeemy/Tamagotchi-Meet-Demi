#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "../Demi/Play/Sing.h"

enum class PlayState {
    STOPPED,
    PLAYING,
    PAUSED
};

class Buzzer {
private:
    uint8_t pin;
    uint8_t channel;
    String currentSongName;
    const Song* currentSong;
    int currentNoteIndex;
    PlayState state;
    unsigned long noteStartTime;
    bool isPlayingNote;
    unsigned long noteDurationMs;

    int midiToFrequency(int midiNote);
    void playTone(int frequency);
    void stopTone();

public:
    Buzzer(uint8_t buzzerPin, uint8_t ledcChannel = 0);

    void update();

    void play(const String& songName);
    void pause();
    void resume();
    void restart();
    void stop();

    bool isPlaying();
    PlayState getState();
};

#endif