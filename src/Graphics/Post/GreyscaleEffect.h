#pragma once

#include "Graphics/Post/PostEffect.h"

class GreyscaleEffect : public PostEffect
{
public:
	//Initializes framebuffer
	//Overrides post effect Init
	void Init(unsigned width, unsigned height) override;

	//Applies the effect to this buffer
	//Passes the previous framebuffer with the texture to apply as a parameter
	void ApplyEffect(PostEffect* buffer) override;

	//Applies the effect to the screen
	//void DrawToScreen() override;

	//Getters
	float GetIntensity() const;

	//Setters
	void SetIntensity(float intensity);

private:
	float _intensity = 1.0f;
};