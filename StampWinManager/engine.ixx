
export module engine;

import std;

import gamerender;
import swm;
import math;

enum struct State {
	Creating,
	Running,
	Destroying
};

enum struct EnableState {
	Enabling,
	Enabled,
	Disabling,
	Disabled
};

export namespace engine {
	//uimanager
	//	--main uielement: screen
	//	--camera
	//		--varrying width or height
	//	--render function
	
	//dictionary of name, id
	//dictionary of id, element
	//virtual: uielement
	//	--id
	//	--child elements
	//	--disable/enable (pass to children)
	//	--virtual onclick
	//	--virtual onmouserelease
	//	--virtual render function
	//  --onenabled/ondisabled
	//	--transform
	//	--up/right/down/left id for joystick/arrow key identification.

	//alignment: 
	// +--+--+
	// |  |  |
	// +--+--+ 
	// |  |  |
	// +--+--+
	// stretch/fixed width and height
	//x1, x2
	//y1, y2
	//
	//lua construction
	
	class GameObject;
	class Component;
	class Scene;

	class Component {
		friend class GameObject;
		State state = State::Creating;
		EnableState enable = EnableState::Enabling;
		std::shared_ptr<GameObject> gameObject;
	protected:
		//sync safe
		virtual void Start() = 0;
		//unsafe
		virtual void Update() = 0;
		//gl context safe
		virtual void Render(int phase) = 0;
		//sync safe
		virtual void SyncUpdate() = 0;
		//sync safe
		virtual void OnEnable() = 0;
		//sync safe
		virtual void OnDisable() = 0;
		//sync safe
		virtual void OnDestroy() = 0;
		Component() {}
	public:
		std::shared_ptr<GameObject> GameObject() {
			return gameObject;
		}
		void Enable() {
			if (enable == EnableState::Disabled || enable == EnableState::Disabling) {
				enable = EnableState::Enabling;
			}
		}
		void Disable() {
			if (enable == EnableState::Enabled || enable == EnableState::Enabling) {
				enable = EnableState::Disabling;
			}
		}
		bool IsEnabled() {
			return enable == EnableState::Enabled;
		}
		virtual ~Component() {}
	};

