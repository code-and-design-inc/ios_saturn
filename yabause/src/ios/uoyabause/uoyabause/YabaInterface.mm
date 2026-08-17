//
//  YabaInterface.m
//  YabaSnashiro
//
//  Created by Shinya Miyamoto on 2024/07/20.
//  Copyright © 2024 devMiyax. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AuthenticationServices/AuthenticationServices.h>
#import <CommonCrypto/CommonDigest.h>
#include <MetalANGLE/GLES3/gl3.h>
#import <MetalANGLE/MGLKViewController.h>
#include <string>
#include "../../../BackupManager.h"
#include "../../../cheat.h"
#include "../../../cs2.h"
#include "../../../retroachievements/yabause_ra_integration_cwrapper.h"

// Use rcheevos integration if available, fallback to iOS implementation
// Set to 0 to use iOS fallback, 1 to use rcheevos C++ wrapper
#define USE_RCHEEVOS_INTEGRATION 0

extern "C" {


#define CART_NONE            0
#define CART_PAR             1
#define CART_BACKUPRAM4MBIT  2
#define CART_BACKUPRAM8MBIT  3
#define CART_BACKUPRAM16MBIT 4
#define CART_BACKUPRAM32MBIT 5
#define CART_DRAM8MBIT       6
#define CART_DRAM32MBIT      7
#define CART_NETLINK         8
#define CART_ROM16MBIT       9


MGLContext *g_context = nil;
MGLContext *g_share_context = nil;


// Settings
BOOL _bios =YES;
int _cart = 0;
BOOL _fps = NO;
BOOL _frame_skip = NO;
BOOL _aspect_rate = NO;
int _filter = 0;
int _sound_engine = 0;
int _rendering_resolution = 0;
BOOL _rotate_screen = NO;
float _controller_scale = 1.0;
const char * currentGamePath = NULL;

extern "C" const int MSG_SAVE_STATE = 1;
extern "C" const int MSG_LOAD_STATE = 2;
extern "C" const int MSG_RESET = 3;
extern "C" const int MSG_OPEN_TRAY = 4;
extern "C" const int MSG_CLOSE_TRAY = 5;


GLuint _renderBuffer = 0;
NSObject* _objectForLock;
MGLLayer *glLayer = nil;


int swapAglBuffer (void)
{
    if( glLayer == nil ) return 0;
    
    @synchronized (_objectForLock){
        MGLContext* context = [MGLContext currentContext];
        if (![context present:glLayer]) {
        }
    }
    return 0;
}

void RevokeOGLOnThisThread(void){
    [MGLContext setCurrentContext:g_share_context forLayer:glLayer];
}

void UseOGLOnThisThread(void){
    [MGLContext setCurrentContext:g_context forLayer:glLayer];
}

const char * GetBiosPath(void){
#if 1
    return NULL;
#else
    if( _bios == YES ){
        return NULL;
    }
    
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"bios.bin";
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    // check if file exists
    if ([filemgr fileExistsAtPath: filePath] == YES){
        NSLog(@"File exists");
        
    }else {
        NSLog (@"File not found, file will be created");
        return NULL;
    }
    
    return [filePath fileSystemRepresentation];
#endif
}

const char * GetGamePath(void){
    
    //if( sharedData_ == nil ){
    //    return nil;
    // }
    //NSString *path = sharedData_.selected_file;
    //return [path cStringUsingEncoding:1];
    return currentGamePath;;
}

const char * GetStateSavePath(void){
    BOOL isDir;
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"state/";
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *dirName = [docDir stringByAppendingPathComponent:@"state"];
    
    
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dirName isDirectory:&isDir])
    {
        if([fm createDirectoryAtPath:dirName withIntermediateDirectories:YES attributes:nil error:nil])
            NSLog(@"Directory Created");
        else
            NSLog(@"Directory Creation Failed");
    }
    else
        NSLog(@"Directory Already Exist");
    
    return [filePath fileSystemRepresentation];
}

const char * GetMemoryPath(void){
    BOOL isDir;
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"backup/memory.bin";
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *dirName = [docDir stringByAppendingPathComponent:@"backup"];
    
    
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dirName isDirectory:&isDir])
    {
        if([fm createDirectoryAtPath:dirName withIntermediateDirectories:YES attributes:nil error:nil])
            NSLog(@"Directory Created");
        else
            NSLog(@"Directory Creation Failed");
    }
    else
        NSLog(@"Directory Already Exist");
    
    // check if file exists
    if ([filemgr fileExistsAtPath: filePath] == YES){
        NSLog(@"File exists");
        
    }else {
        NSLog (@"File not found, file will be created");
    }
    
    return [filePath fileSystemRepresentation];
}

