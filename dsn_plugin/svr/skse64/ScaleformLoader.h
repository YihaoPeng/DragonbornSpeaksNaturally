#pragma once

#include "skse64/ScaleformState.h"

class NiTexture;
class IMenu;
class GImageInfoBase;
class GFxFile;
class GFxLog;

extern bool g_logScaleform;

class GFxImageLoader : public GFxState
{
public:
	virtual GImageInfoBase*	LoadImage(const char * url) = 0;
};

class GFxFileOpenerBase : public GFxState
{
public:
	GFxFileOpenerBase() { interfaceType = GFxState::kInterface_FileOpener; }

	virtual GFxFile* OpenFile(const char* url, int flags, int mode) = 0;
	virtual SInt64 GetFileModifyTime(const char* purl) = 0;
	virtual GFxFile* OpenFileEx(const char* url, GFxLog* plog, int flags, int mode) = 0;
};

class BSScaleformFileOpener : public GFxFileOpenerBase
{
public:
	virtual GFxFile* OpenFile(const char* url, int flags, int mode) { return OpenFile_Impl(url, flags, mode); }
	virtual SInt64 GetFileModifyTime(const char* purl) { return 0; }
	virtual GFxFile* OpenFileEx(const char* url, GFxLog* plog, int flags, int mode) { return OpenFileEx_Impl(url, plog, flags, mode); }

	DEFINE_MEMBER_FN_3(OpenFile_Impl, GFxFile*, 0x00F20AC0, const char* url, int flags, int mode);
	DEFINE_MEMBER_FN_4(OpenFileEx_Impl, GFxFile*, 0x00F20B40, const char* url, GFxLog* plog, int flags, int mode);
};

class BSScaleformImageLoader : public GFxImageLoader
{
public:
	virtual ~BSScaleformImageLoader();
	virtual GImageInfoBase*	LoadImage(const char * url);

	DEFINE_MEMBER_FN_1(AddVirtualImage, UInt8, 0x00FA2050, NiTexturePtr& texture);
	DEFINE_MEMBER_FN_1(ReleaseVirtualImage, UInt8, 0x00FA2520, NiTexturePtr& texture);
};

class GFxLoader
{
public:
	UInt32			unk00;					// 00
	GFxStateBag		* stateBag;				// 08
	void			* unk10;				// 10
	void			* unk18;				// 18
	BSScaleformImageLoader	* imageLoader;	// 20
	void			* unk28;				// 28
	void			* unk30;				// 30

	static GFxLoader * GetSingleton();	

	MEMBER_FN_PREFIX(GFxLoader);
	DEFINE_MEMBER_FN(ctor, GFxLoader *, 0x00F2ADC0);

	// Note: Probably in subclass
	DEFINE_MEMBER_FN(LoadMovie, bool, 0x00F2B480, IMenu* menu, GFxMovieView** viewOut, const char* name, int arg4, float arg5);
};
