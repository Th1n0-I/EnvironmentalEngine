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
    camera.position = { 0.0f, 1000.0f, 0.0f };
    camera.moveSpeed = 300.0f;

    

    std::vector<std::unique_ptr<GameObject>> objects = {};

  
    auto lightObject = std::make_unique<GameObject>();
    lightObject->name = "Lights";
    DirectionalLight* dl1 = lightObject->AddComponent<DirectionalLight>();
    AmbientLight* al1 = lightObject->AddComponent<AmbientLight>();;
    PointLight* pl1 = lightObject->AddComponent<PointLight>();;

    auto object3 = std::make_unique<GameObject>();
    object3->name = "Planet";
    object3->transform = { {0.0f, -1000.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1000.0f, 1000.0f, 1000.0f} };
    MeshRenderer* mr3 = object3->AddComponent<MeshRenderer>();
    mr3->mesh = renderer.PlanetMesh();

    objects.push_back(std::move(lightObject));
    objects.push_back(std::move(object3));

    while (window.ProccessMessages()) {
        Input& input = window.GetInput();
		timer.Tick();

		camera.Update(input, timer.DeltaTime());

        int deltaX, deltaY;
        window.UpdateMouseLock(deltaX, deltaY);

        if (!ImGui::GetIO().WantCaptureMouse)
            camera.Rotate(deltaX, deltaY);

        renderer.BeginFrame(window.Width(), window.Height(), timer.DeltaTime(), camera.GetViewMatrix(), camera.position,
            *dl1,
            *al1,
            *pl1);

        ImGui::DragFloat("Move speed", &camera.moveSpeed, 0.4f);

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

                if (AmbientLight* al = go.GetComponent<AmbientLight>()) {
                    if (ImGui::CollapsingHeader(al->TypeName())) {
                        ImGui::ColorPicker3("Color", &al->color.x);
                        ImGui::DragFloat("intensity", &al->intensity, 0.01f, 0.0f, 1.0f);
                    }
                }

                if (DirectionalLight* dl = go.GetComponent<DirectionalLight>()) {
                    if (ImGui::CollapsingHeader(dl->TypeName())) {
                        ImGui::ColorPicker3("Color", &dl->color.x);
                        ImGui::DragFloat3("Direction", &dl->direction.x);
                        ImGui::DragFloat("intensity", &dl->intensity, 0.01f, 0.0f, 1.0f);
                    }
                }

                if (PointLight* pl = go.GetComponent<PointLight>()) {
                    if (ImGui::CollapsingHeader(pl->TypeName())) {
                        ImGui::ColorPicker3("Color", &pl->color.x);
                        ImGui::DragFloat3("Direction", &pl->position.x);
                        ImGui::DragFloat("intensity", &pl->intensity, 0.01f, 0.0f, 1.0f);
                    }
                }
            }

            ImGui::PopID();
            if(MeshRenderer* mr = go.GetComponent<MeshRenderer>())
                renderer.Draw(*mr, go.transform);
        }

        renderer.DrawAtmosphere(camera.position);
        

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
