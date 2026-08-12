#pragma once

namespace TETHER
{
	class InputHandler final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		static InputHandler* GetSingleton();
		static void          Register();

		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
			RE::BSTEventSource<RE::InputEvent*>*) override;

	private:
		InputHandler() = default;
		InputHandler(const InputHandler&) = delete;
		InputHandler(InputHandler&&) = delete;
		InputHandler& operator=(const InputHandler&) = delete;
		InputHandler& operator=(InputHandler&&) = delete;
	};
}
