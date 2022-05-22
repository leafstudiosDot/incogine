#include "core/core.hpp"
Core *core = nullptr;

Console console;

#define _WINDOW_NAME "leafstudiosDot"

bool Core::corerunning;

int main(int argc, char* argv[]) {
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        system("cls");
    #elif __APPLE__
        system("clear");
    #endif

    console.Println("Incogine by leafstudiosDot");
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    SDL_Window* window = nullptr;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        //console.Println("Init Core Error " + SDL_GetError());
        cout << "Init Core Error " << SDL_GetError() << endl;
        return -1;
    }

    if (IMG_Init(IMG_INIT_PNG) < 0) {
        cout << "Init Image Error " << IMG_GetError() << endl;
        return -1;
    }

    int width = 1280;
    int height = 720;
    int windowWidth;
    int windowHeight;
    
    console.Println("Initializing Core...");
    core = new Core();
        
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        windowWidth = width*1.5;
        windowHeight = height*1.5;
    #else
        windowWidth = width;
        windowHeight = height;
    #endif

    int flags = SDL_WINDOW_ALLOW_HIGHDPI + SDL_WINDOW_RESIZABLE + SDL_WINDOW_SHOWN;
    #if __APPLE__
        //flags = flags + SDL_WINDOW_METAL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
        flags = flags + SDL_WINDOW_OPENGL; // uncomment later when Apple is accepting OpenGL and Metal is friendship.
    #else
        flags = flags + SDL_WINDOW_OPENGL;
    #endif

    window = SDL_CreateWindow(_WINDOW_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);
    Core::corerunning = true;

    SDL_GLContext context;
    context = SDL_GL_CreateContext(window);
    if( SDL_GL_SetSwapInterval(1) < 0 )
    {
        printf( "Warning: Unable to enable VSync: %s\n", SDL_GetError() );
    }
    
    glewExperimental=GL_TRUE;
    GLenum glew_init_error = glewInit(); 
    
    glMatrixMode(GL_PROJECTION_MATRIX);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW_MATRIX);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    
    //gladLoadGLLoader(SDL_GL_GetProcAddress);
    
    console.Println("Starting...");
    SDL_Delay(1000);
    core->StartInit();

    while (Core::corerunning) {
        glViewport(0, 0, windowWidth, windowHeight);
        
        //Uint64 startF = SDL_GetPerformanceCounter();
        core->Event();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        core->Update();
        core->Render();
        
        /*Uint64 endF = SDL_GetPerformanceCounter();
        float elapsedMS = (endF-startF) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
        SDL_Delay(floor(16.666f - elapsedMS));*/
        
        glFlush();

        SDL_GL_SwapWindow(window);
    }
    
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();
    delete core;

    return 0;
}
