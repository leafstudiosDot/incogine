#include <iostream>
#include "../core/scenes/scenes.h"

extern SceneManager sceneManager;

#ifndef MAINSCENE_H
#define MAINSCENE_H

class MainScene : public Scene {
    public:
        void Start() override;

        void Update() override;

        void Render() override;
};

#endif