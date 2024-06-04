#include "scenes.h"

SceneManager::SceneManager() : currentScene(nullptr) {}

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
    currentScene->Start();
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

Scene::Scene() {
    
}

Scene::~Scene() {

}