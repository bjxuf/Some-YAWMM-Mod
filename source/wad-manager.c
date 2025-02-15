#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <malloc.h>
#include <ctype.h>
#include <wiilight.h>
#include <wiidrc/wiidrc.h>

#include "sys.h"
#include "gui.h"
#include "menu.h"
#include "restart.h"
#include "sys.h"
#include "video.h"
#include "wpad.h"
#include "fat.h"
#include "nand.h"
#include "globals.h"
#include "iospatch.h"

// Globals
CONFIG gConfig;

// Prototypes
extern u32 WaitButtons (void);
void CheckPassword (void);
void SetDefaultConfig (void);
int ReadConfigFile (char *configFilePath);
int GetIntParam (char *inputStr);
int GetStartupPath (char *startupPath, char *inputStr);
int GetStringParam (char *outParam, char *inputStr, int maxChars);

// Default password Up-Down-Left-Right-Up-Down
//#define PASSWORD "UDLRUD"
void CheckPassword (void)
{
	char curPassword [11]; // Max 10 characters password, NULL terminated
	int count = 0;

	if (strlen (gConfig.password) == 0)
		return;

	// Ask user for a password. Press "B" to restart Wii
	printf("[+] [Escribe clave para continuar]:\n\n");

	printf(">>  Presiona A para continuar.\n");
	printf(">>  Presiona B para reiniciar Wii.\n");

	/* Wait for user answer */
	for (;;) 
	{
		u32 buttons = WaitButtons();

		if (buttons & WPAD_BUTTON_A)
		{
			// A button, validate the pw
			curPassword [count] = 0;
			//if (strcmp (curPassword, PASSWORD) == 0)
			if (strcmp (curPassword, gConfig.password) == 0)
			{
				printf(">>  Clave aceptada...\n");
				break;
			}
			else
			{
				printf ("\n");
				printf(">>  Clave incorrecta. Intenta otra vez...\n");
				printf("[+] [Escribe clave para continuar]:\n\n");
				printf(">>  Presiona A para continuar.\n");
				printf(">>  Presiona B para reiniciar Wii.\n");
				count = 0;
			}
		}
		else if (buttons & WPAD_BUTTON_B)
			// B button, restart
			Restart();
		else
		{
			if (count < 10)
			{
				// Other buttons, build the password
				if (buttons & WPAD_BUTTON_LEFT)
				{
					curPassword [count++] = 'L';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_RIGHT)
				{
					curPassword [count++] = 'R';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_UP)
				{
					curPassword [count++] = 'U';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_DOWN)
				{
					curPassword [count++] = 'D';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_1)
				{
					curPassword [count++] = '1';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_2)
				{
					curPassword [count++] = '2';
					printf ("*");
				}
			}
		}
	}
}

void Disclaimer(void)
{
	/* Print disclaimer */
	printf("[+] [AVISO LEGAL]:\n\n");

	printf("    ESTA APLICACION VIENE SIN NINGUNA GARANTIA,\n");
	printf("    NI EXPRESA NI IMPLICITA.\n");
	printf("    NO ASUMO NINGUNA RESPONSIBILIDAD POR CUALQUIER PERJUICIO CAUSADO\n");
	printf("    EN TU CONSOLA WII AL USAR INADECUADAMENTE ESTE SOFTWARE.\n\n");

	printf(">>  Si aceptas, presiona A para continuar.\n");
	printf(">>  De lo contrario, presiona B para reiniciar Wii.\n");

	/* Wait for user answer */
	for (;;) {
		//u32 buttons = Wpad_WaitButtons();
		u32 buttons = WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			Restart();
	}
}

int main(int argc, char **argv)
{
	ES_GetBoot2Version(&boot2version);
	if(!AHBPROT_DISABLED)
	{
		if(boot2version < 5) 
		{
			if(!loadIOS(202)) if(!loadIOS(222)) if(!loadIOS(223)) if(!loadIOS(224)) if(!loadIOS(249)) loadIOS(36);
		}else{
			if(!loadIOS(249)) loadIOS(36);
		}
	}
	/* Initialize subsystems */
	Sys_Init();

	/* Set video mode */
	Video_SetMode();

	/* Initialize console */
	Gui_InitConsole();

	/* Draw background */
	Gui_DrawBackground();

	/* Initialize Wiimote and GC Controller */
	Wpad_Init();
	PAD_Init ();
	WiiDRC_Init();
	WIILIGHT_Init();

	/* Print disclaimer */
	//Disclaimer();
	
	// Set the defaults
	SetDefaultConfig ();

	// Read the config file
	ReadConfigFile (WM_CONFIG_FILE_PATH);

	// Check password
	CheckPassword ();

	/* Menu loop */
	Menu_Loop();

	/* Restart Wii */
	Restart_Wait();

	return 0;
}


