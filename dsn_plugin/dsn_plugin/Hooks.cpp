
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
typedef UInt32 getDefaultCompiler(void* unk01, char* compilerName, UInt32 unk03);
typedef void executeCommand(UInt32* unk01, void* parser, char* command);

static void __cdecl Hook_Loop()
{
	if (dialogueMenu != NULL)
	{
		// Menu exiting, avoid NPE
		if (dialogueMenu->GetPause() == 0)
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
			}
			else {
				FavoritesMenuManager::getInstance()->ClearFavorites();
				Log::info("Favorites Menu Voice-Equip Disabled");
			}
		}
		return kEvent_Continue;
	}
};


class PostLoadSink : public BSTEventSink<void> {
public:
	EventResult ReceiveEvent(void* evn, EventDispatcher<void>* dispatcher) override {
		FavoritesMenuManager::getInstance()->RefreshFavorites();
		Log::info("Favorites Menu Voice-Equip Updated");
		return kEvent_Continue;
	}
};

static void __cdecl Hook_Invoke(GFxMovieView* movie, char * gfxMethod, GFxValue* argv, UInt32 argc)
{
#ifndef IS_VR
	static bool inited = false;
	if (!inited && g_SkyrimType == SE) {
		// Currently in the source code directory is the latest version of SKSE64 instead of SKSEVR,
		// so we can call GetSingleton() directly instead of use a RelocAddr.
		static RunCommandSink runCommandSink;
		static ObjectLoadedSink objectLoadedSink;
		static PostLoadSink postLoadSink;

		InputEventDispatcher::GetSingleton()->AddEventSink(&runCommandSink);
		GetEventDispatcherList()->objectLoadedDispatcher.AddEventSink(&objectLoadedSink);
		GetEventDispatcherList()->unk6E0.AddEventSink(&postLoadSink);

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

static void __cdecl Hook_PostLoad() {
	if (g_SkyrimType == VR) {
		FavoritesMenuManager::getInstance()->RefreshFavorites();
	}
}

static uintptr_t loopEnter = 0x0;
static uintptr_t loopCallTarget = 0x0;
static uintptr_t invokeTarget = 0x0;
static uintptr_t invokeReturn = 0x0;
static uintptr_t loadEventEnter = 0x0;
static uintptr_t loadEventTarget = 0x0;

static uintptr_t LOOP_ENTER_ADDR[2];
static uintptr_t LOOP_TARGET_ADDR[2];
static uintptr_t LOAD_EVENT_ENTER_ADDR[2];

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
