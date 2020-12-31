#include "UI.h"

#include "btcontent.h"
#include "peerlist.h"
#include "tracker.h"

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

UI UI_Instance;

//FIXME: Text on PSP is rendered 4px lower... wtf?
#ifdef PSP
#define Render(a,b,c,d,e) Render(a,b,(c)-4,d,e)

#include <psputility.h>
#include <pspnet_apctl.h>
#endif
//FIXME: Show that we are checking files, connecting, etc...

//FIXME: Too many files makes crash
// http://forums.ps2dev.org/viewtopic.php?t=7871&highlight=maximum+open+files
// btfiles.cpp - MAX_OPEN_FILES !!!!!!!! :D
enum UserEventCode
{
	UEC_Redraw
};

int UI_Thread_Function(void *unused)
{
	while (true)
	{
		UI_Instance.Poll();
		SDL_Delay(1000/60);
	}
}

SDL_Color COLOR_BLACK = { 0, 0, 0, 255 };

///Public
UI::UI()
{
	// Initialize SDL's video system and check for errors.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
		return;
	};
	
	atexit(SDL_Quit);
	
	mScreen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0);
	if (mScreen == NULL) {
		printf("Unable to set video mode: %s\n", SDL_GetError());
		return;
	};
	
#ifdef PSP
	SDL_JoystickOpen(0);
#endif
	
	if(TTF_Init()==-1) {
		printf("TTF_Init Failed: %s\n", TTF_GetError());
		return;
	}
	
	mThread = NULL;
	
	mFileOffset = 0;
	mFileNamesValid = 0;
		
	mXOffset = 0;
	
	mState = UIS_Invalid;
}

char* UI::Initialise()
{
	SDL_Surface* splash = IMG_Load("resources/splash.png");
	if (splash)
	{
		SDL_BlitSurface(splash, 0, mScreen, 0);
		SDL_UpdateRect(mScreen, 0, 0, 0, 0);
		SDL_Delay(4000);
		SDL_FreeSurface(splash);
	}
	
	// Load the Background
	mBackgroundImg = IMG_Load("resources/background.png");
	if (mBackgroundImg == NULL)
	{
		printf("Unable to load background.png!\n");
		return NULL;
	}
	//FIXME: Make into Hardware surface on PSP for spppppeeeeeeeeeeeeeddddddd
	
	//Load bold font
	mBigBoldFont = TTF_OpenFont("resources/bold.ttf", 16);
	mTopText = CachedStringSurface(mBigBoldFont);
	mTopText.SetText("Loading...", COLOR_BLACK);
	
	///Get something onto the screen ASAP
	SDL_BlitSurface(mBackgroundImg, 0, mScreen, 0);
	mTopText.Render(mScreen, 0, 0, 480, TA_TopCenter);
	SDL_UpdateRect(mScreen, 0, 0, 0, 0);

	//Load standard font
	mStandardFont = TTF_OpenFont("resources/plain.ttf", 14);
	
	mPeersText = CachedStringSurface(mStandardFont);
	mTotalsText = CachedStringSurface(mStandardFont);
	mSpeedText = CachedStringSurface(mStandardFont);
	mPercentText = CachedStringSurface(mStandardFont);

	mSelector = IMG_Load("resources/selector.png");
	
	for (int a = 0; a < MAX_FILES_SHOWN; a++)
	{
		mFileNames[a] = CachedStringSurface(mStandardFont);
		mFilePercent[a] = CachedStringSurface(mStandardFont);
	}
	
	//Ask what torrent to download (and get that detail back to the caller....
	char* res = GetTorrentFromUser();
	
	
	SwitchState(UIS_ConnectDialog);
	
	//Run untill we are connected
	while (mState != UIS_Viewing)
	{
		UI_Instance.Poll();
		SDL_Delay(1000/60);
	}

	//Boot the UI thread!
	mThread = SDL_CreateThread(UI_Thread_Function, NULL);
	
	return res;
}

void UI::QueueRedraw()
{
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = UEC_Redraw;
	SDL_PushEvent(&event);
}

