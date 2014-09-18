// Weather.h
// This file manages the retrieval of Weather related information and adjustment of durations
//   from Weather Underground
// Author: Richard Zimmerman
// Copyright (c) 2013 Richard Zimmerman
// 
// Sep, 2014
// Modified by Ray Wang to fit OpenSprinkler 

#ifndef _WEATHER_h
#define _WEATHER_h

struct WeatherVals
{
	short minhumidity;
	short maxhumidity;
	short meantempi;
	short precip_today;
	short precipi;
	short windmph;
	uint16_t sunrise;
	uint16_t sunset;
	bool  valid;
};

#endif
