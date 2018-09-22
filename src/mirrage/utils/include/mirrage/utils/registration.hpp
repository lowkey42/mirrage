/** base for classes that know about the instances of another ****************
 *                                                                           *
 * Copyright (c) 2018 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/container_utils.hpp>
#include <mirrage/utils/log.hpp>

#include <utility>
#include <vector>

namespace mirrage::util {

	template <class CCTP, class ParentT>
	class Registered;

	template <class CCTP, class ChildT>
	class Registration {
	  public:
		Registration() = default;
		Registration(Registration&& rhs) noexcept : _children(std::move(rhs._children))
		{
			for(auto& c : _children) {
				c->_parent = this;
			}
		}
		Registration(const Registration& rhs) = delete;
		~Registration()
		{
			for(auto& c : _children) {
				c->_parent = nullptr;
			}
		}

		Registration& operator=(Registration&& rhs) noexcept
		{
			for(auto& c : _children) {
				c->_parent = nullptr;
			}

			_children = std::move(rhs._children);
			for(auto& c : _children) {
				c->_parent = this;
			}

			return *this;
		}
		Registration& operator=(const Registration& rhs) = delete;

	  protected:
		template <typename F>
		void foreach_child(F&& f)
		{
			for(auto c : _children) {
				f(*static_cast<ChildT*>(c));
			}
		}

	  private:
		friend class Registered<ChildT, CCTP>;

		std::vector<Registered<ChildT, CCTP>*> _children;

		static Registration* asRegistration(CCTP* self)
		{
			static_assert(std::is_base_of_v<Registration, CCTP>,
			              "The first template argument of Registration needs to be CRTP.");
			return static_cast<Registration*>(self);
		}
	};

	template <class CCTP, class ParentT>
	class Registered {
	  public:
		Registered() noexcept : _parent(nullptr) {}
		Registered(ParentT& p) noexcept : _parent(&p) { _registration()->_children.push_back(this); }
		Registered(Registered&& rhs) noexcept : _parent(rhs._parent)
		{
			rhs._parent = nullptr;

			if(_parent) {
				util::erase_fast(_registration()->_children, &rhs);
				_registration()->_children.push_back(this);
			}
		}
		Registered(const Registered& rhs) noexcept : _parent(rhs._parent)
		{
			if(_parent) {
				_registration()->_children.push_back(this);
			}
		}
		~Registered()
		{
			if(_parent) {
				// safe unless the casted this ptr is dereferenced
				util::erase_fast(_registration()->_children, this);
			}
		}

		Registered& operator=(Registered&& rhs) noexcept
		{
			if(_parent) {
				util::erase_fast(_registration()->_children, this);
			}

			_parent     = rhs._parent;
			rhs._parent = nullptr;
			if(_parent) {
				util::erase_fast(_registration()->_children, &rhs);
				_registration()->_children.push_back(this);
			}

			return *this;
		}
		Registered& operator=(const Registered& rhs) noexcept
		{
			if(_parent) {
				util::erase_fast(_registration()->_children, this);
			}

			_parent = rhs._parent;
			if(_parent) {
				_registration()->_children.push_back(this);
			}

			return *this;
		}

	  protected:
		auto parent() noexcept -> auto&
		{
			MIRRAGE_INVARIANT(_parent, "Deref nullptr");
			return *_parent;
		}

	  private:
		friend class Registration<ParentT, CCTP>;
		ParentT* _parent;

		auto _registration() { return Registration<ParentT, CCTP>::asRegistration(_parent); }
	};

} // namespace mirrage::util
