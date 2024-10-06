#include "scenes.h"

SceneManager::SceneManager(SDL_Renderer& renderer) : currentScene(nullptr) {}

SceneManager::~SceneManager() {
    if (currentScene != nullptr) {
        delete currentScene;
    }
}

void SceneManager::SetScene(Scene* scene) {
    if (currentScene != nullptr) {
        delete currentScene;
    }
    currentScene = scene;
    if (currentScene != nullptr) {
        currentScene->renderer = renderer;
        currentScene->Start();
        currentScene->StartInitialized = true;
    }
}

void SceneManager::UpdateScene() {
    if (currentScene != nullptr) {
        currentScene->Update();
    }
}

void SceneManager::RenderScene() {
    if (currentScene != nullptr) {
        currentScene->Render();
    }
}

Scene::Scene() : renderer(nullptr) {
    
}

Scene::~Scene() {

}