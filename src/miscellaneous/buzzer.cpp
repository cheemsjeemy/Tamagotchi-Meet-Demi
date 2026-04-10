#include "miscellaneous/buzzer.h"
#include <cmath>

Buzzer::Buzzer(uint8_t buzzerPin, uint8_t ledcChannel) 
    : pin(buzzerPin), channel(ledcChannel), currentSong(nullptr), 
      currentNoteIndex(0), state(PlayState::STOPPED), 
      noteStartTime(0), isPlayingNote(false), noteDurationMs(0) {
}

int Buzzer::midiToFrequency(int midiNote) {
    if (midiNote <= 0) return 0;
    return (int)(440.0 * pow(2.0, (midiNote - 69) / 12.0));
}

double FrequencySum(double a, double b, double c) {
    double y = std::sin(a) + std::sin(b) + std::sin(c);
    return y;
}


void Buzzer::playTone(int frequency) {
    if (frequency <= 0) {
        stopTone();
        return;
    }

    uint8_t dutyResolution;
    if (frequency < 100) {
        dutyResolution = 12;
    } else if (frequency < 200) {
        dutyResolution = 10;
    } else if (frequency < 400) {
        dutyResolution = 8;
    } else {
        dutyResolution = 8;
    }

    ledcSetup(channel, frequency, dutyResolution);
    ledcAttachPin(pin, channel);
    uint16_t halfDuty = (1 << dutyResolution) / 2;
    ledcWrite(channel, halfDuty);
}

void Buzzer::stopTone() {
    ledcWrite(channel, 0);
}

void Buzzer::update() {
    if (state != PlayState::PLAYING || currentSong == nullptr) {
        return;
    }

    const auto& notes = currentSong->notes;
    if (notes.empty() || currentNoteIndex >= notes.size()) {
        state = PlayState::STOPPED;
        stopTone();
        return;
    }

    if (!isPlayingNote) {
        const Note& note = notes[currentNoteIndex];
        noteDurationMs = (int)((note.duration / currentSong->bpm) * 60000.0);
        noteStartTime = millis();
        isPlayingNote = true;

        if (note.pitch > 0) {
            playTone(midiToFrequency(note.pitch));
        } else {
            stopTone();
        }
    } else {
        unsigned long elapsed = millis() - noteStartTime;
        if (elapsed >= noteDurationMs) {
            stopTone();
            currentNoteIndex++;
            isPlayingNote = false;

            if (currentNoteIndex >= notes.size()) {
                state = PlayState::STOPPED;
                currentNoteIndex = 0;
            }
        }
    }
}

void Buzzer::play(const String& songName) {
    std::string key = songName.c_str();
    auto it = song_library.find(key);
    if (it == song_library.end()) {
        return;
    }

    currentSongName = songName;
    currentSong = &it->second;
    currentNoteIndex = 0;
    state = PlayState::PLAYING;
    isPlayingNote = false;
}

void Buzzer::pause() {
    if (state == PlayState::PLAYING) {
        state = PlayState::PAUSED;
        stopTone();
    }
}


void Buzzer::resume() {
    if (state == PlayState::PAUSED) {
        state = PlayState::PLAYING;
    }
}

void Buzzer::restart() {
    currentNoteIndex = 0;
    isPlayingNote = false;
    state = PlayState::PLAYING;
}

void Buzzer::stop() {
    stopTone();
    currentNoteIndex = 0;
    isPlayingNote = false;
    state = PlayState::STOPPED;
    currentSong = nullptr;
    currentSongName = "";
}

bool Buzzer::isPlaying() {
    return state == PlayState::PLAYING;
}

PlayState Buzzer::getState() {
    return state;
}