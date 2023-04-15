#include "SkyrimType.h"

SkyrimType g_SkyrimType;

#ifdef IS_AE
SkyrimType g_DllType(AE);
#endif
#ifdef IS_SE
SkyrimType g_DllType(SE);
#endif
#ifdef IS_VR
SkyrimType g_DllType(VR);
#endif

UInt64 SKYRIM_VERSION[3] = {
	0x0001000602800000,  // SkyrimAE
	0x0001000500610000,  // SkyrimSE
	0x00010004000F0000   // SkyrimVR
};

std::string SKYRIM_VERSION_STR[3] = {
	"1.6.640.0",
	"1.5.97.0",
	"1.4.15.0"
};

std::string SKYRIM_VERSION_NAME[3] = {
	"Skyrim Anniversary Edition",
	"Skyrim Special Edition",
	"Skyrim VR"
};

std::string MOD_DIR_NAME[3] = {
	"SkyrimAE",
	"SkyrimSE",
	"SkyrimVR"
};
