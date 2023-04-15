
#include <cstring>
#include <cinttypes>
#include "common/IPrefix.h"
#include "skse64_common/SafeWrite.h"
#include "skse64/ScaleformAPI.h"
#include "skse64/ScaleformMovie.h"
#include "skse64/ScaleformValue.h"
#include "skse64/GameEvents.h"
#include "skse64/GameInput.h"
#include "skse64_common/BranchTrampoline.h"
#include "xbyak.h"
#include "SkyrimType.h"
#include "Hooks.h"
#include "Log.h"
#include "ConsoleCommandRunner.h"
#include "FavoritesMenuManager.h"

static GFxMovieView* dialogueMenu = NULL;
static int desiredTopicIndex = 1;
static int numTopics = 0;
static int lastMenuState = -1;
typedef void executeCommand(UInt32* unk01, void* parser, char* command);
void InitDebugEventSink();

static void __cdecl Hook_Loop()
{
	if (dialogueMenu != NULL)
	{
		// Menu exiting, avoid NPE
#ifdef IS_VR
		if (dialogueMenu->GetPlayState() == 0)
#else
		if (dialogueMenu->GetPause() == 0)
#endif
		{
			dialogueMenu = NULL;
			SpeechRecognitionClient::getInstance()->StopDialogue();
			return;
		}
		GFxValue stateVal;
		dialogueMenu->GetVariable(&stateVal, "_level0.DialogueMenu_mc.eMenuState");
		int menuState = stateVal.data.number;
		desiredTopicIndex = SpeechRecognitionClient::getInstance()->ReadSelectedIndex();
		if (menuState != lastMenuState) {

			lastMenuState = menuState;
			if (menuState == 2) // NPC Responding
			{
				// fix issue #11 (SSE crash when teleport with a dialogue line).
				// It seems no side effects have been found at present.
				dialogueMenu = NULL;
				SpeechRecognitionClient::getInstance()->StopDialogue();
				return;
			}
		}
		if (desiredTopicIndex >= 0) {
			GFxValue topicIndexVal;
			dialogueMenu->GetVariable(&topicIndexVal, "_level0.DialogueMenu_mc.TopicList.iSelectedIndex");

			int currentTopicIndex = topicIndexVal.data.number;
			if (currentTopicIndex != desiredTopicIndex) {

				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.SetSelectedTopic", NULL, "%d", desiredTopicIndex);
				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.doSetSelectedIndex", NULL, "%d", desiredTopicIndex);
				dialogueMenu->Invoke("_level0.DialogueMenu_mc.TopicList.UpdateList", NULL, NULL, 0);
			}

			dialogueMenu->Invoke("_level0.DialogueMenu_mc.onSelectionClick", NULL, "%d", 1.0);
		}
		else if (desiredTopicIndex == -2) { // Indicates a "goodbye" phrase was spoken, hide the menu
			dialogueMenu->Invoke("_level0.DialogueMenu_mc.StartHideMenu", NULL, NULL, 0);
		}
	}
	else
	{
		std::string command = SpeechRecognitionClient::getInstance()->PopCommand();
		if (command != "") {
			ConsoleCommandRunner::RunCommand(command);
			Log::info("run command: " + command);
		}

			FavoritesMenuManager::getInstance()->ProcessEquipCommands();
		}
	}

class RunCommandSink : public BSTEventSink<InputEvent> {
	EventResult ReceiveEvent(InputEvent ** evnArr, InputEventDispatcher * dispatcher) override {
		Hook_Loop();
		return kEvent_Continue;
	}
};

class ObjectLoadedSink : public BSTEventSink<TESObjectLoadedEvent> {
	EventResult ReceiveEvent(TESObjectLoadedEvent* evn, EventDispatcher<TESObjectLoadedEvent>* dispatcher) override {
		if (evn != nullptr && evn->formId==0x00000014 /*player*/) {
			if (evn->loaded) {
				FavoritesMenuManager::getInstance()->RefreshFavorites();
				Log::info("Favorites Menu Voice-Equip Initialized");
				//InitDebugEventSink();
			}
			else {
				FavoritesMenuManager::getInstance()->ClearFavorites();
				Log::info("Favorites Menu Voice-Equip Disabled");
			}
		}
		return kEvent_Continue;
	}
};

