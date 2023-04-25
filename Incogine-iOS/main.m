//
//  main.m
//  Incogine-iOS
//
//  Created by moritz on 4/24/23.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"
#import <SDL2/SDL.h>

int main(int argc, char * argv[]) {
    NSString * appDelegateClassName;
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
        appDelegateClassName = NSStringFromClass([AppDelegate class]);
    }
    return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
