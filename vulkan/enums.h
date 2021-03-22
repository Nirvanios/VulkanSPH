//
// Created by Igor Frank on 25.01.21.
//

#ifndef VULKANAPP_ENUMS_H
#define VULKANAPP_ENUMS_H

enum class DrawType { Particles, Grid};

enum class Visualization { None, Density, PressureForce, Velocity};

enum class SPHStep { advect, compute};

#endif//VULKANAPP_ENUMS_H