void UI::Poll()
{
	bool needRedraw = false;
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		switch(mState)
		{
			case UIS_Viewing:
				needRedraw |= HandleInput_Viewing(event);
				break;
			case UIS_ConnectDialog:
				needRedraw |= HandleInput_ConnectDialog(event);
				break;
				
			case UIS_Invalid:
			case UIS_Connecting:
				//Do Nothing
				break;
		}
		
		//Default handlers
		if (event.type == SDL_USEREVENT)
		{
			switch (event.user.code)
			{
				case UEC_Redraw:
					needRedraw = true;
					break;
			}
		}
		else if (event.type == SDL_QUIT)
		{
			exit(0);
		}
	}
	
	if (mState == UIS_Connecting)
		needRedraw |= Poll_Connecting();
	
	mXOffset += mDXOffset;
	if (mXOffset > 0)
		mXOffset = 0;
	
	if (needRedraw || mDXOffset != 0)
	{
		if (mState == UIS_Viewing)
		{
			UpdateFilenames();
			UpdateBottomBar();
		}
		
		FullRedraw();
	}
}


///Private
void UI::SwitchState(const UIState &newState)
{
	//Always reset these
	mXOffset = 0;
	mDXOffset = 0;
	mFileNamesValid = 0;
	
	UIState oldState = mState;
	mState = newState;

	if (newState == UIS_ConnectDialog)
	{
		mTopText.SetText("Choose a Network Connection", COLOR_BLACK);
		//Load the network connections list
		mSelectorIndex = 0;
#ifdef PSP
		
		//Load the list of connections
		char name[100];
		for (int iNetIndex = 1; iNetIndex < 100;iNetIndex++)
		{
			if (sceUtilityCheckNetParam(iNetIndex) != 0)
				continue;  // this one is no good
			sceUtilityGetNetParam(iNetIndex, 0, (netData*)name);
			printf("%i %s\n", iNetIndex, name);
			
			mFileNames[mFileNamesValid].SetText(name, COLOR_BLACK);
			mFilePercent[mFileNamesValid].SetText("", COLOR_BLACK);
			mFileNamesValid++;
		}
#else
		//PC gets a fake list.
		mFileNames[0].SetText("Fake Connection 1", COLOR_BLACK);
		mFilePercent[0].SetText("", COLOR_BLACK);
		
		mFileNames[1].SetText("Fake Connection 2", COLOR_BLACK);
		mFilePercent[1].SetText("", COLOR_BLACK);

		mFileNamesValid = 2;
#endif
		QueueRedraw();
	}
	else if (newState == UIS_Connecting)
	{
		mFileNamesValid = 0;
#ifdef PSP
		mTopText.SetText("Connecting...", COLOR_BLACK);
		
		//Pull out the connection index based on mSelectorIndex
		int iNetIndex = 1;
		char name[100];
		for (; iNetIndex < 100;iNetIndex++)
		{
			if (sceUtilityCheckNetParam(iNetIndex) != 0)
				continue;  // this one is no good
			
			if (mSelectorIndex == 0) //We've found the chosen one
			{
				sceUtilityGetNetParam(iNetIndex, 0, (netData*)name);
				break;
			}
			mSelectorIndex--;
		}
		
		char tmp[200];
		sprintf(tmp, "Connecting to %s", name);
		mFileNames[0].SetText(tmp, COLOR_BLACK);
		mFileNamesValid = 1;
		
		//Boot the connection!
		int err;
		err = sceNetApctlConnect(iNetIndex);
		if (err != 0)
		{
			printf("sceNetApctlConnect returns %08X\n", err);
			exit(0);
		}
		mSelectorIndex = 0;
		QueueRedraw();
#else
		SwitchState(UIS_Viewing);
#endif
	}
	else if (newState == UIS_Viewing)
	{
		//Do Nothing...
	}
	
}

