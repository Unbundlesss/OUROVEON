
#import <Cocoa/Cocoa.h>

// maybe?
// NSArray *fileURLs = [NSArray arrayWithObjects:fileURL1, /* ... */ nil];
// [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:fileURLs];

// NSString* path = @"/Users/user/Downloads/my file"
// NSArray *fileURLs = [NSArray arrayWithObjects:[NSURL fileURLWithPath:path], nil];
// [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:fileURLs];


int xpOpenURL(const char *url)
{
    @autoreleasepool {
        CFURLRef cfurl = CFURLCreateWithBytes(NULL, (const UInt8 *)url, std::strlen(url), kCFStringEncodingUTF8, NULL);
        OSStatus status = LSOpenCFURLRef(cfurl, NULL);
        CFRelease(cfurl);
        return status == noErr ? 0 : -1;
    }
}

int xpOpenFolder(const char *folder)
{
    @autoreleasepool {
        NSArray *fileURLs = [NSArray arrayWithObjects:[NSURL fileURLWithPath:[NSString stringWithUTF8String:folder]], nil];
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:fileURLs];
        return 0;
    }
}
