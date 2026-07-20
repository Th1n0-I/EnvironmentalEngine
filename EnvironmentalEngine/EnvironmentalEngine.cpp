// EnvironmentalEngine.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include "Window.h"
#include "Renderer.h"
#include "Timer.h"
#include "Camera.h"
#include "imgui/imgui.h"
#include "Lights.h"

int main()
{
    using namespace EnvironmentalEngine;

    Window window(L"Environmental Engine", 1280, 720);
    Renderer renderer(window.Handle(), window.Width(), window.Height());

    Timer timer;

    Camera camera;

    DirectionalLight dl;
    AmbientLight al;
    PointLight pl;

    MeshRenderer cubeOne = {
        "Cube",
        renderer.CubeMesh(),
        { 0.0f, 0.0f, 1.0f },
        { 55.0f, 25.0f, 120.0f },
        { 3.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f },
        0.5f, 0.5f };

    MeshRenderer cubeTwo = {
        "other Cube",
        renderer.CubeMesh(),
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        0.5f, 0.5f };

    std::vector<MeshRenderer> mr = { cubeOne, cubeTwo };


    while (window.ProccessMessages()) {
        Input& input = window.GetInput();
		timer.Tick();

		camera.Update(input, timer.DeltaTime());

        int deltaX, deltaY;
        window.UpdateMouseLock(deltaX, deltaY);

        if (!ImGui::GetIO().WantCaptureMouse)
            camera.Rotate(deltaX, deltaY);

        renderer.BeginFrame(window.Width(), window.Height(), timer.DeltaTime(), camera.GetViewMatrix(), camera.position, dl, al, pl);

        int id = 0;

        for (MeshRenderer& meshRenderer : mr) {
            ImGui::PushID(id);
            if (ImGui::CollapsingHeader(meshRenderer.name.c_str())) {
                ImGui::DragFloat3("Position", &meshRenderer.position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &meshRenderer.rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &meshRenderer.scale.x, 0.1f);
                ImGui::ColorPicker3("Color", &meshRenderer.color.x);
                ImGui::DragFloat("Smoothness", &meshRenderer.smoothness, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Specular intensity", &meshRenderer.specularIntensity, 0.01f, 0.0f, 1.0f);
            }
            renderer.Draw(meshRenderer);
            ImGui::PopID();
            id++;
        }

        renderer.EndFrame();
    }

    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