#ifndef IS_VR
class PostLoadSink : public BSTEventSink<TESLoadGameEvent> {
public:
	EventResult ReceiveEvent(TESLoadGameEvent* evn, EventDispatcher<TESLoadGameEvent>* dispatcher) override {
		FavoritesMenuManager::getInstance()->RefreshFavorites();
		Log::info("Favorites Menu Voice-Equip Updated");
		return kEvent_Continue;
	}
};
#endif

// For debug only
//#define DEBUG_EVENT_SINK_LOG_DIFF_MAP
#define DEBUG_EVENT_SINK_PRINT_EACH
#define DEBUG_EVENT_SINK_PRINT_INTERVAL 5000

template <typename T>
class DebugEventSink : public BSTEventSink<T> {
  std::string name_;
  DWORD beginTime_ = 0;
  DWORD time_ = 0;
  DWORD count_ = 0;
  std::map<DWORD, DWORD> diffMap_;

  DWORD printTime_ = 0;
  DWORD printCount_ = 0;

public:
  DebugEventSink(const std::string& name)
    : name_(name)
    , beginTime_(GetTickCount())
    , time_(beginTime_)
    , printTime_(beginTime_){
    Log::info("AddEventSink " + name_);
  }

  EventResult ReceiveEvent(T* evn, EventDispatcher<T>* dispatcher) override {
    //Hook_Loop();

    DWORD now = GetTickCount();
    DWORD diff = now - time_;
    time_ = now;
    count_++;
    printCount_++;

#ifdef DEBUG_EVENT_SINK_PRINT_EACH
    Log::info("ReceiveEvent " + name_ + ": " + std::to_string(count_) + ", " + std::to_string(diff) + "ms");
#else
    if (now - printTime_ >= DEBUG_EVENT_SINK_PRINT_INTERVAL) {
      DWORD avgInterval = (now - printTime_) / printCount_;
      Log::info("ReceiveEvent " + name_ + ": " + std::to_string(printCount_) + ", " + std::to_string(avgInterval) + "ms");
      printTime_ = now;
      printCount_ = 0;
    }
#endif

#ifdef DEBUG_EVENT_SINK_LOG_DIFF_MAP
    if (diff <= 20) {
      diff -= diff % 5;
    }
    else if (diff <= 50) {
      diff -= diff % 10;
    }
    else if (diff <= 500) {
      diff -= diff % 100;
    }
    else if (diff <= 2000) {
      diff -= diff % 500;
    }
    else {
      diff -= diff % 1000;
    }
    diffMap_[diff]++;
#endif

    return kEvent_Continue;
  }

  ~DebugEventSink() {
    if (count_ > 0) {
      DWORD now = GetTickCount();
      DWORD avgInterval = (now - beginTime_) / count_;
      Log::info("RemoveEventSink " + name_ + ": " + std::to_string(count_) + ", " + std::to_string(avgInterval) + "ms");

#ifdef DEBUG_EVENT_SINK_LOG_DIFF_MAP
      for (auto itr : diffMap_) {
        Log::info("\t" + std::to_string(itr.first) + "ms: " + std::to_string(itr.second));
      }
#endif
    }
    else {
      Log::info("RemoveEventSink " + name_ + ": " + std::to_string(count_));
    }
  }
};