int ReadConfigFile (char *configFilePath)
{
    int retval = 0;
	FILE *fptr;
	char *tmpStr = malloc (MAX_FILE_PATH_LEN);
	char tmpOutStr [40], path[128];
	int i;

	if (tmpStr == NULL)
		return (-1);

	fatDevice *fdev = &fdevList[0];
	int ret = Fat_Mount(fdev);
	snprintf(path, sizeof(path), "%s%s", fdev->mount, configFilePath);
	
	if (ret < 0) 
	{
		fdev = &fdevList[2];
		ret = Fat_Mount(fdev);
		snprintf(path, sizeof(path), "%s%s", fdev->mount, configFilePath);
		snprintf(path, sizeof(path), "%s%s", fdev->mount, configFilePath);
	}
	
	if (ret < 0) 
	{
		printf(" ERROR! (ret = %d)\n", ret);
		// goto err;
		retval = -1;
	}
	else
	{
		// Read the file
		fptr = fopen (path, "rb");
		if (fptr != NULL)
		{	
			// Read the options
			char done = 0;

			while (!done)
			{
				if (fgets (tmpStr, MAX_FILE_PATH_LEN, fptr) == NULL)
					done = 1;
				else if (isalpha(tmpStr[0]))
				{
					// Get the password
					if (strncmp (tmpStr, "Password", 8) == 0)
					{
						// Get password
						// GetPassword (gConfig.password, tmpStr);
						GetStringParam (gConfig.password, tmpStr, MAX_PASSWORD_LENGTH);
						
						// If password is too long, ignore it
						if (strlen (gConfig.password) > 10)
						{
							gConfig.password [0] = 0;
							printf ("Claves con mas de 10 caracteres seran ignoradas. Presiona un boton...\n");
							WaitButtons ();
						}
					}
				
					// Get startup path
					else if (strncmp (tmpStr, "StartupPath", 11) == 0)
					{
						// Get startup Path
						GetStartupPath (gConfig.startupPath, tmpStr);
					}
					
					// cIOS
					else if (strncmp (tmpStr, "cIOSVersion", 11) == 0)
					{
						// Get cIOSVersion
						gConfig.cIOSVersion = (u8)GetIntParam (tmpStr);
					}
					
					// FatDevice
					else if (strncmp (tmpStr, "FatDevice", 9) == 0)
					{
						// Get fatDevice
						GetStringParam (tmpOutStr, tmpStr, MAX_FAT_DEVICE_LENGTH);
						for (i = 0; i < 5; i++)
						{
							if (strncmp (fdevList[i].mount, tmpOutStr, 4) == 0)
							{
								gConfig.fatDeviceIndex = i;
							}
						}
					}
					
					// NandDevice
					else if (strncmp (tmpStr, "NANDDevice", 10) == 0)
					{
						// Get fatDevice
						GetStringParam (tmpOutStr, tmpStr, MAX_NAND_DEVICE_LENGTH);
						for (i = 0; i < 3; i++)
						{
							if (strncmp (ndevList[i].name, tmpOutStr, 2) == 0)
							{
								gConfig.nandDeviceIndex = i;
							}
						}
					}
				}
			} // EndWhile
			
			// Close the config file
			fclose (fptr);
		}
		else
		{
			// If the wm_config.txt file is not found, just take the default config params
			//printf ("Config file is not found\n");  // This is for testing only
			//WaitButtons();
		}
		Fat_Unmount(fdev);
	}

	// Free memory
	free (tmpStr);

	return (retval);
} // ReadConfig


void SetDefaultConfig (void)
{
	// Default password is NULL or no password
	gConfig.password [0] = 0;
	
	// Default startup folder
	strcpy (gConfig.startupPath, WAD_ROOT_DIRECTORY);
	
	gConfig.cIOSVersion = CIOS_VERSION_INVALID;            // Means that user has to select later
	gConfig.fatDeviceIndex = FAT_DEVICE_INDEX_INVALID;     // Means that user has to select
	gConfig.nandDeviceIndex = NAND_DEVICE_INDEX_INVALID;   // Means that user has to select

} // SetDefaultConfig


int GetStartupPath (char *startupPath, char *inputStr)
{
	int i = 0;
	int len = strlen (inputStr);
	
	// Find the "="
	while ((inputStr [i] != '=') && (i < len))
	{
		i++;
	}
	i++;

	// Get to the "/"
	while ((inputStr [i] != '/') && (i < len))
	{
		i++;
	}

	// Get the startup Path
	int count = 0;
	while (isascii(inputStr [i]) && (i < len) && (inputStr [i] != '\n') && 
	         (inputStr [i] != '\r') && (inputStr [i] != ' '))
	{
		startupPath [count++] = inputStr [i++];
	}
	startupPath [count] = 0; // NULL terminate

	return (0);
} // GetStartupPath

int GetIntParam (char *inputStr)
{
	int retval = 0;
	int i = 0;
	int len = strlen (inputStr);
	char outParam [40];
	
	// Find the "="
	while ((inputStr [i] != '=') && (i < len))
	{
		i++;
	}
	i++;
	
	// Get to the first alpha numeric character
	while ((isdigit(inputStr [i]) == 0) && (i < len))
	{
		i++;
	}

	// Get the string param
	int outCount = 0;
	while ((isdigit(inputStr [i])) && (i < len) && (outCount < 40))
	{
		outParam [outCount++] = inputStr [i++];
	}
	outParam [outCount] = 0; // NULL terminate
	retval = atoi (outParam);
	
	return (retval);
} // GetIntParam


int GetStringParam (char *outParam, char *inputStr, int maxChars)
{
	int i = 0;
	int len = strlen (inputStr);
	
	// Find the "="
	while ((inputStr [i] != '=') && (i < len))
	{
		i++;
	}
	i++;
	
	// Get to the first alpha character
	while ((isalpha(inputStr [i]) == 0) && (i < len))
	{
		i++;
	}
	
	// Get the string param
	int outCount = 0;
	while ((isalnum(inputStr [i])) && (i < len) && (outCount < maxChars))
	{
		outParam [outCount++] = inputStr [i++];
	}
	outParam [outCount] = 0; // NULL terminate
	
	return (0);
} // GetStringParam