bool UI::HandleInput_Viewing(const SDL_Event &event)
{
	switch(event.type){
#ifndef PSP
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
				case SDLK_UP:
					if (mFileOffset > 0)
					{
						mFileOffset--;
						return true;
					}
					break;
							
				case SDLK_DOWN:
					if (mFileOffset < (int)(BTCONTENT.GetNFiles() - MAX_FILES_SHOWN))
					{
						mFileOffset++;
						return true;
					}
					break;
				case SDLK_RIGHT:
					mDXOffset = -4;
					break;
				case SDLK_LEFT:
					mDXOffset = 4;
					break;
				default:
					break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.sym)
			{
				case SDLK_RIGHT:
					mDXOffset = 0;
					break;
				case SDLK_LEFT:
					mDXOffset = 0;
					break;
				default:
					break;
			}
			break;
#else
		case SDL_JOYBUTTONDOWN:
			switch(event.jbutton.button)
			{
				case 8: //Up
					if (mFileOffset > 0)
					{
						mFileOffset--;
						return true;
					}
					break;
							
				case 6: //Down
					if (mFileOffset < (int)(BTCONTENT.GetNFiles() - MAX_FILES_SHOWN))
					{
						mFileOffset++;
						return true;
					}
					break;
				case 9: //Right
					mDXOffset = -4;
					break;
				case 7: //Left
					mDXOffset = 4;
					break;
				default:
					break;
			}
			break;
		case SDL_JOYBUTTONUP:
			switch(event.jbutton.button)
			{
				case 9: //Right
					mDXOffset = 0;
					break;
				case 7: //Left
					mDXOffset = 0;
					break;
				default:
					break;
			}
#endif
	}
	return false;
}

bool UI::HandleInput_ConnectDialog(const SDL_Event &event)
{
	switch(event.type){
#ifndef PSP
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
				case SDLK_UP:
					if (mSelectorIndex > 0)
					{
						mSelectorIndex--;
						return true;
					}
					break;
				case SDLK_DOWN:
					if (mSelectorIndex <mFileNamesValid-1)
					{
						mSelectorIndex++;
						return true;
					}
				case SDLK_RETURN:
					//User has selected a connection, switch states
					SwitchState(UIS_Connecting);
					break;
				default:
					break;
			}
			break;
#else
		case SDL_JOYBUTTONDOWN:
			switch(event.jbutton.button)
			{
				case 8: //Up
					if (mSelectorIndex > 0)
					{
						mSelectorIndex--;
						return true;
					}
					break;
				case 6: //Down
					if (mSelectorIndex <mFileNamesValid-1)
					{
						mSelectorIndex++;
						return true;
					}
					break;
				case 2: //Cross
				case 1: //Circle
					SwitchState(UIS_Connecting);
					break;
			}
			break;
#endif
	}
	return false;
}

bool UI::Poll_Connecting()
{
#ifdef PSP
	//We use mSelectorIndex to track the state, yeah I'm a bad person
	
	int state;
	int err;
	err = sceNetApctlGetState(&state);
	
	if (err != 0)
	{
		printf("sceNetApctlGetState returns $%x\n", err);
		exit(0);
	}
	
	if (state == 4) //Connected
	{
		SwitchState(UIS_Viewing);
		return false;
	}
	else if (state != mSelectorIndex) //State change
	{
		if (state == 0)
		{
			//Connection failed, return to connect dialog
			SwitchState(UIS_ConnectDialog);
		}
		else
		{
			char tmp[100];
			sprintf(tmp, "Connection State %i", state);
			mFileNames[mFileNamesValid].SetText(tmp, COLOR_BLACK);
			mFilePercent[mFileNamesValid].SetText("", COLOR_BLACK);
			mFileNamesValid++;
			mSelectorIndex = state;
			return true;
		}
	}
	
#endif
	return false;
}

void UI::FullRedraw()
{
	SDL_BlitSurface(mBackgroundImg, 0, mScreen, 0);
	
	mTopText.Render(mScreen, 0, 0, 480, TA_TopCenter);
	
	
	for (int a = 0; a < mFileNamesValid; a++)
	{
		int arrIdx = (mFileOffset + a) % MAX_FILES_SHOWN;
		mFileNames[arrIdx].Render(mScreen, mXOffset+10, 38 + (a*15), 426-mXOffset, TA_TopLeft);
	
		mFilePercent[arrIdx].Render(mScreen, 480-2, 38 + (a*15), 480, TA_TopRight);
	}
	
	mPeersText.Render(mScreen, 25, 253, 480, TA_TopLeft);
	mTotalsText.Render(mScreen, 166, 253, 480, TA_TopLeft);
	mSpeedText.Render(mScreen, 304, 253, 480, TA_TopLeft);
	mPercentText.Render(mScreen, 480-2, 253, 480, TA_TopRight);

	if (mState == UIS_ConnectDialog)
	{
		SDL_Rect DstRect;
		DstRect.x = 1;
		DstRect.y = 40 + (mSelectorIndex*15);
		SDL_BlitSurface(mSelector, 0, mScreen, &DstRect);
	}
	
	SDL_UpdateRect(mScreen, 0, 0, 0, 0);
}

