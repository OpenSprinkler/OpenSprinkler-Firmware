/*
  rf_lora.h - LORA long range driver for OpenSprinkler

  Copyright (C) 2020 Javier Arigita

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _RF_LORA_H
#define _RF_LORA_H

#if defined(ESP32) && defined(LORA_ENABLE)
void loraInit();
void loraMain();
#endif // defined(ESP32) && defined(LORA_ENABLE)

#endif	// _RF_LORA_H
