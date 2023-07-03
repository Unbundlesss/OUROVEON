
#import <Cocoa/Cocoa.h>

int xpOpenURL(const char *url)
{
    @autoreleasepool {
        CFURLRef cfurl = CFURLCreateWithBytes(NULL, (const UInt8 *)url, SDL_strlen(url), kCFStringEncodingUTF8, NULL);
        OSStatus status = LSOpenCFURLRef(cfurl, NULL);
        CFRelease(cfurl);
        return status == noErr ? 0 : -1;
    }
}