static void InitDebugEventSink() {
  static DebugEventSink<void>                            unk00("unk00");	GetEventDispatcherList()->unk00.AddEventSink(&unk00);
  //static DebugEventSink<void>                            unk58("unk58");	GetEventDispatcherList()->unk58.AddEventSink(&unk58);
  //static DebugEventSink<TESActiveEffectApplyRemoveEvent> unkB0("unkB0");	GetEventDispatcherList()->unkB0.AddEventSink(&unkB0);
  //static DebugEventSink<void>                            unk108("unk108");	GetEventDispatcherList()->unk108.AddEventSink(&unk108);
  static DebugEventSink<void>                            unk160("unk160");	GetEventDispatcherList()->unk160.AddEventSink(&unk160);
  //static DebugEventSink<TESCellAttachDetachEvent>        unk1B8("unk1B8");	GetEventDispatcherList()->unk1B8.AddEventSink(&unk1B8);
  //static DebugEventSink<void>                            unk210("unk210");	GetEventDispatcherList()->unk210.AddEventSink(&unk210);
  //static DebugEventSink<void>                            unk2C0("unk2C0");	GetEventDispatcherList()->unk2C0.AddEventSink(&unk2C0);
  //static DebugEventSink<TESCombatEvent>                  combatDispatcher("combatDispatcher");	GetEventDispatcherList()->combatDispatcher.AddEventSink(&combatDispatcher);
  //static DebugEventSink<TESContainerChangedEvent>        unk370("unk370");	GetEventDispatcherList()->unk370.AddEventSink(&unk370);
  //static DebugEventSink<TESDeathEvent>                   deathDispatcher("deathDispatcher");	GetEventDispatcherList()->deathDispatcher.AddEventSink(&deathDispatcher);
  static DebugEventSink<void>                            unk420("unk420");	GetEventDispatcherList()->unk420.AddEventSink(&unk420);
  static DebugEventSink<void>                            unk478("unk478");	GetEventDispatcherList()->unk478.AddEventSink(&unk478);
  //static DebugEventSink<void>                            unk4D0("unk4D0");	GetEventDispatcherList()->unk4D0.AddEventSink(&unk4D0);
  //static DebugEventSink<void>                            unk528("unk528");	GetEventDispatcherList()->unk528.AddEventSink(&unk528);
  //static DebugEventSink<void>                            unk580("unk580");	GetEventDispatcherList()->unk580.AddEventSink(&unk580);
  static DebugEventSink<void>                            unk5D8("unk5D8");	GetEventDispatcherList()->unk5D8.AddEventSink(&unk5D8);
  static DebugEventSink<void>                            unk630("unk630");	GetEventDispatcherList()->unk630.AddEventSink(&unk630);
  //static DebugEventSink<TESInitScriptEvent>              initScriptDispatcher("initScriptDispatcher");	GetEventDispatcherList()->initScriptDispatcher.AddEventSink(&initScriptDispatcher);
  //static DebugEventSink<void>                            unk6E0("unk6E0");	GetEventDispatcherList()->unk6E0.AddEventSink(&unk6E0); // Archive loading completed
  //static DebugEventSink<void>                            unk738("unk738");	GetEventDispatcherList()->unk738.AddEventSink(&unk738);
  //static DebugEventSink<void>                            unk790("unk790");	GetEventDispatcherList()->unk790.AddEventSink(&unk790);
  static DebugEventSink<void>                            unk7E8("unk7E8");	GetEventDispatcherList()->unk7E8.AddEventSink(&unk7E8);
  //static DebugEventSink<void>                            unk840("unk840");	GetEventDispatcherList()->unk840.AddEventSink(&unk840);
  //static DebugEventSink<TESObjectLoadedEvent>            objectLoadedDispatcher("objectLoadedDispatcher");	GetEventDispatcherList()->objectLoadedDispatcher.AddEventSink(&objectLoadedDispatcher);
  //static DebugEventSink<void>                            unk8F0("unk8F0");	GetEventDispatcherList()->unk8F0.AddEventSink(&unk8F0);
  static DebugEventSink<void>                            unk948("unk948");	GetEventDispatcherList()->unk948.AddEventSink(&unk948);
  //static DebugEventSink<void>                            unk9A0("unk9A0");	GetEventDispatcherList()->unk9A0.AddEventSink(&unk9A0);
  static DebugEventSink<void>                            unk9F8("unk9F8");	GetEventDispatcherList()->unk9F8.AddEventSink(&unk9F8); // Open/Close a door
  //static DebugEventSink<void>                            unkA50("unkA50");	GetEventDispatcherList()->unkA50.AddEventSink(&unkA50);
  //static DebugEventSink<void>                            unkAA8("unkAA8");	GetEventDispatcherList()->unkAA8.AddEventSink(&unkAA8);
  //static DebugEventSink<void>                            unkB00("unkB00");	GetEventDispatcherList()->unkB00.AddEventSink(&unkB00);
  //static DebugEventSink<void>                            unkB58("unkB58");	GetEventDispatcherList()->unkB58.AddEventSink(&unkB58);
  static DebugEventSink<void>                            unkBB0("unkBB0");	GetEventDispatcherList()->unkBB0.AddEventSink(&unkBB0);
  //static DebugEventSink<void>                            unkC08("unkC08");	GetEventDispatcherList()->unkC08.AddEventSink(&unkC08);
  //static DebugEventSink<void>                            unkC60("unkC60");	GetEventDispatcherList()->unkC60.AddEventSink(&unkC60);
  static DebugEventSink<void>                            unkCB8("unkCB8");	GetEventDispatcherList()->unkCB8.AddEventSink(&unkCB8);
  //static DebugEventSink<void>                            unkD10("unkD10");	GetEventDispatcherList()->unkD10.AddEventSink(&unkD10);
  static DebugEventSink<void>                            unkD68("unkD68");	GetEventDispatcherList()->unkD68.AddEventSink(&unkD68);
  static DebugEventSink<void>                            unkDC0("unkDC0");	GetEventDispatcherList()->unkDC0.AddEventSink(&unkDC0);
  static DebugEventSink<void>                            unkE18("unkE18");	GetEventDispatcherList()->unkE18.AddEventSink(&unkE18);
  static DebugEventSink<void>                            unkE70("unkE70");	GetEventDispatcherList()->unkE70.AddEventSink(&unkE70);
  static DebugEventSink<void>                            unkEC8("unkEC8");	GetEventDispatcherList()->unkEC8.AddEventSink(&unkEC8);
  //static DebugEventSink<void>                            unkF20("unkF20");	GetEventDispatcherList()->unkF20.AddEventSink(&unkF20);
  static DebugEventSink<void>                            unkF78("unkF78");	GetEventDispatcherList()->unkF78.AddEventSink(&unkF78);
  static DebugEventSink<void>                            unkFD0("unkFD0");	GetEventDispatcherList()->unkFD0.AddEventSink(&unkFD0);
  //static DebugEventSink<void>                            unk1028("unk1028");	GetEventDispatcherList()->unk1028.AddEventSink(&unk1028);
  //static DebugEventSink<void>                            unk1080("unk1080");	GetEventDispatcherList()->unk1080.AddEventSink(&unk1080);
  //static DebugEventSink<void>                            unk10D8("unk10D8");	GetEventDispatcherList()->unk10D8.AddEventSink(&unk10D8);
  //static DebugEventSink<void>                            unk1130("unk1130");	GetEventDispatcherList()->unk1130.AddEventSink(&unk1130);
  static DebugEventSink<void>                            unk1188("unk1188");	GetEventDispatcherList()->unk1188.AddEventSink(&unk1188); // Before waiting
  static DebugEventSink<void>                            unk11E0("unk11E0");	GetEventDispatcherList()->unk11E0.AddEventSink(&unk11E0); // After waiting
  //static DebugEventSink<void>                            unk1238("unk1238");	GetEventDispatcherList()->unk1238.AddEventSink(&unk1238);
  //static DebugEventSink<TESUniqueIDChangeEvent>          uniqueIdChangeDispatcher("uniqueIdChangeDispatcher");	GetEventDispatcherList()->uniqueIdChangeDispatcher.AddEventSink(&uniqueIdChangeDispatcher);
}