int GetCartridgeType(void){
    return _cart;
}

int GetVideoInterface(void){
    return 0;
}

int GetEnableFPS(void){
    if( _fps == YES )
        return 1;
    
    return 0;
}

int GetIsRotateScreen(void) {
    if( _rotate_screen == YES )
        return 1;
    
    return 0;
}

int GetEnableFrameSkip(void){
    if( _frame_skip == YES ){
        return 1;
    }
    return 0;
}

int GetUseNewScsp(void){
    return 1; //_sound_engine;
}

int GetVideFilterType(void){
    return _filter;
}

int GetResolutionType(void){
    NSLog (@"GetResolutionType %d",_rendering_resolution);
    return _rendering_resolution;
}

const char * GetCartridgePath(void){
    BOOL isDir;
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"cart/invalid.ram";
    
    switch(_cart) {
        case CART_NONE:
            fileName = @"cart/none.ram";
        case CART_PAR:
            fileName = @"cart/par.ram";
        case CART_BACKUPRAM4MBIT:
            fileName = @"cart/backup4.ram";
        case CART_BACKUPRAM8MBIT:
            fileName = @"cart/backup8.ram";
        case CART_BACKUPRAM16MBIT:
            fileName = @"cart/backup16.ram";
        case CART_BACKUPRAM32MBIT:
            fileName = @"cart/backup32.ram";
        case CART_DRAM8MBIT:
            fileName = @"cart/dram8.ram";
        case CART_DRAM32MBIT:
            fileName = @"cart/dram32.ram";
        case CART_NETLINK:
            fileName = @"cart/netlink.ram";
        case CART_ROM16MBIT:
            fileName = @"cart/om16.ram";
        default:
            fileName = @"cart/invalid.ram";
    }
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *dirName = [docDir stringByAppendingPathComponent:@"cart"];
    
    
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dirName isDirectory:&isDir])
    {
        if([fm createDirectoryAtPath:dirName withIntermediateDirectories:YES attributes:nil error:nil])
            NSLog(@"Directory Created");
        else
            NSLog(@"Directory Creation Failed");
    }
    else
        NSLog(@"Directory Already Exist");
    
    // check if file exists
    if ([filemgr fileExistsAtPath: filePath] == YES){
        NSLog(@"File exists");
        
    }else {
        NSLog (@"File not found, file will be created");
    }
    return [filePath fileSystemRepresentation];
}

int GetPlayer2Device(void){
    return -1;
}

NSString* YSGetBackupDevicelist(){
    BackupManager * i = BackupManager::getInstance();
    string jsonstr;
    i->getDevicelist(jsonstr);
    NSString *objcString = [NSString stringWithUTF8String:jsonstr.c_str()];
    return objcString;
}

NSString* YSGetBackupFilelist( int deviceid ){
    BackupManager * i = BackupManager::getInstance();
    string jsonstr;
    i->getFilelist(deviceid,jsonstr);
    NSString *objcString = [NSString stringWithUTF8String:jsonstr.c_str()];
    return objcString;
}

int YSDeleteBackupFile( int index ){
    BackupManager * i = BackupManager::getInstance();
    string jsonstr;
    return i->deletefile(index);
}

NSString* YSGetBackupFile( int index ){
    BackupManager * i = BackupManager::getInstance();
    string jsonstr;
    i->getFile(index,jsonstr);
    NSString *objcString = [NSString stringWithUTF8String:jsonstr.c_str()];
    return objcString;
}

int YSPutFile( NSString* jsonstr  ){
    const char *cString = [jsonstr UTF8String];
    if (cString == NULL) return -1;
    BackupManager * i = BackupManager::getInstance();
    int rtn = i->putFile(string(cString));
    return rtn;
}

int YSCopy( int target, int file  ){
    BackupManager * i = BackupManager::getInstance();
    return i->copy(target,file);
}

void YSUpdateCheat(NSArray* stringArray) {
    if (stringArray == nil || [stringArray count] == 0) {
        CheatClearCodes();
        return;
    }
    
    int stringCount = (int)[stringArray count];
    int index = 0;
    CheatClearCodes();
    
    for (int i = 0; i < stringCount; i++) {
        NSString* string = [stringArray objectAtIndex:i];
        if (string == nil) {
            continue;
        }
        const char* rawString = [string UTF8String];
        index = CheatAddARCode(rawString);
        CheatEnableCode(index);
    }
    // CheatDoPatches(); will call at Vblank-in
    return;
}

