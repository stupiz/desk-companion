#include "mood_engine.h"

namespace mood {

MoodState resolveMood(air_quality::SeverityLevel severity) {
    MoodState mood;
    mood.severity = severity;

    switch (severity) {
        case air_quality::SeverityLevel::GREAT:
            mood.eyeMood = EyeMood::GREAT;
            mood.eyesOpen = "0 0";
            mood.eyesClosed = "- -";
            mood.mouth = " \\/ ";
            mood.label = "GREAT";
            mood.displayColor565 = 0x001F;  // blue
            mood.blinkMs = 850;
            break;
        case air_quality::SeverityLevel::NORMAL:
            mood.eyeMood = EyeMood::NORMAL;
            mood.eyesOpen = "o o";
            mood.eyesClosed = "- -";
            mood.mouth = " ___ ";
            mood.label = "NORMAL";
            mood.displayColor565 = 0x07E0;  // green
            mood.blinkMs = 900;
            break;
        case air_quality::SeverityLevel::STUFFY:
            mood.eyeMood = EyeMood::STUFFY;
            mood.eyesOpen = "u u";
            mood.eyesClosed = "- -";
            mood.mouth = " --- ";
            mood.label = "STUFFY";
            mood.displayColor565 = 0xFFE0;  // yellow
            mood.blinkMs = 950;
            break;
        case air_quality::SeverityLevel::BAD:
            mood.eyeMood = EyeMood::BAD;
            mood.eyesOpen = "x x";
            mood.eyesClosed = "_ _";
            mood.mouth = " --- ";
            mood.label = "BAD";
            mood.displayColor565 = 0xF800;  // red
            mood.blinkMs = 1000;
            break;
    }

    return mood;
}

const char* eyeMoodName(EyeMood moodState) {
    switch (moodState) {
        case EyeMood::GREAT:
            return "GREAT";
        case EyeMood::NORMAL:
            return "NORMAL";
        case EyeMood::STUFFY:
            return "STUFFY";
        case EyeMood::BAD:
            return "BAD";
        default:
            return "UNKNOWN";
    }
}

const char* severityToText(air_quality::SeverityLevel severity) {
    return air_quality::severityLabel(severity);
}

}  // namespace mood
