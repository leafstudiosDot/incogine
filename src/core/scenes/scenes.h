#include <iostream>

#ifndef SCENE_H
#define SCENE_H

class Scene {
    private:

    public:
        Scene();
        virtual ~Scene();
        virtual void Start() = 0;
        virtual void Update() = 0;
        virtual void Render() = 0;
};

#endif

#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

class SceneManager {
    private:
        Scene* currentScene;
    public:
        SceneManager();
        ~SceneManager();
        void SetScene(Scene* scene);
        void UpdateScene();
        void RenderScene();
};

#endif