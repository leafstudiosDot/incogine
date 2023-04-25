/*
    SDL_uikit_main.c, placed in the public domain by Sam Lantinga  3/18/2019
*/

/* Include the SDL main definition header */
#include <SDL2/SDL_main.h>
#include "../src/core/core.hpp"

#if defined(__IPHONEOS__) || defined(__TVOS__)

#ifndef SDL_MAIN_HANDLED
#ifdef main
#undef main
#endif

int HandleAppEvents(void *userdata, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_APP_TERMINATING:
        /* Terminate the app.
           Shut everything down before returning from this function.
        */
            
        return 0;
    case SDL_APP_LOWMEMORY:
        /* You will get this when your app is paused and iOS wants more memory.
           Release as much memory as possible.
        */
        return 0;
    case SDL_APP_WILLENTERBACKGROUND:
        /* Prepare your app to go into the background.  Stop loops, etc.
           This gets called when the user hits the home button, or gets a call.
        */
        return 0;
    case SDL_APP_DIDENTERBACKGROUND:
        /* This will get called if the user accepted whatever sent your app to the background.
           If the user got a phone call and canceled it, you'll instead get an SDL_APP_DIDENTERFOREGROUND event and restart your loops.
           When you get this, you have 5 seconds to save all your state or the app will be terminated.
           Your app is NOT active at this point.
        */
        return 0;
    case SDL_APP_WILLENTERFOREGROUND:
        /* This call happens when your app is coming back to the foreground.
           Restore all your state here.
        */
        return 0;
    case SDL_APP_DIDENTERFOREGROUND:
        /* Restart your loops here.
           Your app is interactive and getting CPU again.
        */
        return 0;
    default:
        /* No special processing, add it to the event queue */
        return 1;
    }
}

int mainloop() {
    SDL_SetEventFilter(HandleAppEvents, NULL);
}

int main(int argc, char *argv[])
{
    return SDL_UIKitRunApp(argc, argv, mainloop);
}
#endif /* !SDL_MAIN_HANDLED */

#endif /* __IPHONEOS__ || __TVOS__ */

/* vi: set ts=4 sw=4 expandtab: */
