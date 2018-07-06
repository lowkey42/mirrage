#include <mirrage/ecs/serializer.hpp>

#include <mirrage/asset/asset_manager.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/utils/template_utils.hpp>

#include <sf2/sf2.hpp>

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace mirrage::asset;

namespace mirrage::ecs {

	namespace {
		class Blueprint;

		const std::string import_key = "$import";
		void              apply(const Blueprint& b, Entity_facet e);


		class Blueprint {
		  public:
			Blueprint(std::string id, std::string content, asset::Asset_manager* asset_mgr);
			Blueprint(Blueprint&&) noexcept;
			Blueprint(const Blueprint&) = delete;
			~Blueprint() noexcept;
			Blueprint& operator=(Blueprint&&) noexcept;

			void detach(Entity_handle target) const;
			void on_reload();

			mutable std::vector<Entity_handle> users;
			mutable std::vector<Blueprint*>    children;
			std::string                        id;
			std::string                        content;
			asset::Ptr<Blueprint>              parent;
			asset::Asset_manager*              asset_mgr;
			mutable Entity_manager*            entity_manager = nullptr;
		};


		Blueprint::Blueprint(std::string id, std::string content, asset::Asset_manager* asset_mgr)
		  : id(std::move(id)), content(std::move(content)), asset_mgr(asset_mgr)
		{

			std::istringstream stream{this->content};
			auto               deserializer = sf2::JsonDeserializer{stream};
			deserializer.read_lambda([&](const auto& key) {
				if(key == import_key) {
					auto value = std::string{};
					deserializer.read_value(value);
					parent = asset_mgr->load<Blueprint>(
					        AID{"blueprint"_strid, value}); // TODO: could/should be async
					parent->children.push_back(this);
				} else {
					deserializer.skip_obj();
				}
				return true;
			});
		}
		Blueprint::Blueprint(Blueprint&& rhs) noexcept
		  : id(rhs.id)
		  , content(std::move(rhs.content))
		  , parent(std::move(rhs.parent))
		  , asset_mgr(rhs.asset_mgr)
		  , entity_manager(rhs.entity_manager)
		{

			if(parent) {
				util::erase_fast(parent->children, &rhs);
				parent->children.push_back(this);
			}
		}
		Blueprint::~Blueprint() noexcept
		{
			if(parent) {
				util::erase_fast(parent->children, this);
			}
			MIRRAGE_INVARIANT(children.empty(), "Blueprint children not deregistered");
		}

		Blueprint& Blueprint::operator=(Blueprint&& o) noexcept
		{
			// swap data but keep user-list
			id      = o.id;
			content = std::move(o.content);
			if(parent) {
				util::erase_fast(parent->children, this);
				parent.reset();
			}

			if(o.parent) {
				util::erase_fast(o.parent->children, &o);
				parent = std::move(o.parent);
				o.parent->children.push_back(this);
			}

			on_reload();

			return *this;
		}
		void Blueprint::on_reload()
		{
			for(auto&& c : children) {
				c->on_reload();
			}

			for(auto&& u : users) {
				auto entity = entity_manager->get(u);
				MIRRAGE_INVARIANT(entity.is_some(), "dead entity in blueprint.users");
				apply(*this, entity.get_or_throw());
			}
		}

		void Blueprint::detach(Entity_handle target) const { util::erase_fast(users, target); }
	} // namespace
} // namespace mirrage::ecs

namespace mirrage::asset {
	template <>
	struct Loader<ecs::Blueprint> {
		static auto load(istream in) -> ecs::Blueprint
		{
			return ecs::Blueprint(in.aid().str(), in.content(), &in.manager());
		}

		static void save(ostream, const ecs::Blueprint&) { MIRRAGE_FAIL("NOT IMPLEMENTED, YET!"); }
	};
} // namespace mirrage::asset

namespace mirrage::ecs {
	namespace {
		// Blueprint_component
		class Blueprint_component : public ecs::Component<Blueprint_component> {
		  public:
			static constexpr const char* name() { return "$Blueprint"; }

			friend void load_component(ecs::Deserializer& state, Blueprint_component&);
			friend void save_component(ecs::Serializer& state, const Blueprint_component&);

			Blueprint_component() = default;
			Blueprint_component(ecs::Entity_handle    owner,
			                    ecs::Entity_manager&  manager,
			                    asset::Ptr<Blueprint> blueprint = {}) noexcept
			  : Component(owner, manager), _manager(&manager), blueprint(std::move(blueprint))
			{
			}
			Blueprint_component(Blueprint_component&&) noexcept = default;
			Blueprint_component& operator=(Blueprint_component&&) = default;
			~Blueprint_component()
			{
				if(blueprint) {
					blueprint->detach(owner_handle());
					blueprint.reset();
				}
			}

