#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

enum TextAlignment
{
	TA_TopLeft,
	TA_TopCenter,
	TA_TopRight
};

class CachedStringSurface
{
	public:
		CachedStringSurface(); //DONT USE!!!!!!!!!!
		CachedStringSurface(TTF_Font* font);
		~CachedStringSurface();
		
		void SetText(char* text, const SDL_Color &color);
		void Render(SDL_Surface* target, const int &x, const int &y, const int &maxWidth, TextAlignment align);
		
		//Gets the text on the surface, DO NOT FREE!
		const char* GetText() const;
	private:
		char* mCurrentText;
		SDL_Color mCurrentColor;
		SDL_Surface* mSurface;
		
		TTF_Font* mFont;
};

enum UIState
{
	UIS_Invalid,
	UIS_Viewing,
	UIS_ConnectDialog,
	UIS_Connecting
};

#define MAX_FILES_SHOWN 13

class UI
{
	public:
		UI();
	
		//Loads the UI parts from disk
		//Asks the user what torrent to download
		//Boots the UI Thread
		//Returns the torrent file name
		char* Initialise();
		
		//Specifies the UI should redraw itself on the next loop
		void QueueRedraw();
		
		//Called by the thread function
		void Poll();
	private:
		UIState mState;
		void SwitchState(const UIState &newState);
		
		bool HandleInput_Viewing(const SDL_Event &event);
		bool HandleInput_ConnectDialog(const SDL_Event &event);
		
		bool Poll_Connecting();
		
		void FullRedraw();
		
		///Called during Initialise:
		//Gets the user to browse around for a torrent file
		char* GetTorrentFromUser();
		//Render for GetTorrentFromUser
		void GTFURender();
		//Input handler for GetTorrentFromUser, returns true if they pressed X to select a torrent
		bool GTFUInput();
		
		
		//Updates filenames from BTCONTENT
		//returns true if they have changed
		void UpdateFilenames();
		
		//Updates the text in the bottom bar
		void UpdateBottomBar();
		SDL_Thread *mThread;
		//Screen
		SDL_Surface *mScreen;

		//Background
		SDL_Surface *mBackgroundImg;
		
		//Selector
		SDL_Surface *mSelector;
		int mSelectorIndex;
		
		//Text
		TTF_Font* mStandardFont;
		TTF_Font* mBigBoldFont;
		
		CachedStringSurface mTopText;
		
		//Files list
		CachedStringSurface mFileNames[MAX_FILES_SHOWN];
		CachedStringSurface mFilePercent[MAX_FILES_SHOWN];
		int mFileOffset; //Offset into the btFiles.
		int mFileNamesValid; //how many of the mFileNames are valid, incase total files < MAX_FILES_SHOWN
		
		int mXOffset; //Scroll amount in the X direction
		int mDXOffset; //Amount we are scrolling by (if we are scrolling)
		
		//Text at the bottom
		CachedStringSurface mPeersText;
		CachedStringSurface mTotalsText;
		CachedStringSurface mSpeedText;
		CachedStringSurface mPercentText;
};

extern UI UI_Instance;