	class GameObject final : public std::enable_shared_from_this<GameObject> {
		friend class Scene;
		bool destroying = false;
		EnableState enable = EnableState::Enabling;
		std::vector<std::shared_ptr<GameObject>> children{};
		std::shared_ptr<GameObject> parent{};
		std::vector<std::unique_ptr<Component>> components{};
		void Update() {
			
			for (int i = 0; i < components.size(); i++) {
				if (components[i]->IsEnabled() && components[i]->state == State::Running) {
					components[i]->Update();
				}
			}
		}
		void Render(int phase) {
			for (int i = 0; i < components.size(); i++) {
				if (components[i]->IsEnabled() && components[i]->state == State::Running) {
					components[i]->Render(phase);
				}
			}
		}
		void SyncUpdate() {
			for (int i = 0; i < components.size(); i++) {
				if ((components[i]->enable == EnableState::Enabled || components[i]->enable == EnableState::Enabling) && components[i]->state == State::Creating) {
					components[i]->state = State::Running;
					components[i]->Start();
				}
			}
			for (int i = 0; i < components.size(); i++) {
				if (components[i]->enable == EnableState::Enabling && components[i]->state == State::Running) {
					components[i]->enable = EnableState::Enabled;
					components[i]->OnEnable();
				}
				else if (components[i]->enable == EnableState::Disabling && components[i]->state == State::Running) {
					components[i]->enable = EnableState::Disabled;
					components[i]->OnDisable();
				}

				if (components[i]->enable == EnableState::Enabled && components[i]->state == State::Running) {
					components[i]->SyncUpdate();
				}
			}
			for (int i = 0; i < components.size(); i++) {
				if (components[i]->state == State::Destroying) {
					components[i]->OnDestroy();
					components.erase(components.begin() + i);
					i--;
				}
			}
		}
		void OnDestroy() {
			if (components.size() > 0) {
				for (int i = 0; i < components.size(); i++) {
					components[i]->OnDestroy();
				}
				components.resize(0);
			}
			SetParent();
		}
	protected:
		GameObject() {}
	public:
		//only change in sync update, onenable, ondisable.
		std::string name;
		render::Transform transform;
		template<typename T, typename... P>
		std::unique_ptr<T> AddComponent(P... param) {
			std::unique_ptr<T> component{ new T(param...) };
			component->gameObject = shared_from_this();
			components.push_back(component);
		}
		template<typename T>
		bool RemoveComponent() {
			for (int i = 0; i < components.size(); i++) {
				if (typeid(T) == typeid(components[i])) {
					components.erase(components.begin() + i);
					return true;
				}
			}
			return false;
		}
		template<typename T>
		std::unique_ptr<T> GetComponent() {
			for (auto& comp : components) {
				if (typeid(T) == typeid(comp)) {
					return (std::unique_ptr<T>)comp;
				}
			}
			return nullptr;
		}
		void SetParent(std::shared_ptr<GameObject> parent = nullptr) {
			if (!this->parent.get()) {
				int index = -1;
				for (int i = 0; i < this->parent->children.size(); i++) {
					if (this->parent->children[i].get() == this) {
						index = i;
						break;
					}
				}
				this->parent->children.erase(this->parent->children.begin() + index);
			}
			this->parent = parent;
			if (this->parent) {
				this->parent->children.push_back(shared_from_this());
			}
		}
		const std::vector<std::shared_ptr<GameObject>> GetChildren() {
			return children;
		}
		void Destroy() {
			destroying = true;
			if (children.size() > 0) {
				for (int i = 0; i < children.size(); i++) {
					children[i]->Destroy();
					children[i]->parent = 0;
				}
				children.resize(0);
			}
		}
		~GameObject() {
		}
	};

	class Scene final : swm::SceneBase {
		static inline Scene* currentScene;
		int maxPhases = 9;
		std::vector<std::shared_ptr<GameObject>> objects{};
		virtual void Start() {
			currentScene = this;
			Initialize();
		}
		virtual void End() {
			if (currentScene == this) currentScene = 0;
			for (int i = 0; i < objects.size(); i++) {
				objects[i]->OnDestroy();
			}
			objects.resize(0);
		}
		virtual void Update() {
			for (int i = 0; i < objects.size(); i++) {
				objects[i]->Update();
			}
			for (int i = 0; i < objects.size(); i++) {
				if (objects[i]->destroying) {
					objects[i]->OnDestroy();
					objects.erase(objects.begin() + i);
					i--;
				}
			}
		}
		virtual void SyncUpdate() {
			for (int i = 0; i < objects.size(); i++) {
				objects[i]->SyncUpdate();
			}
		}
		virtual void Render() {
			for (int p = 0; p < maxPhases; p++) {
				PreRender(p);
				for (int i = 0; i < objects.size(); i++) {
					objects[i]->Render(p);
				}
				PostRender(p);
			}
		}
		virtual void Resize(long width, long height) {}
	public:
		Scene() {}
		virtual void Initialize() = 0;
		virtual void PreRender(int phase) = 0;
		virtual void PostRender(int phase) = 0;
		static std::shared_ptr<GameObject> CreateObject(std::string name, std::shared_ptr<GameObject> parent = nullptr) {
			std::shared_ptr<GameObject> obj(new GameObject());
			obj->name = name;
			obj->SetParent(parent);
			currentScene->objects.push_back(obj);
			return obj;
		}
		static std::shared_ptr<GameObject> GetObject(std::string name) {
			for (int i = 0; i < currentScene->objects.size(); i++) {
				if (currentScene->objects[i] && name == currentScene->objects[i]->name) {
					return currentScene->objects[i];
				}
			}
		}
	};
}