//Just a simple handler for simple initialization stuffs
#include "Utilities/BackendHandler.h"

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <Texture2D.h>
#include <Texture2DData.h>
#include <MeshBuilder.h>
#include <MeshFactory.h>
#include <NotObjLoader.h>
#include <ObjLoader.h>
#include <VertexTypes.h>
#include <ShaderMaterial.h>
#include <RendererComponent.h>
#include <TextureCubeMap.h>
#include <TextureCubeMapData.h>

#include <Timing.h>
#include <GameObjectTag.h>
#include <InputHelpers.h>

#include <Behaviours/CameraControlBehaviour.h>
#include <Behaviours/RotateObjectBehaviour.h>

#include <IBehaviour.h>
#include <FollowPathBehaviour.h>
#include <SimpleMoveBehaviour.h>

int main() {
	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui
		Shader::sptr passthroughShader = Shader::Create();
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		passthroughShader->LoadShaderPartFromFile("shaders/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
		passthroughShader->Link();

		Shader::sptr colourCorrectionShader = Shader::Create();
		colourCorrectionShader->LoadShaderPartFromFile("shaders/passthrough_vert.glsl", GL_VERTEX_SHADER);
		colourCorrectionShader->LoadShaderPartFromFile("shaders/Post/colour_correction_frag.glsl", GL_FRAGMENT_SHADER);
		colourCorrectionShader->Link();

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();

		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 5.0f);
		glm::vec3 lightCol = glm::vec3(0.9f, 0.85f, 0.5f);
		float     lightAmbientPow = 0.05f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.09f;
		float     lightQuadraticFalloff = 0.032f;
		bool	  noLighting = true;
		bool	  ambientOnly = false;
		bool	  specularOnly = false;
		bool	  applyBloom = false;
		bool	  ambientAndSpecular = false;
		bool	  ambientSpecularAndBloom = false;

		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		shader->SetUniform("u_NoLight", (int)noLighting);
		shader->SetUniform("u_AmbientOnly", (int)ambientOnly);
		shader->SetUniform("u_SpecularOnly", (int)specularOnly);

		PostEffect* basicEffect;

		int activeEffect = 2;
		std::vector<PostEffect*> effects;

		GreyscaleEffect* greyscaleEffect;

		SepiaEffect* sepiaEffect;

		BloomEffect* bloomEffect;

		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::Checkbox("No Lighting", &noLighting)) {
				noLighting = true;
				ambientOnly = false;
				specularOnly = false;
				applyBloom = false;
				ambientAndSpecular = false;
				ambientSpecularAndBloom = false;
			}
			if (ImGui::Checkbox("Ambient Light Only", &ambientOnly)) {
				noLighting = false;
				ambientOnly = true;
				specularOnly = false;
				applyBloom = false;
				ambientAndSpecular = false;
				ambientSpecularAndBloom = false;
			}
			if (ImGui::Checkbox("Specular Light Only", &specularOnly)) {
				noLighting = false;
				ambientOnly = false;
				specularOnly = true;
				applyBloom = false;
				ambientAndSpecular = false;
				ambientSpecularAndBloom = false;
			}
			if (ImGui::Checkbox("Ambient and Specular Light", &ambientAndSpecular)) {
				noLighting = false;
				applyBloom = false;
				ambientSpecularAndBloom = false;
				if (ambientAndSpecular)
				{
					specularOnly = false;
					ambientOnly = false;
				}
				else
				{
					specularOnly = true;
					ambientOnly = true;
				}
			}
			if (ImGui::Checkbox("Ambient and Specular Light with Custom Shader", &ambientSpecularAndBloom)) {
				noLighting = false;
				ambientAndSpecular = false;
				if (ambientSpecularAndBloom)
				{
					specularOnly = false;
					ambientOnly = false;
					applyBloom = true;
				}
				else
				{
					specularOnly = true;
					ambientOnly = true;
					applyBloom = false;
				}
			}
			shader->SetUniform("u_NoLight", (int)noLighting);
			shader->SetUniform("u_SpecularOnly", (int)specularOnly);
			shader->SetUniform("u_AmbientOnly", (int)ambientOnly);
			bloomEffect->SetShaderUniform("u_ApplyBloom", (int)applyBloom);
			/*if (ImGui::CollapsingHeader("Effect Controls"))
			{
				ImGui::SliderInt("Chosen Effect", &activeEffect, 0, effects.size() - 1);

				if (activeEffect == 0)
				{
					ImGui::Text("Active Effect: Greyscale Effect");

					GreyscaleEffect* temp = (GreyscaleEffect*)effects[activeEffect];
					float intensity = temp->GetIntensity();

					if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f))
					{
						temp->SetIntensity(intensity);
					}
				}
				if (activeEffect == 1)
				{
					ImGui::Text("Active Effect: Sepia Effect");

					SepiaEffect* temp = (SepiaEffect*)effects[activeEffect];
					float intensity = temp->GetIntensity();

					if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f))
					{
						temp->SetIntensity(intensity);
					}
				}
				if (activeEffect == 2)
				{
					ImGui::Text("Active Effect: Bloom Effect");

					BloomEffect* temp = (BloomEffect*)effects[activeEffect];
					float threshold = temp->GetThreshold();

					if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 1.0f))
					{
						temp->SetThreshold(threshold);
					}
				}
			}*/
			/*if (ImGui::CollapsingHeader("Assignment 1 Toggles"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}*/
			/*if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode");*/

			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion 

		// GL states
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		// Load some textures from files
		Texture2D::sptr diffuse = Texture2D::LoadFromFile("images/Stone_001_Diffuse.png");
		Texture2D::sptr diffuse2 = Texture2D::LoadFromFile("images/box.bmp");
		Texture2D::sptr specular = Texture2D::LoadFromFile("images/Stone_001_Specular.png");
		Texture2D::sptr reflectivity = Texture2D::LoadFromFile("images/box-reflections.bmp");
		LUT3D testCube("cubes/WarmCorrection.cube");

		// Lego Character Textures
		Texture2D::sptr legodiffuse1 = Texture2D::LoadFromFile("images/HappyBusinessman.png");
		Texture2D::sptr legospecular1 = Texture2D::LoadFromFile("images/HappyBusinessman_s.png");
		Texture2D::sptr legodiffuse2 = Texture2D::LoadFromFile("images/Magician.png");
		Texture2D::sptr legodiffuse3 = Texture2D::LoadFromFile("images/ShellLady.png");
		Texture2D::sptr legodiffuse4 = Texture2D::LoadFromFile("images/Wonderwoman.png");
		Texture2D::sptr legodiffuse5 = Texture2D::LoadFromFile("images/LegoHead.png");

		//Specular Textures
		Texture2D::sptr nospecular = Texture2D::LoadFromFile("images/nospec.png");
		Texture2D::sptr darkspecular = Texture2D::LoadFromFile("images/DarkGrey.png");
		Texture2D::sptr offwhitespecular = Texture2D::LoadFromFile("images/offwhite.png");

		//Lego Block Colour Textures
		Texture2D::sptr legoblockred = Texture2D::LoadFromFile("images/Red.png");
		Texture2D::sptr legoblockbrown = Texture2D::LoadFromFile("images/Brown.png");

		// Load the cube map
		//TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/sample.jpg");
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/space.jpg");

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion

		///////////////////////////////////// Scene Generation //////////////////////////////////////////////////
		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create a scene, and set it to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());

		// Create a material and set some properties for it
		ShaderMaterial::sptr material0 = ShaderMaterial::Create();
		material0->Shader = shader;
		material0->Set("s_Diffuse", diffuse);
		material0->Set("s_Diffuse2", diffuse2);
		material0->Set("s_Specular", specular);
		material0->Set("u_Shininess", 8.0f);
		material0->Set("u_TextureMix", 0.0f);

		// Lego Block Materials
		ShaderMaterial::sptr legoblock1 = ShaderMaterial::Create();
		legoblock1->Shader = shader;
		legoblock1->Set("s_Diffuse", legoblockred);
		legoblock1->Set("s_Specular", offwhitespecular);
		legoblock1->Set("u_Shininess", 8.0f);
		legoblock1->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr legoblock2 = ShaderMaterial::Create();
		legoblock2->Shader = shader;
		legoblock2->Set("s_Diffuse", legoblockbrown);
		legoblock2->Set("s_Specular", offwhitespecular);
		legoblock2->Set("u_Shininess", 8.0f);
		legoblock2->Set("u_TextureMix", 0.0f);

		// Lego Materials
		ShaderMaterial::sptr legocharacter1 = ShaderMaterial::Create();
		legocharacter1->Shader = shader;
		legocharacter1->Set("s_Diffuse", legodiffuse1);
		legocharacter1->Set("s_Specular", legospecular1);
		legocharacter1->Set("u_Shininess", 8.0f);
		legocharacter1->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr legocharacter2 = ShaderMaterial::Create();
		legocharacter2->Shader = shader;
		legocharacter2->Set("s_Diffuse", legodiffuse2);
		legocharacter2->Set("s_Specular", offwhitespecular);
		legocharacter2->Set("u_Shininess", 8.0f);
		legocharacter2->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr legocharacter3 = ShaderMaterial::Create();
		legocharacter3->Shader = shader;
		legocharacter3->Set("s_Diffuse", legodiffuse3);
		legocharacter3->Set("s_Specular", offwhitespecular);
		legocharacter3->Set("u_Shininess", 8.0f);
		legocharacter3->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr legocharacter4 = ShaderMaterial::Create();
		legocharacter4->Shader = shader;
		legocharacter4->Set("s_Diffuse", legodiffuse4);
		legocharacter4->Set("s_Specular", offwhitespecular);
		legocharacter4->Set("u_Shininess", 8.0f);
		legocharacter4->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr legocharacter5 = ShaderMaterial::Create();
		legocharacter5->Shader = shader;
		legocharacter5->Set("s_Diffuse", legodiffuse5);
		legocharacter5->Set("s_Specular", offwhitespecular);
		legocharacter5->Set("u_Shininess", 8.0f);
		legocharacter5->Set("u_TextureMix", 0.0f);

		// Load a second material for our reflective material!
		Shader::sptr reflectiveShader = Shader::Create();
		reflectiveShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflectiveShader->LoadShaderPartFromFile("shaders/frag_reflection.frag.glsl", GL_FRAGMENT_SHADER);
		reflectiveShader->Link();

		Shader::sptr reflective = Shader::Create();
		reflective->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflective->LoadShaderPartFromFile("shaders/frag_blinn_phong_reflection.glsl", GL_FRAGMENT_SHADER);
		reflective->Link();

		// 
		ShaderMaterial::sptr material1 = ShaderMaterial::Create();
		material1->Shader = reflective;
		material1->Set("s_Diffuse", diffuse);
		material1->Set("s_Diffuse2", diffuse2);
		material1->Set("s_Specular", specular);
		material1->Set("s_Reflectivity", reflectivity);
		material1->Set("s_Environment", environmentMap);
		material1->Set("u_LightPos", lightPos);
		material1->Set("u_LightCol", lightCol);
		material1->Set("u_AmbientLightStrength", lightAmbientPow);
		material1->Set("u_SpecularLightStrength", lightSpecularPow);
		material1->Set("u_AmbientCol", ambientCol);
		material1->Set("u_AmbientStrength", ambientPow);
		material1->Set("u_LightAttenuationConstant", 1.0f);
		material1->Set("u_LightAttenuationLinear", lightLinearFalloff);
		material1->Set("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		material1->Set("u_Shininess", 8.0f);
		material1->Set("u_TextureMix", 0.5f);
		material1->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));

		ShaderMaterial::sptr reflectiveMat = ShaderMaterial::Create();
		reflectiveMat->Shader = reflectiveShader;
		reflectiveMat->Set("s_Environment", environmentMap);
		reflectiveMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));

		GameObject LegoFloor = scene->CreateEntity("lego_floor");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoFloor.obj");
			LegoFloor.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legoblock1);
			LegoFloor.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
		}

		GameObject LegoTable = scene->CreateEntity("lego_table");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoTable.obj");
			LegoTable.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legoblock2);
			LegoTable.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
		}

		/*GameObject LegoPiece = scene->CreateEntity("lego_piece");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/legopiece.obj");
			LegoPiece.emplace<RendererComponent>().SetMesh(vao).SetMaterial(reflectiveMat);
			LegoPiece.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(LegoPiece);
			// Set up a path for the object to follow
			pathing->Points.push_back({ 0.0f, 0.0f, 3.0f });
			pathing->Points.push_back({ 0.0f, 0.0f, 5.0f });
			pathing->Speed = 2.0f;
		}*/

		GameObject LegoCharacter1 = scene->CreateEntity("lego_character");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoCharacter.obj");
			LegoCharacter1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legocharacter1);
			LegoCharacter1.get<Transform>().SetLocalPosition(0.0f, -3.0f, 0.0f);
		}

		GameObject LegoCharacter2 = scene->CreateEntity("lego_character1");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoCharacter.obj");
			LegoCharacter2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legocharacter2);
			LegoCharacter2.get<Transform>().SetLocalPosition(3.0f, 0.0f, 0.0f);
			LegoCharacter2.get<Transform>().SetLocalRotation(0, 0, 90);
		}

		GameObject LegoCharacter3 = scene->CreateEntity("lego_character2");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoCharacter.obj");
			LegoCharacter3.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legocharacter3);
			LegoCharacter3.get<Transform>().SetLocalPosition(-3.0f, 0.0f, 0.0f);
			LegoCharacter3.get<Transform>().SetLocalRotation(0, 0, -90);
		}

		GameObject LegoCharacter4 = scene->CreateEntity("lego_character3");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoCharacter.obj");
			LegoCharacter4.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legocharacter4);
			LegoCharacter4.get<Transform>().SetLocalPosition(0.0f, 3.0f, 0.0f);
			LegoCharacter4.get<Transform>().SetLocalRotation(0, 0, 180);
		}

		GameObject LegoCharacter5 = scene->CreateEntity("lego_character4");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/LegoHead.obj");
			LegoCharacter5.emplace<RendererComponent>().SetMesh(vao).SetMaterial(legocharacter5);
			LegoCharacter5.get<Transform>().SetLocalPosition(0.0f, 0.0f, 3.5f);
			BehaviourBinding::Bind<RotateObjectBehaviour>(LegoCharacter5);

			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(LegoCharacter5);
			// Set up a path for the object to follow
			pathing->Points.push_back({ 0.0f, 0.0f, 3.0f });
			pathing->Points.push_back({ 0.0f, 0.0f, 4.0f });
			pathing->Speed = 0.6f;
		}

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(3, 3, 3).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(3, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(90.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		Framebuffer* colourCorrection;
		GameObject colorCorrectionObject = scene->CreateEntity("Color Correction Effect");
		{
			colourCorrection = &colorCorrectionObject.emplace<Framebuffer>();
			colourCorrection->AddColorTarget(GL_RGBA8);
			colourCorrection->AddDepthTarget();
			colourCorrection->Init(width, height);
		}

		GameObject framebufferObject = scene->CreateEntity("Basic Buffer");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}

		GameObject greyscaleEffectObject = scene->CreateEntity("Greyscale Effect");
		{
			greyscaleEffect = &greyscaleEffectObject.emplace<GreyscaleEffect>();
			greyscaleEffect->Init(width, height);
		}
		effects.push_back(greyscaleEffect);

		GameObject SepiaEffectObject = scene->CreateEntity("Sepia Effect");
		{
			sepiaEffect = &SepiaEffectObject.emplace<SepiaEffect>();
			sepiaEffect->Init(width, height);
		}
		effects.push_back(sepiaEffect);

		GameObject BloomEffectObject = scene->CreateEntity("Bloom Effect");
		{
			bloomEffect = &BloomEffectObject.emplace<BloomEffect>();
			bloomEffect->Init(width, height);
		}
		effects.push_back(bloomEffect);

		#pragma endregion 
		//////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
		{
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		}
		////////////////////////////////////////////////////////////////////////////////////////


		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });

			/*controllables.push_back(obj2);
			controllables.push_back(obj3);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});*/
		}

		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			// Iterate over all the behaviour binding components
			scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
				// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
				for (const auto& behaviour : binding.Behaviours) {
					if (behaviour->Enabled) {
						behaviour->Update(entt::handle(scene->Registry(), entity));
					}
				}
			});

			// Clear the screen
			basicEffect->Clear();
			//colourCorrection->Clear();
			/*greyscaleEffect->Clear();
			sepiaEffect->Clear();*/

			for (int i = 0; i < effects.size(); i++)
			{
				effects[i]->Clear();
			}

			glClearColor(0.08f, 0.17f, 0.31f, 1.0f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Update all world matrices for this frame
			scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
				t.UpdateWorldMatrix();
			});
			
			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;
						
			// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
			renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
				// Sort by render layer first, higher numbers get drawn last
				if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
				if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

				// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
				if (l.Material->Shader < r.Material->Shader) return true;
				if (l.Material->Shader > r.Material->Shader) return false;

				// Sort by material pointer last (so we can minimize switching between materials)
				if (l.Material < r.Material) return true;
				if (l.Material > r.Material) return false;
				
				return false;
			});

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			basicEffect->BindBuffer(0);
			//colourCorrection->Bind();

			// Iterate over the render group components and draw them
			renderGroup.each( [&](entt::entity e, RendererComponent& renderer, Transform& transform) {
				// If the shader has changed, set up it's uniforms
				if (current != renderer.Material->Shader) {
					current = renderer.Material->Shader;
					current->Bind();
					BackendHandler::SetupShaderForFrame(current, view, projection);
				}
				// If the material has changed, apply it
				if (currentMat != renderer.Material) {
					currentMat = renderer.Material;
					currentMat->Apply();
				}
				// Render the mesh
				BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
			});

			/*colourCorrection->Unbind();

			colourCorrectionShader->Bind();
			colourCorrection->BindColorAsTexture(0, 0);
			testCube.bind(30);
			colourCorrection->DrawFullscreenQuad();
			testCube.unbind(30);
			colourCorrection->UnbindTexture(0);
			colourCorrectionShader->UnBind();*/

			basicEffect->UnbindBuffer();
			effects[activeEffect]->ApplyEffect(basicEffect);
			effects[activeEffect]->DrawToScreen();

			// Draw our ImGui content
			BackendHandler::RenderImGui();

			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;
		//Clean up the environment generator so we can release references
		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}