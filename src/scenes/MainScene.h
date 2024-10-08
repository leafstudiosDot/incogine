#include <iostream>
#include "../core/scenes/scenes.h"

#ifndef MAINSCENE_H
#define MAINSCENE_H

class MainScene : public Scene {
    public:
        MainScene();
        ~MainScene();

        void Start() override;
        void Update() override;
        void Render() override;
};

#endif