NSString* YSGetCurrentGameCode() {
    const char *gameCode = Cs2GetCurrentGmaecode();
    if (gameCode == NULL) {
        return nil;
    }
    NSString *nsGameCode = [NSString stringWithUTF8String:gameCode];
    return nsGameCode;
}

} // Close extern "C" block temporarily

// Import Swift generated header for RetroAchievements bridge
// Try different possible Swift header names
#if __has_include("uoyabause-Swift.h")
  #import "uoyabause-Swift.h"
#elif __has_include("YabaSnashiro-Swift.h")
  #import "YabaSnashiro-Swift.h"
#elif __has_include("YabaSnashiro_Lite-Swift.h")
  #import "YabaSnashiro_Lite-Swift.h"
#else
  #warning "Could not find Swift bridging header - RetroAchievements bridge may not work"
  // Define forward declarations to prevent compilation errors
  @class RetroAchievementsBridge;
#endif

extern "C" {

// RetroAchievements integration functions
#include <dispatch/dispatch.h>

// Global state for RetroAchievements
static BOOL ra_initialized = NO;
static BOOL ra_user_logged_in = NO;
static BOOL ra_hardcore_enabled = NO;
static BOOL ra_paused = NO;
static char ra_username[256] = {0};
static char ra_token[256] = {0};
static char ra_rich_presence[512] = {0};

// HTTP request callback for native layer
typedef void (*http_request_callback_t)(const char* response, const char* error, void* userdata);

struct http_request_data {
    http_request_callback_t callback;
    void* userdata;
};

// Bridge to Swift HTTP implementation
static void perform_http_request(const char* url, const char* post_data, http_request_callback_t callback, void* userdata) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* nsUrl = [NSString stringWithUTF8String:url];
        NSString* nsPostData = post_data ? [NSString stringWithUTF8String:post_data] : nil;
        
        // Call Swift implementation via bridge class
        [RetroAchievementsBridge performHTTPRequest:nsUrl postData:nsPostData completion:^(NSString* response, NSString* error) {
            if (callback) {
                const char* responseStr = response ? [response UTF8String] : NULL;
                const char* errorStr = error ? [error UTF8String] : NULL;
                callback(responseStr, errorStr, userdata);
            }
        }];
    });
}

// RetroAchievements progress serialization (iOS implementation)
size_t YabauseRA_GetProgressSize() {
    // Return minimum size needed for basic state serialization
    return 64;
}

int YabauseRA_SerializeProgress(uint8_t* buffer, size_t buffer_size) {
    // Use iOS implementation directly
    if (buffer != NULL && buffer_size >= 64 && ra_initialized) {
        // Version header (4 bytes)
        buffer[0] = 'R';
        buffer[1] = 'A';
        buffer[2] = 0x01; // Version 1
        buffer[3] = 0x00;
        
        // User login state (1 byte)
        buffer[4] = ra_user_logged_in ? 1 : 0;
        
        // Hardcore mode state (1 byte)
        buffer[5] = ra_hardcore_enabled ? 1 : 0;
        
        // Reserved bytes
        memset(&buffer[6], 0, buffer_size - 6);
        
        return 1;
    }
    return 0;
}

int YabauseRA_DeserializeProgress(const uint8_t* buffer, size_t buffer_size) {
    // Use iOS implementation directly
    if (buffer != NULL && buffer_size >= 6) {
        // Check version header
        if (buffer[0] == 'R' && buffer[1] == 'A' && buffer[2] == 0x01) {
            // Restore user login state
            ra_user_logged_in = (buffer[4] != 0);
            
            // Restore hardcore mode state
            ra_hardcore_enabled = (buffer[5] != 0);
            
            return 1; // Success
        }
    }
    return 0; // Failure
}


// YabauseRA_CreateAchievementList and YabauseRA_DestroyAchievementList
// are implemented in yabause_ra_integration_cwrapper.cpp
// No implementation needed here - C++ functions will be linked directly

// NOTICE: All iOS RetroAchievements functions have been REMOVED
// The actual implementation is now in yabause_ra_integration_cwrapper.cpp
// which provides the proper rcheevos integration following the official guide:
// https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration

// All YabauseRA_* functions are now provided by the C++ wrapper in:
// - yabause_ra_integration_cwrapper.h (declarations)  
// - yabause_ra_integration_cwrapper.cpp (implementation)
// 
// These functions use the official rcheevos library and follow the integration guide.
// The iOS-specific direct API implementation has been completely removed.

} // extern "C"
