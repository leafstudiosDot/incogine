#include "scenes.h"
#include "../engine/engine.h"

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
    if (currentScene != nullptr) {
        if (Engine::Instance(0, nullptr)->inDevMode()) {
			cout << "Entering scene: " << currentScene->GetSceneName() << endl;
        }
        //currentScene->renderer = renderer;
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

void SceneManager::EventScene(const SDL_Event& event) {
	if (currentScene != nullptr) {
		currentScene->Events(event);
	}
}

Scene::Scene(const string& name = "Scene") : sceneName(name), renderer(nullptr) {
    event = Engine::Instance(0, nullptr)->GetEventProvider();
}

Scene::~Scene() {

}