static void __cdecl Hook_Invoke(GFxMovieView* movie, char * gfxMethod, GFxValue* argv, UInt32 argc)
{
#ifndef IS_VR
	static bool inited = false;
	if (!inited) {
    // Currently in the source code directory is the latest version of SKSE64 instead of SKSEVR,
    // so we can call GetSingleton() directly instead of use a RelocAddr.
    static RunCommandSink runCommandSink;
    InputEventDispatcher::GetSingleton()->AddEventSink(&runCommandSink);

    static ObjectLoadedSink objectLoadedSink;
    GetEventDispatcherList()->objectLoadedDispatcher.AddEventSink(&objectLoadedSink);

    static PostLoadSink postLoadSink;
    GetEventDispatcherList()->loadGameEventDispatcher.AddEventSink(&postLoadSink);

		inited = true;
		Log::info("RunCommandSink Initialized");
	}
#endif

	if (argc >= 1)
	{
		GFxValue commandVal = argv[0];
		if (commandVal.type == 4) { // Command
			const char* command = commandVal.data.string;
			//Log::info(command); // TEMP
			if (strcmp(command, "PopulateDialogueList") == 0)
			{
				numTopics = (argc - 2) / 3;
				desiredTopicIndex = -1;
				dialogueMenu = movie;
				std::vector<std::string> lines;
				for (int j = 1; j < argc - 1; j = j + 3)
				{
					GFxValue dialogueLine = argv[j];
					const char* dialogueLineStr = dialogueLine.data.string;
					lines.push_back(std::string(dialogueLineStr));
				}

				DialogueList dialogueList;
				dialogueList.lines = lines;
				SpeechRecognitionClient::getInstance()->StartDialogue(dialogueList);
			}
			else if (strcmp(command, "UpdatePlayerInfo") == 0)
			{
				FavoritesMenuManager::getInstance()->RefreshFavorites();
			}
		}
	}
}

