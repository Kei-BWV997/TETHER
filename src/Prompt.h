#pragma once

namespace TETHER::Prompt
{
	void Install();  // kDataLoaded
	bool IsAvailable();
	void Show(RE::Actor* a_target);  // no-op if already showing for this target
	void Hide();
}
