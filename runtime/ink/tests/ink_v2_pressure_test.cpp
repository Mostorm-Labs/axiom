#include "canvas/ink/compact_brush_session.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    using namespace canvas;
    ink::CompactBrushSession slow({}, 42), fast({}, 42);
    for (std::uint64_t i = 0; i < 240; ++i) {
        const auto t = i * 8'333'333ULL;
        const ink::TimedBrushSample a{double(i) * .4, 0, .99, false, i+1, t};
        const ink::TimedBrushSample b{double(i) * 15, 0, .01, false, i+1, t};
        require(slow.append({&a,1}, {}), "simulated source rejected non-device input");
        require(fast.append({&b,1}, {}), "fast append rejected");
    }
    require(slow.latestPressure() - fast.latestPressure() >= .15, "speed must affect effective pressure");
    auto deviceConfig = ink::CompactBrushConfig{};
    deviceConfig.pressureSource = ink::BrushPressureSource::kDevice;
    ink::CompactBrushSession device(deviceConfig, 1);
    const std::array<ink::TimedBrushSample,2> invalid{{{0,0,.5,true,1,1},{1,0,.5,false,2,2}}};
    require(!device.append(invalid,{}), "missing pressure capability must reject batch");
    require(device.confirmedCount()==0, "invalid batch partially accepted");
    const ink::TimedBrushSample zero{0,0,0,true,1,1,true};
    require(device.append({&zero,1},{}), "explicit device zero rejected");
    require(device.latestPressure()==0, "device zero changed");
    ink::CompactBrushSession plain, speculative;
    const ink::TimedBrushSample start{0,0,0,false,1,1};
    const ink::TimedBrushSample prediction{100,0,0,false,90,900'000'001};
    const ink::TimedBrushSample next{8,0,0,false,2,10'000'001};
    require(plain.append({&start,1},{}) && speculative.append({&start,1},{&prediction,1}), "start rejected");
    require(plain.append({&next,1},{}) && speculative.append({&next,1},{}), "prediction advanced confirmed time");
    require(plain.latestPressure()==speculative.latestPressure(), "prediction changed confirmed pressure");
    const auto count=plain.confirmedCount();
    const std::array<ink::TimedBrushSample,2> backwards{{{9,0,0,false,3,20'000'001},{10,0,0,false,4,15'000'001}}};
    require(!plain.append(backwards,{}), "batch time rollback accepted");
    require(plain.confirmedCount()==count, "time rollback partially accepted");
    // A stationary terminal observation must not turn a thin tail into a dot.
    auto terminalConfig=ink::CompactBrushConfig{};terminalConfig.algorithmVersion=3;
    ink::CompactBrushSession released(terminalConfig);
    const std::array<ink::TimedBrushSample,2> run{{{0,0,0,false,1,1},{100,0,0,false,2,10'000'001}}};
    require(released.append(run,{}),"terminal fixture");const auto moving=released.latestPressure();
    const ink::TimedBrushSample stationary{100,0,0,false,3,210'000'001};
    require(released.append({&stationary,1},{}),"stationary release rejected");
    require(released.latestPressure()==moving,"stationary release increases simulated pressure");
    std::cout << "InkV2Pressure: simulated speed delta=" << slow.latestPressure()-fast.latestPressure() << '\n';
}
