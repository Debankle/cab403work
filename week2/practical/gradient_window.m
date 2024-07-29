#include <Cocoa/Cocoa.h>

// Custom view to draw the gradient
@interface GradientView : NSView
@end

@implementation GradientView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Create a gradient
    NSColor *startColor = [NSColor redColor];
    NSColor *endColor = [NSColor blueColor];
    
    NSGradient *gradient = [[NSGradient alloc] initWithColors:@[startColor, endColor]];
    
    // Draw the gradient from top-left to bottom-right
    [gradient drawInRect:self.bounds angle:45.0];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create the application
        NSApplication *app = [NSApplication sharedApplication];
        
        // Create the window
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 800, 600)
                          styleMask:(NSWindowStyleMaskTitled |
                                     NSWindowStyleMaskClosable |
                                     NSWindowStyleMaskResizable)
                            backing:NSBackingStoreBuffered
                              defer:NO];
        
        // Set the window title
        [window setTitle:@"Gradient Example"];
        
        // Create the gradient view and set it as the content view of the window
        GradientView *gradientView = [[GradientView alloc] initWithFrame:[window contentRectForFrameRect:[window frame]]];
        [window setContentView:gradientView];
        
        // Show the window
        [window makeKeyAndOrderFront:nil];
        
        // Start the application event loop
        [app run];
    }
    return 0;
}
