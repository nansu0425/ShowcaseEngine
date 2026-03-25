#pragma once

#ifdef SHOWCASE_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#define SE_FRAME_MARK FrameMark
#define SE_ZONE_SCOPED ZoneScoped
#define SE_ZONE_SCOPED_N(name) ZoneScopedN(name)
#define SE_PLOT(name, val) TracyPlot(name, val)
#define SE_MESSAGE(text, len) TracyMessage(text, len)
#else
#define SE_FRAME_MARK (void)0
#define SE_ZONE_SCOPED (void)0
#define SE_ZONE_SCOPED_N(name) (void)0
#define SE_PLOT(name, val) (void)0
#define SE_MESSAGE(text, len) (void)0
#endif
