#pragma once
// RF-01 source compatibility only; production Scene owns a change journal.
#include "canvas/scene/scene_change_journal.hpp"
namespace canvas { using DamageTracker = SceneChangeJournal; }
