#include <iostream>
#include "../../core/scenes/scenes.h"

#ifndef SPLASH_H
#define SPLASH_H

class Splash : public Scene {
    public:
        void Start() override;

        void Update() override;

        void Render() override;
};

#endif