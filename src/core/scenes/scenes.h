#include <iostream>
#include <SDL/SDL.h>
#include <string>

using namespace std;

#ifndef SCENE_H
#define SCENE_H

class Scene {
    friend class SceneManager;
    protected:
        SDL_Renderer* renderer;
        bool StartInitialized = false;
        string sceneName;
    public:
        Scene(const std::string& name = "Scene") : sceneName(name), renderer(nullptr) {}
        virtual ~Scene();
        virtual void Start() = 0;
        virtual void Update() = 0;
        virtual void Render() = 0;

        const string& GetSceneName() const { return sceneName; }
};

#endif

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

class SceneManager {
    protected:
        SDL_Renderer* renderer;
    private:
        Scene* currentScene;
    public:
        SceneManager(SDL_Renderer& renderer);
        ~SceneManager();
        void SetScene(Scene* scene);
        void UpdateScene();
        void RenderScene();
};

#endif