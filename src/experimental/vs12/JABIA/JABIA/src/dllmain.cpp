#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <detours.h>

#include "resource.h"
#include "character.h"

#pragma comment(lib, "detours.lib")

#define FULL
//#define DEMO

//#define SHOW_INVENTORY_FUNC 0x00D39B70  
#ifdef DEMO
#define CHARACTER_CONST_OFFSET 0x112450
#define CHARACTER_CONST_RETN_OFFSET 0x210
static ProcessName = "GameDemo.exe";
#else
#define CHARACTER_CONST_OFFSET 0x132880
#define CHARACTER_CONST_RETN_OFFSET 0x2D8
static char ProcessName[] = "GameJABiA.exe";
#endif

/*
address of CHARACTER_CONST_OFFSET is at the start of this sequence
51 53 8B 5C 24 10 57 B8 02 00 00 00 8D 7E 18 C7 46 04 72 61 68 63 66 89 46 08
*/
typedef int (_stdcall *ShowInventoryPtr)(void * unknown1, void * character_ptr, void * unknown2, int unknown3);
typedef void * (_stdcall *CharacterConstRetrunPtr)();

BOOL CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD WINAPI MyThread(LPVOID);
void* myCharacterConstRetrun();
void recordCharacters(void* instance);
void dump_current_character(HWND hwnd, uint32_t ptr);
void fillDialog(HWND hwnd, uint32_t ptr);
void setCharacter(HWND hwnd, uint32_t ptr);

CharacterConstRetrunPtr ParseCharacter;

HMODULE game_handle; // address of GameDemo.exe
DWORD g_threadID;
HMODULE g_hModule;
//void __stdcall CallFunction(void);
HINSTANCE TheInstance = 0;


// character vector
std::vector<JABIA_Character *> jabia_characters;
int last_character_selected_index = 0;
int last_weaponslot_selected_index = 0;

