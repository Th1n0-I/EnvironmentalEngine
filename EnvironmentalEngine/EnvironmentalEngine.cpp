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
#include "imgui/imgui_stdlib.h"
#include "GameObject.h"

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

    std::vector<std::unique_ptr<GameObject>> objects = {};

    auto object1 = std::make_unique<GameObject>();
    object1->name = "Mango";
    object1->transform = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} };
    MeshRenderer* mr = object1->AddComponent<MeshRenderer>();
    mr->mesh = renderer.CubeMesh();

    objects.push_back(std::move(object1));

    while (window.ProccessMessages()) {
        Input& input = window.GetInput();
		timer.Tick();

		camera.Update(input, timer.DeltaTime());

        int deltaX, deltaY;
        window.UpdateMouseLock(deltaX, deltaY);

        if (!ImGui::GetIO().WantCaptureMouse)
            camera.Rotate(deltaX, deltaY);

        renderer.BeginFrame(window.Width(), window.Height(), timer.DeltaTime(), camera.GetViewMatrix(), camera.position, dl, al, pl);

        for (size_t i = 0; i < objects.size(); i++ ) {
            GameObject& go = *objects[i];

            ImGui::PushID(i);

            std::string header = go.name + "###obj";
            if (ImGui::CollapsingHeader(header.c_str()))
            {
                ImGui::InputText("Name", &go.name);

                ImGui::DragFloat3("position", &go.transform.position.x, 0.1f);
                ImGui::DragFloat3("rotation", &go.transform.rotation.x, 0.1f);
                ImGui::DragFloat3("scale", &go.transform.scale.x, 0.1f);

                if (MeshRenderer* mr = go.GetComponent<MeshRenderer>()) {
                    if (ImGui::CollapsingHeader(mr->TypeName())) {
                        ImGui::ColorPicker3("Color", &mr->color.x);
                        ImGui::DragFloat("Smoothness", &mr->smoothness, 0.01f, 0.0f, 1.0f);
                        ImGui::DragFloat("Specular Intensity", &mr->specularIntensity, 0.01f, 0.0f, 1.0f);
                    }
                }
            }

            ImGui::PopID();

            renderer.Draw(*go.GetComponent<MeshRenderer>(), go.transform);
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
