//
//  game.hpp
//  Incogine
//
//  Created by moritz on 10/17/22.
//

#ifndef game_hpp
#define game_hpp

#include "../core/core.hpp"

class Game {
public:
    Game();
    ~Game();
    
    void RawEvent(SDL_Event event, int _windowWidth, int _windowHeight);
    void Event(SDL_Event event, int _windowWidth, int _windowHeight);
    void Start(int _windowWidth, int _windowHeight);
    void Update(int _windowWidth, int _windowHeight);
    void Render(int _windowWidth, int _windowHeight);
    void RenderCanvas(int _windowWidth, int _windowHeight, bool devMode);
    void Destroy();
    
private:
    int SceneNow;
};

#endif /* game_hpp */
