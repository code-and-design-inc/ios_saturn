//
//  ChdWrapper.h
//  YabaSnashiro
//
//  Created by Shinya Miyamoto on 2024/07/14.
//  Copyright © 2024 devMiyax. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <MetalANGLE/MGLKViewController.h>
#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint8_t, uint32_t

extern BOOL _bios;
extern int _cart;
extern BOOL _fps;
extern BOOL _frame_skip;
extern BOOL _aspect_rate;
extern int _filter;
extern int _sound_engine;
extern int _rendering_resolution;
extern BOOL _rotate_screen;
extern float _controller_scale;
extern const char * currentGamePath;

extern MGLContext *g_context;
extern MGLContext *g_share_context;
extern MGLLayer *glLayer;
extern NSObject* _objectForLock;


extern const int MSG_SAVE_STATE;
extern const int MSG_LOAD_STATE;
extern const int MSG_RESET;
extern const int MSG_OPEN_TRAY;
extern const int MSG_CLOSE_TRAY;

//    MSG_RESUME,
//    MSG_OPEN_TRAY,
//    MSG_CLOSE_TRAY,
//    MSG_RESET,

static const unsigned int PERANALOG_AXIS_X = 18; // left to right
static const unsigned int PERANALOG_AXIS_Y = 19; // up to down
static const unsigned int PERANALOG_AXIS_RTRIGGER = 20; // right trigger
static const unsigned int PERANALOG_AXIS_LTRIGGER = 21; // left trigger

void PerKeyDown(unsigned int key);
void PerKeyUp(unsigned int key);
int SetAnalogMode(int mode);
void PerAxisValue(unsigned int, unsigned char val);
int start_emulation( int originx, int originy, int width, int height );
void resize_screen( int x, int y, int width, int height );
int emulation_step( int command );
int enterBackGround();
int MuteSound();
int UnMuteSound();


char * getGameinfoFromChd( const char * path );


NSString* YSGetBackupDevicelist();
NSString* YSGetBackupFilelist( int deviceid );
int YSDeleteBackupFile( int index );
NSString* YSGetBackupFile( int index );
int YSPutFile( NSString* jsonstr  );
int YSCopy( int target, int file  );
void YSUpdateCheat(NSArray* stringArray);
NSString* YSGetCurrentGameCode();
void YSOnBackupWrite(const char *fname, int deviceId, const void *before, const void *after, int size);


// RetroAchievements integration functions
#ifdef __cplusplus
extern "C" {
#endif

struct YabauseRA_AchievementList;

// Core initialization and shutdown
int YabauseRA_Initialize();
void YabauseRA_Shutdown();

// User authentication
int YabauseRA_InitializeUser(const char* username, const char* token);
void YabauseRA_Logout();
int YabauseRA_IsUserLoggedIn();
const char* YabauseRA_GetUsername();

// Hardcore mode management
void YabauseRA_SetHardcoreEnabled(int enabled);
int YabauseRA_GetHardcoreEnabled();
void YabauseRA_Reset();

// Game loading and identification
int YabauseRA_LoadGame(const char* hash);
int YabauseRA_LoadGameFromFile(const char* path);
const char* YabauseRA_GetGameHash();

// Rich presence system
void YabauseRA_UpdateRichPresence(const char* richPresence);
const char* YabauseRA_GetRichPresenceDisplayString();

// Achievement and leaderboard callbacks
void YabauseRA_OnAchievementTriggered(int achievementId);
void YabauseRA_TestAchievementUnlock(int achievementId);
void YabauseRA_OnLeaderboardSubmitted(int leaderboardId, int score);

// Memory interface for achievement evaluation
uint32_t YabauseRA_ReadMemory(uint32_t address, uint32_t size);

// Frame processing
void YabauseRA_DoFrame();
void YabauseRA_SetPaused(int paused);
int YabauseRA_GetPaused();

// Media change support for multi-disc games
int YabauseRA_BeginChangeMedia(const char* new_media_path);

// Game image URL
int YabauseRA_GetGameImageURL(char* buffer, size_t buffer_size);

// Progress serialization
size_t YabauseRA_GetProgressSize();
int YabauseRA_SerializeProgress(uint8_t* buffer, size_t buffer_size);
int YabauseRA_DeserializeProgress(const uint8_t* buffer, size_t buffer_size);

// Achievement badge image functions
const char* YabauseRA_GetAchievementBadgeURL(int achievementId, int state);
const char* YabauseRA_GetAchievementBadgeName(int achievementId);



// JSON-based achievement list (safer than struct-based approach)
char* YabauseRA_CreateAchievementListJSON(int category, int grouping);

// Challenge Indicator JSON creation (includes badge URL and achievement details)
char* YabauseRA_CreateChallengeIndicatorJSON(const void* achievement);

#ifdef __cplusplus
}
#endif

// Swift bridge class declaration (forward declaration)
@class RetroAchievementsBridge;