char* UI::GetTorrentFromUser()
{
	static char file[2048]; // for returning the file name
	sprintf(file, "./torrents/");
	
	DIR* dir = opendir(file);
	struct dirent* entry;
	
	mFileNamesValid = 0;
	
	int a = 0;
	while ((entry = readdir(dir)) != NULL)
	{
		//If a dir or name starts with '.'
		#ifdef PSP
		if ((entry->d_stat.st_attr & FIO_SO_IFDIR) || entry->d_name[0] == '.')
		#else
		if (entry->d_type == DT_DIR || entry->d_name[0] == '.')
		#endif
			continue;
		
		//If string is long enough to contain .torrent and it ends with .torrent
		char* dotPosition = strrchr(entry->d_name, '.');
		if (dotPosition != NULL && strcmp(dotPosition, ".torrent") == 0)
		{
			mFileNamesValid++;
			mFileNames[a].SetText(entry->d_name, COLOR_BLACK);
			
			a++;
			if (a >= MAX_FILES_SHOWN)
				break;
		}
	}
	
	//No torrents, no service!
	if (a == 0)
	{
		printf("No Torrents :(\n");
		exit(0);
	}
	
	
	mTopText.SetText("Select a Torrent", COLOR_BLACK);
	mSelectorIndex = 0;
	while (true)
	{
		GTFURender();
		
		if (GTFUInput())
		{
			//FIXME Return the selected torrent
			break;
		}
		SDL_Delay(1000/60);
	}
	
	strcat(file, mFileNames[mSelectorIndex].GetText());
	mSelectorIndex = 0;
	mFileNamesValid = 0;
	return file;
}

//Render for GetTorrentFromUser
void UI::GTFURender()
{
	SDL_BlitSurface(mBackgroundImg, 0, mScreen, 0);
	
	mTopText.Render(mScreen, 0, 0, 480, TA_TopCenter);
	
	
	for (int a = 0; a < mFileNamesValid; a++)
	{
		mFileNames[a].Render(mScreen, 10, 38 + (a*15), 426, TA_TopLeft);
	}
	
	SDL_Rect DstRect;
	DstRect.x = 1;
	DstRect.y = 40 + (mSelectorIndex*15);
	SDL_BlitSurface(mSelector, 0, mScreen, &DstRect);
	
	SDL_UpdateRect(mScreen, 0, 0, 0, 0);
}

//Input handler for GetTorrentFromUser, returns true if they pressed X to select a torrent
bool UI::GTFUInput()
{
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		switch(event.type){
#ifndef PSP
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
						if (mSelectorIndex > 0)
							mSelectorIndex--;
						break;
						
					case SDLK_DOWN:
						if (mSelectorIndex < mFileNamesValid-1)
							mSelectorIndex++;
						break;
					case SDLK_RETURN:
						return true;
					default:
						break;
				}
				break;
#else
			case SDL_JOYBUTTONDOWN:
				switch(event.jbutton.button)
				{
					case 8: //Up
						if (mSelectorIndex > 0)
							mSelectorIndex--;
						break;
					case 6: //Down
						if (mSelectorIndex < mFileNamesValid-1)
							mSelectorIndex++;
						break;
					case 2: //Cross
					case 1: //Circle
						return true;
					default:
						break;
				}
				break;
#endif
			case SDL_QUIT:
				exit(0);
				break;
		}
	}
	return false;
}


//Updates the list of file names and percent complete from CTorrent
void UI::UpdateFilenames()
{
	char tmp[100];
	
	mTopText.SetText(BTCONTENT.GetDataName(), COLOR_BLACK);
	//FIXME: We should handle up/down presses smartrly, only redraw the needed one, not all of them!
	//       Can use some awesome modulus maths!
	int filecount = BTCONTENT.GetNFiles();
	//printf("REGETTING FILE NAMES Count: %i\n", filecount);
	
	mFileNamesValid = 0;
	
	for (int a = 0; a < filecount && a < MAX_FILES_SHOWN; a++)
	{
		mFileNamesValid = a+1;
		
		//Use modulus maths so we don't need to redraw names when user scrolls (mFileOffset changed)
		int idx = 1 + a + mFileOffset;
		int arrIdx = (mFileOffset + a) % MAX_FILES_SHOWN;
		
		mFileNames[arrIdx].SetText(BTCONTENT.GetFileName(idx), COLOR_BLACK);
	
		sprintf(tmp, "%i%%", BTCONTENT.GetFileCompletionPercent(idx));
		mFilePercent[arrIdx].SetText(tmp, COLOR_BLACK);
	}
}