// VR Only
static void __cdecl Hook_PostLoad() {
	FavoritesMenuManager::getInstance()->RefreshFavorites();
	Log::info("Favorites Menu Voice-Equip Initialized");
	//InitDebugEventSink();
}

static uintptr_t loopEnter = 0x0;
static uintptr_t loopCallTarget = 0x0;
static uintptr_t invokeTarget = 0x0;
static uintptr_t invokeReturn = 0x0;
static uintptr_t loadEventEnter = 0x0;
static uintptr_t loadEventTarget = 0x0;

static uintptr_t LOOP_ENTER_ADDR[3];
static uintptr_t LOOP_TARGET_ADDR[3];
static uintptr_t LOAD_EVENT_ENTER_ADDR[3];

uintptr_t getCallTarget(uintptr_t callInstructionAddr) {
	// x64 "call" instruction: E8 <32-bit target offset>
	// Note that the offset can be positive or negative.
	int32_t *pInvokeTargetOffset = (int32_t *)(callInstructionAddr + 1);

	// <call target address> = <call instruction beginning address> + <call instruction's size (5 bytes)> + <32-bit target offset>
	return callInstructionAddr + 5 + *pInvokeTargetOffset;
}

void Hooks_Inject(void)
{
	// "CurrentTime" GFxMovie.SetVariable (rax+80)
	LOOP_ENTER_ADDR[VR] = 0x8AC25C; // 0x8AA36C 0x00007FF7321CA36C SKSE UIManager process hook:  0x00F17200 + 0xAD8

	// "CurrentTime" GFxMovie.SetVariable Target (rax+80)
	LOOP_TARGET_ADDR[VR] = 0xF85C50; // SkyrimVR 0xF82710 0x00007FF7328A2710 SKSE UIManager process hook:  0x00F1C650

	// "Finished loading game" print statement, initialize player orientation?
	LOAD_EVENT_ENTER_ADDR[VR] = 0x5852A4;

	RelocAddr<uintptr_t> kSkyrimBaseAddr(0);
	uintptr_t kHook_Invoke_Enter = InvokeFunction.GetUIntPtr() + 0xEE;
	uintptr_t kHook_Invoke_Target = getCallTarget(kHook_Invoke_Enter);
	uintptr_t kHook_Invoke_Return = kHook_Invoke_Enter + 0x14;

	invokeTarget = kHook_Invoke_Target;
	invokeReturn = kHook_Invoke_Return;

	Log::address("Base Address: ", kSkyrimBaseAddr);
	Log::address("Invoke Enter: +", kHook_Invoke_Enter - kSkyrimBaseAddr);
	Log::address("Invoke Target: +", kHook_Invoke_Target - kSkyrimBaseAddr);

	/***
	Post Load HOOK - VR Only
	**/
	if (g_SkyrimType == VR) {
		RelocAddr<uintptr_t> kHook_LoadEvent_Enter(LOAD_EVENT_ENTER_ADDR[g_SkyrimType]);
		uintptr_t kHook_LoadEvent_Target = getCallTarget(kHook_LoadEvent_Enter);

		loadEventEnter = kHook_LoadEvent_Enter;
		loadEventTarget = kHook_LoadEvent_Target;

		Log::address("LoadEvent Enter: +", kHook_LoadEvent_Enter - kSkyrimBaseAddr);
		Log::address("LoadEvent Target: +", kHook_LoadEvent_Target - kSkyrimBaseAddr);

		struct Hook_LoadEvent_Code : Xbyak::CodeGenerator {
			Hook_LoadEvent_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				// Invoke original virtual method
				mov(rax, (uintptr_t)loadEventTarget);
				call(rax);

				// Call our method
				sub(rsp, 0x30);
				mov(rax, (uintptr_t)Hook_PostLoad);
				call(rax);
				add(rsp, 0x30);

				// Return
				mov(rax, loadEventEnter + 0x5);
				jmp(rax);
			}
		};
		void * codeBuf = g_localTrampoline.StartAlloc();
		Hook_LoadEvent_Code loadEventCode(codeBuf);
		g_localTrampoline.EndAlloc(loadEventCode.getCurr());
		g_branchTrampoline.Write5Branch(kHook_LoadEvent_Enter, uintptr_t(loadEventCode.getCode()));
	}

	/***
	Loop HOOK - VR Only
	**/
	if (g_SkyrimType == VR) {
		RelocAddr<uintptr_t> kHook_Loop_Enter(LOOP_ENTER_ADDR[g_SkyrimType]);
		RelocAddr<uintptr_t> kHook_Loop_Call_Target(LOOP_TARGET_ADDR[g_SkyrimType]);

		loopCallTarget = kHook_Loop_Call_Target;
		loopEnter = kHook_Loop_Enter;

		Log::address("Loop Enter: +", kHook_Loop_Enter - kSkyrimBaseAddr);
		Log::address("Loop Target: +", kHook_Loop_Call_Target - kSkyrimBaseAddr);

		struct Hook_Loop_Code : Xbyak::CodeGenerator {
			Hook_Loop_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				// Invoke original virtual method
				mov(rax, loopCallTarget);
				call(rax);

				// Call our method
				sub(rsp, 0x30);
				mov(rax, (uintptr_t)Hook_Loop);
				call(rax);
				add(rsp, 0x30);

				// Return
				mov(rax, loopEnter + 0x6); // set to 0x5 when branching for SKSE UIManager
				jmp(rax);
			}
		};
		void * codeBuf = g_localTrampoline.StartAlloc();
		Hook_Loop_Code loopCode(codeBuf);
		g_localTrampoline.EndAlloc(loopCode.getCurr());
		//g_branchTrampoline.Write6Branch(kHook_Loop_Enter, uintptr_t(loopCode.getCode()));
		g_branchTrampoline.Write5Branch(kHook_Loop_Enter, uintptr_t(loopCode.getCode()));
	}

	/***
	Invoke "call" HOOK
	**/
	{
		struct Hook_Entry_Code : Xbyak::CodeGenerator {
			Hook_Entry_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				push(rcx);
				push(rdx);
				push(r8);
				push(r9);
				sub(rsp, 0x30);
				mov(rax, (uintptr_t)Hook_Invoke);
				call(rax);
				add(rsp, 0x30);
				pop(r9);
				pop(r8);
				pop(rdx);
				pop(rcx);

				mov(rax, invokeTarget);
				call(rax);

				mov(rbx, ptr[rsp + 0x50]);
				mov(rsi, ptr[rsp + 0x60]);
				add(rsp, 0x40);
				pop(rdi);

				mov(rax, invokeReturn);
				jmp(rax);
			}
		};

		void * codeBuf = g_localTrampoline.StartAlloc();
		Hook_Entry_Code entryCode(codeBuf);
		g_localTrampoline.EndAlloc(entryCode.getCurr());
		g_branchTrampoline.Write5Branch(kHook_Invoke_Enter, uintptr_t(entryCode.getCode()));
	}
}
