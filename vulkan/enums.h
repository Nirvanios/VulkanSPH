//
// Created by Igor Frank on 25.01.21.
//

#ifndef VULKANAPP_ENUMS_H
#define VULKANAPP_ENUMS_H

enum class SimulationState { Stopped, Simulating, SingleStep, Reset};

enum class RecordingState { Stopped = 1 << 0 , Recording= 1 << 1, Screenshot= 1 << 2};

enum class DrawType { Particles = 1 << 0, Grid = 1 << 1, ToTexture = 1 << 2, ToFile = 1 << 3};

enum class Visualization { None, Density, PressureForce, Velocity, Temperature};

enum class SPHStep { advect, massDensity, force};

enum class CouplingStep { tag, transfer};

#endif//VULKANAPP_ENUMS_H