INT APIENTRY DllMain(HMODULE hDLL, DWORD Reason, LPVOID Reserved)
{
    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hModule = hDLL;
		DisableThreadLibraryCalls(hDLL);
        CreateThread(NULL, NULL, &MyThread, NULL, NULL, &g_threadID);
    break;
    case DLL_THREAD_ATTACH:
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

DWORD WINAPI MyThread(LPVOID)
{
	char buf [100];
	// find base address of GameDemo.exe in memory
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, ProcessName, &game_handle);
	// find address of character constructor
	ParseCharacter = (CharacterConstRetrunPtr)((uint32_t)game_handle+CHARACTER_CONST_OFFSET);
	wsprintf (buf, "Address of CharacterConstructor 0x%x", ParseCharacter);
	OutputDebugString(buf);
	// find address of character constructor return
	ParseCharacter = (CharacterConstRetrunPtr)((uint32_t)game_handle+CHARACTER_CONST_OFFSET+CHARACTER_CONST_RETN_OFFSET);
	wsprintf (buf, "Address of retn in CharacterConstructor 0x%x", ParseCharacter);
	OutputDebugString(buf);

	// If jabia_characters is not empty, clear it. Every time the game loads a level, character pointers change.
	//TODO need to hook load level function and do this then too.
	jabia_characters.clear();

	DWORD oldProtection;
	// read + write
	VirtualProtect(ParseCharacter, 6, PAGE_EXECUTE_READWRITE, &oldProtection);
	DWORD JMPSize = ((DWORD)myCharacterConstRetrun - (DWORD)ParseCharacter - 5);
	BYTE Before_JMP[6]; // save retn here
	BYTE JMP[6] = {0xE9, 0x90, 0x90, 0x90, 0x90, 0x90}; // JMP NOP NOP ...
	memcpy((void *)Before_JMP, (void *)ParseCharacter, 6); // save retn
	memcpy(&JMP[1], &JMPSize, 4);
	wsprintf(buf, "JMP: %x%x%x%x%x", JMP[0], JMP[1], JMP[2], JMP[3], JMP[4], JMP[5]);
    OutputDebugString(buf);
	// overwrite retn with JMP
	memcpy((void *)ParseCharacter, (void *)JMP, 6);
	// restore protection
    VirtualProtect((LPVOID)ParseCharacter, 6, oldProtection, NULL);


    while(true)
    {
        if(GetAsyncKeyState(VK_F7) & 1)
        {

			    HWND hDialog = 0;
				
			    hDialog = CreateDialog (g_hModule,
                            MAKEINTRESOURCE (IDD_DIALOG1),
                            0,
                            DialogProc);

/*				
				uint32_t temp_addr;
				temp_addr = (uint32_t)game_handle;
				
				wsprintf (buf, "Address of GameDemo.exe 0x%x", temp_addr);
				OutputDebugString(buf);		
				wsprintf (buf, "Address of base ptr 0x%x", temp_addr + 0x003C64E8);
				OutputDebugString(buf);		
				temp_addr = *(uint32_t *)(temp_addr + 0x003C64E8)+ 0x6E0;
				wsprintf (buf, "base ptr 0x%x", temp_addr);
				OutputDebugString(buf);		
				temp_addr = *(uint32_t *)temp_addr + 0xD4;
				wsprintf (buf, "fist ptr 0x%x", temp_addr);
				OutputDebugString(buf);
				temp_addr = *(uint32_t *)temp_addr + 0x5C0;
				wsprintf (buf, "second ptr 0x%x", temp_addr);
				OutputDebugString(buf);
				temp_addr = *(uint32_t *)temp_addr + 0x34;
				wsprintf (buf, "third ptr 0x%x", temp_addr);
				OutputDebugString(buf);
				temp_addr = *(uint32_t *)temp_addr;
				wsprintf (buf, "final ptr 0x%x", temp_addr);
				OutputDebugString(buf);
*/
				fillDialog(hDialog, (uint32_t)jabia_characters.at(last_character_selected_index));
				
				if (!hDialog)
				{
					char buf [100];
					wsprintf (buf, "Error x%x", GetLastError ());
					MessageBox (0, buf, "CreateDialog", MB_ICONEXCLAMATION | MB_OK);
					return 1;
				 }
				
				MSG  msg;
				int status;
				while ((status = GetMessage (& msg, 0, 0, 0)) != 0)
				{
					if (status == -1)
						return -1;
					if (!IsDialogMessage (hDialog, & msg))
					{
						TranslateMessage ( & msg );
						DispatchMessage ( & msg );
					}
				}
        }
        else if(GetAsyncKeyState(VK_F8) &1) {
			OutputDebugString("Unloading DLL");
            break;
		}
    Sleep(100);
    }
	// restor retn hook
	// read + write
	VirtualProtect(ParseCharacter, 6, PAGE_EXECUTE_READWRITE, &oldProtection);
	memcpy((void *)ParseCharacter, (void *)Before_JMP, 6);
	// restore protection
    VirtualProtect((LPVOID)ParseCharacter, 6, oldProtection, NULL);

	FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL CALLBACK DialogProc (HWND hwnd, 
                          UINT message, 
                          WPARAM wParam, 
                          LPARAM lParam)
{
	HMENU hMenu;
	HWND comboControl1;
	HWND comboControl2;
	comboControl1=GetDlgItem(hwnd,IDC_COMBO1);	
	comboControl2=GetDlgItem(hwnd,IDC_COMBO2);	
	BOOL status = FALSE;
	uint32_t address = 0; // character address

    switch (message)
    {
		case WM_INITDIALOG:
			// add menu
			hMenu = LoadMenu(g_hModule, MAKEINTRESOURCE(IDR_MENU1));
			SetMenu(hwnd,hMenu);
			
			// add icon
			HICON hIcon;

			hIcon = (HICON)LoadImage(   g_hModule,
                           MAKEINTRESOURCE(IDI_ICON1),
                           IMAGE_ICON,
                           GetSystemMetrics(SM_CXSMICON),
                           GetSystemMetrics(SM_CYSMICON),
                           0);
			if(hIcon) {
				SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			}

			// add characters to drop down list
			for(unsigned long i = 0; i < jabia_characters.size(); i++) {							
				SendMessage(comboControl1,CB_ADDSTRING,0,reinterpret_cast<LPARAM>((LPCTSTR)jabia_characters.at(i)->merc_name));
				//wsprintf(buf, "In init, Character at 0x%X", jabia_characters.at(i));	
				//OutputDebugString(buf);
			}
				// select fist item in list
			SendMessage(comboControl1, CB_SETCURSEL, last_character_selected_index, 0);
			

			// add weapons slots to their combo box
			SendMessage(comboControl2,CB_ADDSTRING,0,reinterpret_cast<LPARAM>((LPCTSTR)"1"));
			SendMessage(comboControl2,CB_ADDSTRING,0,reinterpret_cast<LPARAM>((LPCTSTR)"2"));
			SendMessage(comboControl2,CB_ADDSTRING,0,reinterpret_cast<LPARAM>((LPCTSTR)"3"));

			// select fist item in list
			SendMessage(comboControl2, CB_SETCURSEL, last_weaponslot_selected_index, 0);

			// TODO dialog does not become active the first time it's created
			break;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
				case IDC_COMBO1:
					switch(HIWORD(wParam))
					{
						case CBN_CLOSEUP:
							// use combo box selected index to get a character out of the vector
							last_character_selected_index = SendMessage(comboControl1, CB_GETCURSEL, 0, 0);
							address = (uint32_t)jabia_characters.at(last_character_selected_index);
							fillDialog(hwnd, address);
							break;
					}
				case IDC_COMBO2:
					switch(HIWORD(wParam))
					{
						case CBN_CLOSEUP:
							// use combo box selected index to get weapon from inventory
							last_weaponslot_selected_index = SendMessage(comboControl2, CB_GETCURSEL, 0, 0);
							address = (uint32_t)jabia_characters.at(last_character_selected_index);
							fillDialog(hwnd, address);
							break;
					}
                case IDSET:
					address = (uint32_t)jabia_characters.at(last_character_selected_index);
					setCharacter(hwnd, address);
					break;
				case IDM_DUMP_CHARACTER:					
					address = (uint32_t)jabia_characters.at(last_character_selected_index);
					dump_current_character(hwnd, address);
					break;
                case IDCANCEL:
                    DestroyWindow(hwnd);
					PostQuitMessage(0);
					break;
            }
        break;
        default:
            return FALSE;
    }
    return FALSE;
}

