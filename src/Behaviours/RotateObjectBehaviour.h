#pragma once
#include "IBehaviour.h"


class RotateObjectBehaviour : public IBehaviour
{
public:
	RotateObjectBehaviour() = default;
	~RotateObjectBehaviour() = default;

	void Update(entt::handle entity) override;
};
