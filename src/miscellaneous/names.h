#ifndef NAMES_H
#define NAMES_H 
#include <Arduino.h>

const char* const adjectives[] PROGMEM = {
"Nova","Echo","Luna","Aero",
"Soft","Mellow","Calm","Vivid",
"Pixel","Orbit","Drift","Zen",
"Glow","Sunny","Prism","Velvet"
};

const char* const nouns[] PROGMEM = {
"Core","Byte","Wave","Cloud",
"Bloom","Pulse","Node","Sky",
"Frame","Loop","Glow","Axis",
"Link","Dash","Spark","Field"
};

const int adjCount = sizeof(adjectives) / sizeof(adjectives[0]);
const int nounCount = sizeof(nouns) / sizeof(nouns[0]);

String generate(const uint8_t* mac){

    String adj = String((char*)pgm_read_ptr(&adjectives[mac[5] % adjCount]));
    String noun = String((char*)pgm_read_ptr(&nouns[mac[4] % nounCount]));

    char c1 = (char)('A' + (mac[5] / 10 % 26));
    char c2 = (char)('A' + (mac[5] % 26));
    
    return adj + noun + "-"+ c1 + c2;

}

#endif