void dump_current_character(HWND hwnd, uint32_t ptr) {
	char buf[100];
	JABIA_Character character;
	wsprintf(buf, "Character address 0x%X | ptr address 0x%x", &character, ptr);
	OutputDebugString(buf);
	memcpy(&character, (void *)ptr, sizeof(JABIA_Character));		
	OutputDebugString("Dump character");
	
	OPENFILENAME ofn;
    char szFileName[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn); // SEE NOTE BELOW
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "JABIA Character Dump (*.jcd)\0*.jcd\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "jcd";
	
    if(GetSaveFileName(&ofn))
    {
        // Do something usefull with the filename stored in szFileName 
		dump_character(&character, szFileName);
    } else {
		//TODO show error message
	}
}

void fillDialog(HWND hwnd, uint32_t ptr) {
	char buf[100];
	JABIA_Character character;
	memcpy(&character, (void *)ptr, sizeof(JABIA_Character));		
	
	// address of character
	_itoa_s(ptr, buf, 100, 16);
	SetDlgItemText(hwnd, IDC_ADDRESS, buf);	
	
	_itoa_s(character.level, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_LEV, buf);

	_itoa_s(character.experience, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_EX, buf);

	_itoa_s(character.training_points, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_TP, buf);
	
	_itoa_s(character.weapon_in_hand, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_WPN_EQ, buf);

	_itoa_s(character.weapon_in_hand_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_WPN_EQ_DUR, buf);

	_itoa_s(character.helmet_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_HELM_EQ, buf);

	_itoa_s(character.helmet_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_HELM_EQ_DUR, buf);

	_itoa_s(character.eyewear_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_EYE_EQ, buf);

	_itoa_s(character.eyewear_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_EYE_EQ_DUR, buf);

	_itoa_s(character.special_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SPC_EQ, buf);

	_itoa_s(character.special_equiped_charges, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SPC_EQ_LEFT, buf);

	_itoa_s(character.shirt_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SHRT_EQ, buf);

	_itoa_s(character.shirt_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SHRT_EQ_DUR, buf);

	_itoa_s(character.vest_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_VEST_EQ, buf);

	_itoa_s(character.vest_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_VEST_DUR, buf);

	_itoa_s(character.shoes_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SHOES_EQ, buf);

	_itoa_s(character.shoes_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_SHOES_DUR, buf);

	_itoa_s(character.pants_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_PANTS_EQ, buf);

	_itoa_s(character.pants_equiped_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_PANTS_DUR, buf);

	_itoa_s(character.ammo_equiped, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_AMMO_EQ, buf);

	_itoa_s(character.ammo_equiped_count, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_AMMO_EQ_CNT, buf);

	_itoa_s(character.ammo_equiped_count, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_AMMO_EQ_CNT, buf);

	_itoa_s(character.weapon_attachment_removable, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_WPN_MOD, buf);

	// health and stamina 	
	sprintf_s(buf, "%.1f", character.health, 4);
	//OutputDebugString(buf);
	SetDlgItemText(hwnd, IDC_HLTH, buf);

	sprintf_s(buf, "%.1f", character.stamina, 4);
	SetDlgItemText(hwnd, IDC_STAMINA, buf);

	// name
	memset(buf, 0x00, 16);
	memcpy(buf, character.merc_name, character.name_length);
	SetDlgItemText(hwnd, IDC_MERC_NAME, buf);

	_itoa_s(character.faction, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_MERC_FAC, buf);

	_itoa_s(character.medical_condition, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_MED_COND, buf);

	// inventory
	_itoa_s(character.weapons[last_weaponslot_selected_index].weapon, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_WPN_INV, buf);

	_itoa_s(character.weapons[last_weaponslot_selected_index].weapon_durability, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_WPN_INV_DUR, buf);

	_itoa_s(character.weapons[last_weaponslot_selected_index].ammo_count, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_AMMO_INV_CNT, buf);

	// attributes
	_itoa_s(character.agility, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_AG, buf);

	_itoa_s(character.dexterity, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_DEX, buf);

	_itoa_s(character.strength, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_STR, buf);

	_itoa_s(character.intelligence, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_INT, buf);

	_itoa_s(character.perception, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_PER, buf);

	// skills

	_itoa_s(character.medical, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_MED, buf);

	_itoa_s(character.explosives, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_EXPL, buf);

	_itoa_s(character.marksmanship, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_MARK, buf);

	_itoa_s(character.stealth, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_STEALTH, buf);

	_itoa_s(character.mechanical, buf, 100, 10);
	SetDlgItemText(hwnd, IDC_MECH, buf);
	
}

