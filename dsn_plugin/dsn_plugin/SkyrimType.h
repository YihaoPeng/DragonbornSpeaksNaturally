#pragma once
#ifndef _SKYRIM_TYPE_H_
#define _SKYRIM_TYPE_H_

#include "common/IPrefix.h"
#include <string>

enum SkyrimType { AE, SE, VR };

extern SkyrimType g_SkyrimType;
extern SkyrimType g_DllType;

extern UInt64 SKYRIM_VERSION[3];

extern std::string SKYRIM_VERSION_STR[3];
extern std::string SKYRIM_VERSION_NAME[3];
extern std::string MOD_DIR_NAME[3];

#endif
