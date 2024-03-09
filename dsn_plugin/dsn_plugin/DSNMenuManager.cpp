#include "DSNMenuManager.h"
#include "SkyrimType.h"
#include "common/IPrefix.h"
#include "skse64_common/Relocation.h"
#include "Log.h"

/*
NOTE: Expose private fields on MenuManager and tHashSet to avoid requiring hooks into updated code
*/
IMenu * DSNMenuManager::GetOrCreateMenu(const char *menuName) {
	MenuManager *menuManager = MenuManager::GetSingleton();

	if (!menuManager)
		return NULL;

	MenuManager::MenuTable *t = &menuManager->menuTable;

	for(int i = 0; i< t->m_size;i++){
		MenuManager::MenuTable::_Entry * entry = &t->m_entries[i];
		if (entry != t->m_eolPtr && entry->next != NULL) {
			MenuTableItem *item = &entry->item;

			if ((uintptr_t)item > 1 && item->name) {
				BSFixedString *name = &item->name;
				if ((uintptr_t)name > 1 && name->data && std::strncmp(name->data, menuName, 10) == 0) {
					if (!item->menuInstance && item->menuConstructor) {
						item->menuInstance = ((IMenu*(*)())item->menuConstructor)();
					}

					return item->menuInstance;
				}
			}
		}
	}

	return NULL;
}