void setCharacter(HWND hwnd, uint32_t ptr) {
	char buf [100];
	JABIA_Character * character_ptr = (JABIA_Character *)ptr;
	uint32_t level = 0;
	uint32_t experience;
	uint32_t training_points;
	uint16_t weapon_in_hand;
	uint16_t weapon_in_hand_durability;
	uint16_t helmet_equiped;
	uint16_t helmet_equiped_durability;
	uint16_t eyewear_equiped; 
	uint16_t eyewear_equiped_durability;
	uint16_t special_equiped; 
	uint16_t special_equiped_charges;
	uint16_t shirt_equiped;
	uint16_t shirt_equiped_durability;
	uint16_t vest_equiped;
	uint16_t vest_equiped_durability;
	uint16_t shoes_equiped;
	uint16_t shoes_equiped_durability;
	uint16_t pants_equiped;
	uint16_t pants_equiped_durability;
	uint16_t ammo_equiped;
	uint16_t ammo_equiped_count;
	uint16_t weapon_attachment_removable;

	uint16_t weapon;
	uint16_t weapon_durability;
	uint16_t ammo_count;

	float health;
	float stamina;
	uint32_t faction;
	uint32_t medical_condition;

	// attributes
	uint32_t agility;
	uint32_t dexterity;
	uint32_t strength;
	uint32_t intelligence;
	uint32_t perception;

	// skills
	uint32_t medical;
	uint32_t explosives;
	uint32_t marksmanship;
	uint32_t stealth;
	uint32_t mechanical;

	GetDlgItemText(hwnd, IDC_LEV, buf, 100);
	level = atoi(buf);
	character_ptr->level = level;

	GetDlgItemText(hwnd, IDC_EX, buf, 100);
	experience = atoi(buf);
	character_ptr->experience = experience;

	GetDlgItemText(hwnd, IDC_TP, buf, 100);
	training_points = atoi(buf);
	character_ptr->training_points = training_points;

	GetDlgItemText(hwnd, IDC_WPN_EQ, buf, 100);
	weapon_in_hand = atoi(buf);
	character_ptr->weapon_in_hand = weapon_in_hand;

	character_ptr->weapon_in_hand_removable = 1;

	GetDlgItemText(hwnd, IDC_WPN_EQ_DUR, buf, 100);
	weapon_in_hand_durability = atoi(buf);
	character_ptr->weapon_in_hand_durability = weapon_in_hand_durability;

	GetDlgItemText(hwnd, IDC_HELM_EQ, buf, 100);
	helmet_equiped = atoi(buf);
	character_ptr->helmet_equiped = helmet_equiped;

	GetDlgItemText(hwnd, IDC_HELM_EQ_DUR, buf, 100);
	helmet_equiped_durability = atoi(buf);
	character_ptr->helmet_equiped_durability = helmet_equiped_durability;

	GetDlgItemText(hwnd, IDC_EYE_EQ, buf, 100);
	eyewear_equiped = atoi(buf);
	character_ptr->eyewear_equiped = eyewear_equiped;

	GetDlgItemText(hwnd, IDC_EYE_EQ_DUR, buf, 100);
	eyewear_equiped_durability = atoi(buf);
	character_ptr->eyewear_equiped_durability = eyewear_equiped_durability;

	GetDlgItemText(hwnd, IDC_SPC_EQ, buf, 100);
	special_equiped = atoi(buf);
	character_ptr->special_equiped = special_equiped;

	GetDlgItemText(hwnd, IDC_SPC_EQ_LEFT, buf, 100);
	special_equiped_charges = atoi(buf);
	character_ptr->special_equiped_charges = special_equiped_charges;

	GetDlgItemText(hwnd, IDC_SHRT_EQ, buf, 100);
	shirt_equiped = atoi(buf);
	character_ptr->shirt_equiped = shirt_equiped;

	GetDlgItemText(hwnd, IDC_SHRT_EQ_DUR, buf, 100);
	shirt_equiped_durability = atoi(buf);
	character_ptr->shirt_equiped_durability = shirt_equiped_durability;

	GetDlgItemText(hwnd, IDC_VEST_EQ, buf, 100);
	vest_equiped = atoi(buf);
	character_ptr->vest_equiped = vest_equiped;

	GetDlgItemText(hwnd, IDC_VEST_DUR, buf, 100);
	vest_equiped_durability = atoi(buf);
	character_ptr->vest_equiped_durability = vest_equiped_durability;

	GetDlgItemText(hwnd, IDC_SHOES_EQ, buf, 100);
	shoes_equiped = atoi(buf);
	character_ptr->shoes_equiped = shoes_equiped;

	GetDlgItemText(hwnd, IDC_SHOES_DUR, buf, 100);
	shoes_equiped_durability = atoi(buf);
	character_ptr->vest_equiped_durability = shoes_equiped_durability;

	GetDlgItemText(hwnd, IDC_PANTS_EQ, buf, 100);
	pants_equiped = atoi(buf);
	character_ptr->pants_equiped = pants_equiped;

	GetDlgItemText(hwnd, IDC_PANTS_DUR, buf, 100);
	pants_equiped_durability = atoi(buf);
	character_ptr->pants_equiped_durability = pants_equiped_durability;

	GetDlgItemText(hwnd, IDC_AMMO_EQ, buf, 100);
	ammo_equiped = atoi(buf);
	character_ptr->ammo_equiped = ammo_equiped;

	GetDlgItemText(hwnd, IDC_AMMO_EQ_CNT, buf, 100);
	ammo_equiped_count = atoi(buf);
	character_ptr->ammo_equiped_count = ammo_equiped_count;

	GetDlgItemText(hwnd, IDC_WPN_MOD, buf, 100);
	weapon_attachment_removable = atoi(buf);
	character_ptr->weapon_attachment_removable = weapon_attachment_removable;
	
	character_ptr->weapon_attachment_status = 1;

	// get name and it's length
	memset(buf, 0x0, JABIA_CHARACTER_MAX_NAME_LENGTH);
	GetDlgItemText(hwnd, IDC_MERC_NAME, buf, JABIA_CHARACTER_MAX_NAME_LENGTH);
	memcpy(character_ptr->merc_name, buf, JABIA_CHARACTER_MAX_NAME_LENGTH); // TODO read length of name from character struct
	character_ptr->name_length = (uint32_t)strlen(character_ptr->merc_name);

	// inventory
	GetDlgItemText(hwnd, IDC_WPN_INV, buf, 100);
	weapon = atoi(buf);
	character_ptr->weapons[last_weaponslot_selected_index].weapon = weapon;

	GetDlgItemText(hwnd, IDC_WPN_INV_DUR, buf, 100);
	weapon_durability = atoi(buf);
	character_ptr->weapons[last_weaponslot_selected_index].weapon_durability = weapon_durability;

	GetDlgItemText(hwnd, IDC_AMMO_INV_CNT, buf, 100);
	ammo_count = atoi(buf);
	character_ptr->weapons[last_weaponslot_selected_index].ammo_count = ammo_count;

	character_ptr->weapons[last_weaponslot_selected_index].removable = 1;

	// health and stamina
	GetDlgItemText(hwnd, IDC_HLTH, buf, 100);
	health = (float)atof(buf);
	character_ptr->health = health;

	GetDlgItemText(hwnd, IDC_STAMINA, buf, 100);
	stamina = (float)atof(buf);
	character_ptr->stamina = stamina;

	GetDlgItemText(hwnd, IDC_MERC_FAC, buf, 100);
	faction = atoi(buf);
	character_ptr->faction = faction;

	GetDlgItemText(hwnd, IDC_MED_COND, buf, 100);
	medical_condition = atoi(buf);
	character_ptr->medical_condition = medical_condition;

	// attributes
	GetDlgItemText(hwnd, IDC_AG, buf, 100);
	agility = atoi(buf);
	character_ptr->agility = agility;

	GetDlgItemText(hwnd, IDC_DEX, buf, 100);
	dexterity = atoi(buf);
	character_ptr->dexterity = dexterity;

	GetDlgItemText(hwnd, IDC_STR, buf, 100);
	strength = atoi(buf);
	character_ptr->strength = strength;

	GetDlgItemText(hwnd, IDC_INT, buf, 100);
	intelligence = atoi(buf);
	character_ptr->intelligence = intelligence;

	GetDlgItemText(hwnd, IDC_PER, buf, 100);
	perception = atoi(buf);
	character_ptr->perception = perception;

	// skills
	GetDlgItemText(hwnd, IDC_MED, buf, 100);
	medical = atoi(buf);
	character_ptr->medical = medical;

	GetDlgItemText(hwnd, IDC_EXPL, buf, 100);
	explosives = atoi(buf);
	character_ptr->explosives = explosives;

	GetDlgItemText(hwnd, IDC_MARK, buf, 100);
	marksmanship = atoi(buf);
	character_ptr->marksmanship = marksmanship;

	GetDlgItemText(hwnd, IDC_STEALTH, buf, 100);
	stealth = atoi(buf);
	character_ptr->stealth = stealth;

	GetDlgItemText(hwnd, IDC_MECH, buf, 100);
	mechanical = atoi(buf);
	character_ptr->mechanical = mechanical;
}

