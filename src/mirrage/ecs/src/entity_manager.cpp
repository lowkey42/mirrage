#include <mirrage/ecs/entity_manager.hpp>

#include <mirrage/ecs/component.hpp>
#include <mirrage/ecs/serializer.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>

#include <mirrage/utils/log.hpp>
#include <mirrage/utils/string_utils.hpp>

#include <sf2/sf2.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>



namespace mirrage::ecs {

	Entity_manager::Entity_manager(asset::Asset_manager& assets, util::any_ptr ud)
	  : _assets(assets), _userdata(ud)
	{
		init_serializer(*this);
	}
	Entity_manager::~Entity_manager() { deinit_serializer(*this); }

	auto Entity_builder::create() -> Entity_facet
	{
		MIRRAGE_INVARIANT(_manager, "Called Entity_builder::create() on uninitialized/moved-from object.");

		_facet = _manager->emplace_empty();

		if(!_blueprint.empty()) {
			_manager->_queued_emplace.enqueue(*this);
		} else {
			apply();
		}

		return _facet;
	}

	void Entity_builder::apply()
	{
		MIRRAGE_INVARIANT(_facet, "Called Entity_builder::apply() without facet");

		if(!_blueprint.empty())
			apply_blueprint(_manager->_assets, _facet, _blueprint);

		else if(_position.is_some() || std::holds_alternative<std::monostate>(_rotation))
			_facet.emplace<components::Transform_comp>();

		_facet.process<components::Transform_comp>([&](auto& transform) {
			if(_position.is_some())
				transform.position = _position.get_or_throw();

			if(_scale.is_some())
				transform.scale = _scale.get_or_throw();

			if(auto r = std::get_if<glm::vec3>(&_rotation)) {
				if(_look_at)
					transform.look_at(*r);
				else
					transform.direction(*r);
			} else if(auto r = std::get_if<glm::quat>(&_rotation)) {
				transform.orientation = *r;
			}
		});

		if(_post_create)
			_post_create(_facet);
	}

	auto Entity_manager::emplace_empty() -> Entity_facet { return {*this, _handles.get_new()}; }

	auto Entity_manager::get(Entity_handle entity) -> util::maybe<Entity_facet>
	{
		if(validate(entity))
			return Entity_facet{*this, entity};

		return util::nothing;
	}

	void Entity_manager::erase(Entity_handle entity)
	{
		if(validate(entity)) {
			_queue_erase.enqueue(entity);
		} else {
			LOG(plog::error) << "Double-Deletion of entity " << entity_name(entity);
		}
	}

	void Entity_manager::process_queued_actions()
	{
		MIRRAGE_INVARIANT(_local_queue_erase.empty(),
		                  "Someone's been sleeping in my bed! (_local_queue_erase is dirty)");

		{
			std::array<Entity_builder, 8> emplace_buffer;
			do {
				std::size_t count =
				        _queued_emplace.try_dequeue_bulk(emplace_buffer.data(), emplace_buffer.size());
				if(count > 0) {
					for(std::size_t i = 0; i < count; i++) {
						emplace_buffer[i].apply();
					}

				} else {
					break;
				}
			} while(true);
		}

		{
			std::array<Entity_handle, 32> erase_buffer;
			do {
				std::size_t count = _queue_erase.try_dequeue_bulk(erase_buffer.data(), erase_buffer.size());

				if(count > 0) {
					for(std::size_t i = 0; i < count; i++) {
						const auto h = erase_buffer[i];
						if(!validate(h))
							continue;

						_local_queue_erase.emplace_back(h);

						for(auto& component : _components) {
							if(component)
								component->erase(h);
						}
					}
				} else {
					break;
				}
			} while(true);
		}

		for(auto& component : _components) {
			if(component)
				component->process_queued_actions();
		}

		for(auto h : _local_queue_erase) {
			_handles.free(h);
		}
		_local_queue_erase.clear();
	}

	void Entity_manager::clear()
	{
		for(auto& component : _components)
			if(component)
				component->clear();

		_handles.clear();
		_queue_erase    = Erase_queue{};
		_queued_emplace = Emplace_queue{};
	}


	auto Entity_manager::write_one(Entity_handle source) -> ETO
	{
		std::stringstream stream;
		auto              serializer = Serializer{stream, *this, _assets, _userdata, {}};
		serializer.write(source);
		stream.flush();

		return stream.str();
	}
	auto Entity_manager::read_one(ETO data, Entity_handle target) -> Entity_facet
	{
		if(!validate(target)) {
			target = emplace_empty();
		}

		std::istringstream stream{data};
		auto deserializer = Deserializer{"$EntityRestore", stream, *this, _assets, _userdata, {}};
		deserializer.read(target);
		return {*this, target};
	}

	void Entity_manager::write(std::ostream& stream, Component_filter filter)
	{

		auto serializer = Serializer{stream, *this, _assets, _userdata, filter};
		auto entities   = Entity_collection_facet{*this};
		serializer.write_virtual(sf2::vmember("entities", entities));

		stream.flush();
	}

	void Entity_manager::write(std::ostream&                     stream,
	                           const std::vector<Entity_handle>& entities,
	                           Component_filter                  filter)
	{

		auto serializer = Serializer{stream, *this, _assets, _userdata, filter};
		serializer.write_virtual(sf2::vmember("entities", entities));

		stream.flush();
	}

	void Entity_manager::read(std::istream& stream, bool clear, Component_filter filter)
	{
		if(clear) {
			this->clear();
		}

		auto deserializer = Deserializer{"$EntityDump", stream, *this, _assets, _userdata, filter};
		auto entities     = Entity_collection_facet{*this};
		deserializer.read_virtual(sf2::vmember("entities", entities));
	}


	Entity_collection_facet::Entity_collection_facet(Entity_manager& manager) : _manager(manager) {}

	Entity_iterator Entity_collection_facet::begin() const
	{
		return Entity_iterator(_manager._handles, _manager._handles.next());
	}
	Entity_iterator Entity_collection_facet::end() const
	{
		return Entity_iterator(_manager._handles, invalid_entity);
	}
	void Entity_collection_facet::clear() { _manager.clear(); }
} // namespace mirrage::ecs
