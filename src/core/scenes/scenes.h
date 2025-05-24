#include <iostream>
#include <string>
#include <SDL/SDL.h>

using namespace std;

#ifndef SCENE_H
#define SCENE_H

class Scene {
    friend class SceneManager;
    protected:
        SDL_Renderer* renderer;
        bool StartInitialized = false;
        string sceneName;
        SDL_Event event;

    public:
        Scene(const std::string& name);
        virtual ~Scene();
        virtual void Start() = 0;
        virtual void Update() = 0;
        virtual void Render() = 0;
        virtual void Events(const SDL_Event& event) {}; // Optional

        const string& GetSceneName() const { return sceneName; }
};

#endif

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

class SceneManager {
    protected:
    private:
        Scene* currentScene;
    public:
        SceneManager();
        ~SceneManager();
        void SetScene(Scene* scene);
        void UpdateScene();
        void RenderScene();
        void EventScene(const SDL_Event& event);
};

#endif