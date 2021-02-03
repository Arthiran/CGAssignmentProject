#pragma once

#include "Graphics/Post/PostEffect.h"

class BloomEffect : public PostEffect
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
	float GetThreshold() const;

	//Setters
	void SetThreshold(float threshold);
	void SetShaderUniform(std::string name, float value);

private:
	float _threshold = 0.25f;
};