			void set(asset::Ptr<Blueprint> blueprint)
			{
				if(this->blueprint) {
					this->blueprint->detach(owner_handle());
				}

				this->blueprint = std::move(blueprint);
			}

			auto& manager() { return *_manager; }

		  private:
			ecs::Entity_manager*  _manager;
			asset::Ptr<Blueprint> blueprint;
		};

		void load_component(ecs::Deserializer& state, Blueprint_component& comp)
		{
			std::string blueprintName;
			state.read_virtual(sf2::vmember("name", blueprintName));

			auto blueprint = state.assets.load<Blueprint>(
			        AID{"blueprint"_strid, blueprintName}); // TODO: could/should be async
			comp.set(blueprint);
			blueprint->users.push_back(comp.owner_handle());
			blueprint->entity_manager = &comp.manager();
			apply(*blueprint, comp.owner(comp.manager()));
		}

		void save_component(ecs::Serializer& state, const Blueprint_component& comp)
		{
			auto name = comp.blueprint.aid().name();
			state.write_virtual(sf2::vmember("name", name));
		}


		void apply(const Blueprint& b, Entity_facet e)
		{
			if(b.parent) {
				apply(*b.parent, e);
			}

			std::istringstream stream{b.content};
			auto deserializer = Deserializer{b.id, stream, e.manager(), *b.asset_mgr, e.manager().userdata()};

			auto handle = e.handle();
			deserializer.read_value(handle);
		}


		sf2::format::Error_handler create_error_handler(std::string source_name)
		{
			return [source_name =
			                std::move(source_name)](const std::string& msg, uint32_t row, uint32_t column) {
				LOG(plog::error) << "Error parsing JSON from " << source_name << " at " << row << ":"
				                 << column << ": " << msg;
			};
		}
	} // namespace


	Deserializer::Deserializer(const std::string&    source_name,
	                           std::istream&         stream,
	                           Entity_manager&       m,
	                           asset::Asset_manager& assets,
	                           util::any_ptr         userdata,
	                           Component_filter      filter)
	  : sf2::JsonDeserializer(sf2::format::Json_reader{stream, create_error_handler(source_name)},
	                          create_error_handler(source_name))
	  , manager(m)
	  , assets(assets)
	  , userdata(userdata)
	  , filter(filter)
	{
	}

	void init_serializer(Entity_manager& ecs) { ecs.register_component_type<Blueprint_component>(); }

	Component_type blueprint_comp_id = component_type_id<Blueprint_component>();


	void apply_blueprint(asset::Asset_manager& asset_mgr, Entity_facet e, const std::string& blueprint)
	{
		auto mb = asset_mgr.load_maybe<ecs::Blueprint>(asset::AID{"blueprint"_strid, blueprint});
		if(mb.is_nothing()) {
			LOG(plog::error) << "Failed to load blueprint \"" << blueprint << "\"";
			return;
		}
		auto b = mb.get_or_throw(); // TODO: could/should be async

		if(!e.has<Blueprint_component>())
			e.emplace<Blueprint_component>(b);
		else
			e.get<Blueprint_component>().get_or_throw().set(b);

		b->users.push_back(e.handle());

		apply(*b, e);
	}


	void load(sf2::JsonDeserializer& s, Entity_handle& e)
	{
		auto& ecs_deserializer = static_cast<Deserializer&>(s);

		if(!ecs_deserializer.manager.validate(e)) {
			auto e_facet = ecs_deserializer.manager.emplace();
			e            = e_facet.handle();
		}

		s.read_lambda([&](const auto& key) {
			if(import_key == key) {
				auto value = std::string{};
				s.read_value(value);
				return true;
			}

			auto comp_type_mb = ecs_deserializer.manager.component_type_by_name(key);

			if(comp_type_mb.is_nothing()) {
				LOG(plog::debug) << "Skipped unknown component " << key;
				s.skip_obj();
				return true;
			}

			auto comp_type = comp_type_mb.get_or_throw();

			if(ecs_deserializer.filter && !ecs_deserializer.filter(comp_type)) {
				LOG(plog::debug) << "Skipped filtered component " << key;
				s.skip_obj();
				return true;
			}

			ecs_deserializer.manager.list(comp_type).restore(e, ecs_deserializer);

			return true;
		});
	}

	void save(sf2::JsonSerializer& s, const Entity_handle& e)
	{
		auto& ecs_s = static_cast<Serializer&>(s);

		s.write_lambda([&] {
			ecs_s.manager.list_all([&](auto& container) {
				if(!ecs_s.filter || ecs_s.filter(container.value_type())) {
					container.save(e, ecs_s);
				}
			});
		});
	}
} // namespace mirrage::ecs
