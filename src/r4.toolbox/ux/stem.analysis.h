//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//


namespace ImGui {
namespace ux {

// a debug analysis window giving waveform overview of stems and allowing us to tune things visually (eg. the beat/peak analysis)
void StemAnalysis( endlesss::live::RiffPtr& liveRiff, const int32_t audioSampleRate );

} // namespace ux
} // namespace ImGui
