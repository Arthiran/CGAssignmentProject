#include "RotateObjectBehaviour.h"
#include "Transform.h"
#include "Timing.h"

#include "GLFW/glfw3.h"

void RotateObjectBehaviour::Update(entt::handle entity)
{
	float dt = Timing::Instance().DeltaTime;
	Transform& transform = entity.get<Transform>();

	transform.RotateLocal(0, 0, 1.0f);
}