__declspec(naked) void* myCharacterConstRetrun(){	
	// Character constructor uses thiscall calling convention and as an optimization passes something in EDX. I don't hook the call to the constructor.
	// Instead, I hook the return of the character constructor. The "this" pointer will be in EAX.

#ifdef DEMO
	__asm{
		push eax;
		call recordCharacters;
		pop eax;
		retn 0x10;
	}
#else if FULL
	__asm{
		push eax;
		call recordCharacters;
		pop eax;
		retn 0x1C;
	}
#endif

}


void __cdecl recordCharacters(void* instance){
	char buf [100];
	JABIA_Character * character_ptr;
	OutputDebugString("Parsing character!");
#ifdef DEMO
	__asm{
		mov eax, [esp+4];
		mov character_ptr, eax;
	}
#else 
	__asm{
		mov eax, [esp+0x148];
		mov character_ptr, eax;
	}
#endif
	wsprintf(buf, "Character at 0x%X", character_ptr);
	OutputDebugString(buf);
	jabia_characters.push_back(character_ptr);
	wsprintf(buf, "Size %i", jabia_characters.size());
	OutputDebugString(buf);
}

/*
void BeginRedirect(LPVOID newFunction, uint32_t pOrigMBAddress)
{
    char debugBuffer[128];
    OutputDebugString("Redirecting");
    BYTE tempJMP[SIZE] = {0xE9, 0x90, 0x90, 0x90, 0x90, 0xC3};
    memcpy(JMP, tempJMP, SIZE);
    DWORD JMPSize = ((DWORD)newFunction - (DWORD)pOrigMBAddress - 5);
    VirtualProtect((LPVOID)pOrigMBAddress, SIZE, 
                    PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void *)oldBytes, (void *)pOrigMBAddress, SIZE);

    sprintf(debugBuffer, "Old bytes: %x%x%x%x%x", oldBytes[0], oldBytes[1], oldBytes[2], oldBytes[3], oldBytes[4], oldBytes[5]);
    OutputDebugString(debugBuffer);

    memcpy(&JMP[1], &JMPSize, 4);

    sprintf(debugBuffer, "JMP: %x%x%x%x%x", JMP[0], JMP[1], JMP[2], JMP[3], JMP[4], JMP[5]);
    OutputDebugString(debugBuffer);

    memcpy((void *)pOrigMBAddress, (void *)JMP, SIZE);
    VirtualProtect((LPVOID)pOrigMBAddress, SIZE, oldProtect, NULL);
}
*/