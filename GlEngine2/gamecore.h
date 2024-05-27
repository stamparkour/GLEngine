#pragma once
#include "audio.h"
#include <vector>
#include <string>
#include <xaudio2.h>
#include "glmath.h"
#include <memory>
#define LayerMask_Main 1
#define Component_Requirements(class_name) size_t Size() override { return sizeof(class_name);} void AssignSelf(const Component& other) override { *this = (class_name&)other; }

namespace game::core {
	std::shared_ptr<char> readFile(const char* path, size_t* out_Size, bool isBinary);

	struct Transform {
		game::math::Vec3 position{};
		game::math::Vec3 scale{ 1,1,1 };
		game::math::Quat rotation = game::math::Quat_Identity;

		void Rotate(float x, float y, float z);
		game::math::Mat4 ToMatrix() const;
		game::math::Mat4 ToMatrixInverse() const;
		game::math::Vec3 forward() const;
		game::math::Vec3 back() const;
		game::math::Vec3 left() const;
		game::math::Vec3 right() const;
		game::math::Vec3 up() const;
		game::math::Vec3 down() const;
	};

	enum struct GameObjectState {
		Created,
		Initialized,
		Enabling,
		Enabled,
		Disabling,
		Disabled,
		Destroying,
	};
	enum struct ComponentState {
		Created,
		Active,
		Destroying,
	};

	struct Component;
	struct Scene;

	struct GameObject final {
		friend struct Component;
		friend struct Scene;
		friend struct GameManager;
	private:
		std::vector<Component*> components;
		GameObjectState state;
		game::math::Mat4 transformMatrix;
	public:
		game::core::Transform transform;
		std::string name;
		int layerMask;
		GameObject* parent;

		GameObject();
		GameObject(std::string name);
		GameObject(std::string name, int layerMask);
		GameObject(const GameObject& v);
		GameObject(GameObject&& v) noexcept;
		GameObject& operator =(const GameObject& v);
		GameObject& operator =(GameObject&& v) noexcept;
		~GameObject();
		friend void swap(GameObject& a, GameObject& b);
		void Destroy();

		bool exsists();
		std::vector<GameObject*> getChildren();
		game::math::Mat4 getTransform();
		game::math::Mat4 getPrevTransform() const;
		template <typename T>
		void AddComponent(T& component);
		template <typename T>
		void AddComponent(T&& component);
		template <typename T>
		T* GetComponent();
		template <typename T >
		bool RemoveComponent();
		void Disable();
		void Enable();
	protected:
		void Awake();
		void Update();
		void SyncUpdate();
		void FixedUpdate();
		void OnRender(int phase);
		void OnResize();
	};
	
	struct Component {
		friend struct GameObject;
		friend struct GameManager;
		friend void game::core::swap(game::core::GameObject& a, game::core::GameObject& b);
		ComponentState state { ComponentState::Created};
		Component() {}
	private:
		GameObject* gameObject{};
	protected:
		virtual void Awake() {}
		virtual void OnDisabled() {}
		virtual void OnEnabled() {}
		virtual void Start() {}
		virtual void OnDestroy() {}
		virtual void Update() {}
		virtual void FixedUpdate() {}
		virtual void OnRender(int phase) {}
		virtual void OnResize() {}
		virtual size_t Size() = 0;
		virtual void AssignSelf(const Component& other) = 0;
		GameObject* selfObject() const;
	}; 
	
	struct Scene final {
		friend struct GameManager;
		friend struct GameObject;
	private:
		std::vector<GameObject*> gameObjects{};
	public:
		Scene() {}
		Scene(Scene& other);
		Scene& operator =(Scene& other);
		GameObject& AddObject(const GameObject& object);
		void AddObject(GameObject&& object);

		GameObject* getGameObjectByName(std::string name);

		~Scene();
	};

	struct TimeManager final {
		friend struct GameManager;
	private:
		double deltaTime;
		double time;
		double fixedDeltaTime;
		double fixedTime;

		void NextTimestep(double time);
		void NextFixedTimestep(double time);
	public:
		static double Time(); 
		static double DeltaTime(); 
		static double FixedTime(); 
		static double FixedDeltaTime(); 
	};

	struct ControlsManager final {
		friend struct GameManager;
	private:
		char keyBits[256 / 8]{};
		char repeatKeyBits[256 / 8]{};
		void KeyDown(char virtualKey);
		void KeyUp(char virtualKey);
		void Update();
	public:
		static bool isKeyDown(char virtualKey);
		static bool isKeyPressed(char virtualKey);
		static bool isKeyUp(char virtualKey);
	};

	struct GameManager final {
	private:
		int screenX;
		int screenY;
		bool resized;
		static GameManager* current;
	public:
		TimeManager time;
		ControlsManager controls;
		game::audio::AudioManager audio;
		Scene* scene = {};

		static int ScreenX() { return current->screenX; }
		static int ScreenY() { return current->screenY;  }

		void Initialize();
		void Update(double time);
		void FixedUpdate(double time);
		void SyncUpdate();
		void Resize(int x, int y);
		void Render();
		void KeyDown(char virtualKey);
		void KeyUp(char virtualKey);

		static GameManager* Current() noexcept;
	};
}

namespace game::component {
	struct AudioSource final : game::core::Component {
		Component_Requirements(AudioSource)
	public:
		std::shared_ptr<game::audio::AudioClip> clip = nullptr;
		bool autoDelete = false;
		AudioSource() noexcept;
		AudioSource(std::shared_ptr<game::audio::AudioClip> clip, bool autoDelete = true, bool startPlaying = true) noexcept;
		void Update() override;
		bool isPlaying();

		static game::core::GameObject PlayClip(std::shared_ptr<game::audio::AudioClip> clip);
	};
}

template <typename T>
void game::core::GameObject::AddComponent(T& component) {
	game::core::Component* __dummy = static_cast<T*>(0);
	T* v = new T(component);
	v->gameObject = this;
	components.push_back(v);
}
template <typename T>
void game::core::GameObject::AddComponent(T&& component) {
	game::core::Component* __dummy = static_cast<T*>(0);
	T* v = new T(component);
	v->gameObject = this;
	components.push_back(v);
}
template <typename T>
T* game::core::GameObject::GetComponent() {
	game::core::Component* __dummy = static_cast<T*>(0);
	for (int i = 0; i < components.size(); i++) {
		if ((typeid (*components[i])) == typeid(T)) {
			return (T*)components[i];
		}
	}
	return NULL;
}
template <typename T>
bool game::core::GameObject::RemoveComponent() {
	game::core::Component* __dummy = static_cast<T*>(0);
	for (int i = 0; i < components.size(); i++) {
		if ((typeid (*components[i])) == typeid(T)) {
			components[i]->state = game::core::ComponentState::Destroying;
			return true;
		}
	}
	return false;
}