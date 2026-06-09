#pragma once

#include <LovyanGFX.hpp>

namespace ui::runway {

void drawLargeAirportRunways(lgfx::LGFXBase& gfx);

/** Faint coastline/water underlay from OSM; draw before the grid. No-op if off. */
void drawMapContours(lgfx::LGFXBase& gfx);

}  // namespace ui::runway