//Updates the numbers shown in the bottom bar from CTorrent
void UI::UpdateBottomBar()
{
	char tmp[100];
	
	sprintf(tmp, "%i/%i/%i", 
			(int)(WORLD.GetSeedsCount()), 
			 (int)(WORLD.GetPeersCount()) - WORLD.GetSeedsCount(), 
			  (int)(Tracker.GetPeersCount()));
	mPeersText.SetText(tmp, COLOR_BLACK);
	
	sprintf(tmp, "%lluMB/%lluMB", 
			(unsigned long long)(Self.TotalDL() >> 20), 
			 (unsigned long long)(Self.TotalUL() >> 20));
	mTotalsText.SetText(tmp, COLOR_BLACK);
	
	sprintf(tmp, "%iKs/%iKs", (int)(Self.RateDL() >> 10), (int)(Self.RateUL() >> 10));
	mSpeedText.SetText(tmp, COLOR_BLACK);
	
	int have, all;
	BitField tmpBitfield = *BTCONTENT.pBF;
	tmpBitfield.Except(*BTCONTENT.pBMasterFilter);
	have = tmpBitfield.Count();
	all = BTCONTENT.GetNPieces() - BTCONTENT.pBMasterFilter->Count();
	sprintf(tmp, "%i%%", 100 * have / all);
	mPercentText.SetText(tmp, COLOR_BLACK);
}

#ifdef PSP
#undef Render
#endif
///CachedStringSurface
CachedStringSurface::CachedStringSurface()
{
	mFont = NULL;
	mSurface = NULL;
	mCurrentText = NULL;
}

CachedStringSurface::CachedStringSurface(TTF_Font* font)
{
	mFont = font;
	
	mSurface = NULL;
	mCurrentText = NULL;
}

CachedStringSurface::~CachedStringSurface()
{
	if (mSurface)
		SDL_FreeSurface(mSurface);
	if (mCurrentText)
		free(mCurrentText);
}

void CachedStringSurface::SetText(char* text, const SDL_Color &color)
{
	//No current string or != string or != color
	if (mCurrentText == NULL || strcmp(text, mCurrentText) != 0 || memcmp(&color, &mCurrentColor, sizeof(SDL_Color)) != 0)
	{
		if (mSurface)
		{
			SDL_FreeSurface(mSurface);
			mSurface = NULL;
		}
		if (mCurrentText)
		{
			free(mCurrentText);
			mCurrentText = NULL;
		}
		mCurrentText = strdup(text);
		mCurrentColor = color;
		mSurface = TTF_RenderText_Blended(mFont, text, color);
	}
}

void CachedStringSurface::Render(SDL_Surface* target, const int &x, const int &y, const int &maxWidth, TextAlignment align)
{
	if (!mSurface)
		return;
	
	SDL_Rect DstRect;
	
	SDL_Rect SrcRect;
	SrcRect.x = 0;
	SrcRect.y = 0;
	
	SrcRect.w = maxWidth;
	SrcRect.h = mSurface->h;
	
	if (align == TA_TopLeft)
	{
		DstRect.x = x;
		DstRect.y = y;
		SDL_BlitSurface(mSurface, &SrcRect, target, &DstRect);
	}
	else if (align == TA_TopCenter)
	{
		DstRect.x = (target->w - mSurface->w) / 2;
		DstRect.y = y;
		SDL_BlitSurface(mSurface, &SrcRect, target, &DstRect);
	}
	else if (align == TA_TopRight)
	{
		DstRect.x = x - mSurface->w;
		DstRect.y = y;
		SDL_BlitSurface(mSurface, &SrcRect, target, &DstRect);
	}
}

const char* CachedStringSurface::GetText() const
{
	return mCurrentText;
}

#ifdef BUILD_UI_ONLY

#include <unistd.h>

int main()
{
	UI u;
	
	u.FullRedraw();
	
	sleep(2);
}
#endif
