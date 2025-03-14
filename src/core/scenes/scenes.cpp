#include "scenes.h"
#include "../engine/engine.h"

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
        if (Engine::Instance(0, nullptr)->inDevMode()) {
			cout << "Entering scene: " << currentScene->GetSceneName() << endl;
        }
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

/*Scene::Scene(const string& name = "Scene") : sceneName(name), renderer(nullptr) {
	this->sceneName = sceneName;
}*/

Scene::~Scene() {

}