#pragma once

#include <mirrage/ecs/component.hpp>

#include <mirrage/utils/sf2_glm.hpp>

#include <glm/vec3.hpp>


namespace mirrage::audio {

	class Listener_comp : public ecs::Component<Listener_comp> {
	  public:
		static constexpr const char* name() { return "Listener"; }
		using Component::Component;

		float     priority = 0.f;
		glm::vec3 offset   = {0, 0, 0};
	};

	sf2_structDef(Listener_comp, priority, offset);

} // namespace mirrage::audio
