/** event-types caused by user-input *****************************************
 *                                                                           *
 * Copyright (c) 2016 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/input/types.hpp>

#include <glm/vec2.hpp>
#include <memory>


namespace mirrage::input {

	struct Char_input {
		std::string character;
	};

	struct File_dropped {
		std::string path;
	};

	struct Source_added {
		Input_source src;
	};
	struct Source_removed {
		Input_source src;
	};


	// mapped inputs
	struct Once_action {
		Action_id    id;
		Input_source src;
	};
	inline bool operator==(const Once_action& lhs, const Once_action& rhs) {
		return lhs.id == rhs.id && lhs.src == rhs.src;
	}

	struct Continuous_action {
		Action_id    id;
		Input_source src;
		bool         begin; //< true=begin, false=end
	};

	struct Range_action {
		Action_id    id;
		Input_source src;
		glm::vec2    rel;
		glm::vec2    abs;
	};

	struct Force_feedback {
		Input_source src;
		float        force; //< 0-1
	};
}
